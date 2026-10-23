#include "launcher_controller.h"
#include "launcher.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QtConcurrent>

namespace {
QVariantMap entry(const QString &label, const QString &value) {
    return {{"label", label}, {"value", value}};
}
}

LauncherController::LauncherController(QObject *parent) : QObject(parent) {
    QSettings settings("PCLQt", "Launcher");
    playerName_ = settings.value("name", "Player").toString();
    QString error;
    if (!store_.load(error)) fail("实例登记加载失败，已禁用写入：" + error);
    else { storeReady_ = true; refreshInstances(); }
    connect(&game_, &QProcess::started, this, [this] {
        status_ = selectedName() + " 已启动（离线模式）";
        appendLog(status_);
        emit stateChanged();
    });
    connect(&game_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        fail("游戏进程错误：" + game_.errorString());
    });
    connect(&game_, &QProcess::readyRead, this, [this] { appendLog(QString::fromLocal8Bit(game_.readAll())); });
    connect(&game_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus exit) {
        appendLog(QString("进程退出：%1 (%2)").arg(code).arg(exit == QProcess::NormalExit ? "normal" : "crashed"));
        emit stateChanged();
        if (error_.isEmpty()) checkSelected();
    });
    connect(&versionsWatcher_, &QFutureWatcher<QPair<QStringList, QString>>::finished, this, [this] {
        const auto result = versionsWatcher_.result();
        versions_.clear();
        for (const auto &id : result.first) versions_.append(entry(id, id));
        loadingVersions_ = false;
        appendLog(result.second);
        emit versionsChanged();
        emit stateChanged();
    });
    connect(&javaWatcher_, &QFutureWatcher<QList<JavaRuntime>>::finished, this, [this] {
        auto discovered = javaWatcher_.result();
        for (const auto &runtime : runtimes_) {
            bool found = false;
            for (const auto &candidate : discovered) if (candidate.path == runtime.path) found = true;
            if (!found) discovered.append(runtime);
        }
        runtimes_ = discovered;
        scanning_ = false;
        refreshJava();
        recommendJava();
        appendLog(QString("检测到 %1 个 Java 运行时").arg(runtimes_.size()));
        emit stateChanged();
    });
    loadVersions(true);
    scanJava();
    if (selected()) checkSelected();
}

LauncherController::~LauncherController() {
    ++generation_;
    if (worker_) { worker_->cancel(); worker_->wait(); delete worker_; worker_ = nullptr; }
    if (game_.state() != QProcess::NotRunning) { game_.kill(); game_.waitForFinished(5000); }
    versionsWatcher_.waitForFinished();
    javaWatcher_.waitForFinished();
    for (auto *watcher : checks_) { watcher->waitForFinished(); delete watcher; }
}

QVariantList LauncherController::instances() const {
    QVariantList list;
    for (const auto &item : store_.instances())
        list.append(QVariantMap{{"id", item.id}, {"name", item.name}, {"version", item.versionId},
                                {"directory", item.directory}, {"pending", item.pending}});
    return list;
}
QString LauncherController::selectedName() const { return selected() ? selected()->name : QString(); }
QString LauncherController::selectedVersion() const { return selected() ? selected()->versionId : QString(); }
QString LauncherController::selectedDirectory() const { return selected() ? selected()->directory : QString(); }
QString LauncherController::javaPath() const { return selected() ? selected()->javaPath : QString(); }
bool LauncherController::javaCompatible() const {
    if (!requiredJava() || javaPath().isEmpty()) return false;
    for (const auto &runtime : runtimes_)
        if (runtime.path == javaPath()) return runtime.compatible(requiredJava());
    return false;
}
void LauncherController::setPlayerName(const QString &name) {
    if (name == playerName_) return;
    playerName_ = name;
    QSettings("PCLQt", "Launcher").setValue("name", name);
    emit stateChanged();
}
void LauncherController::appendLog(const QString &message) {
    if (message.isEmpty()) return;
    log_ += message + (message.endsWith('\n') ? "" : "\n");
    if (log_.size() > 60000) log_ = log_.right(50000);
    emit stateChanged();
}
void LauncherController::fail(const QString &message) {
    error_ = message;
    status_ = message;
    appendLog("错误：" + message);
    emit stateChanged();
}
void LauncherController::refreshInstances() { emit instancesChanged(); emit stateChanged(); }
void LauncherController::refreshJava() {
    javaRuntimes_.clear();
    const int required = current_.plan.javaMajor;
    for (const auto &runtime : runtimes_) javaRuntimes_.append(entry(runtime.label(required), runtime.path));
    emit javaChanged();
}
void LauncherController::recommendJava() {
    const auto *item = selected();
    if (!storeReady_ || !item || !item->javaPath.isEmpty() || !requiredJava()) return;
    for (const auto &runtime : runtimes_) {
        if (!runtime.compatible(requiredJava())) continue;
        QString error;
        if (!store_.setJavaPath(item->id, runtime.path, error)) fail(error);
        else refreshInstances();
        return;
    }
}
void LauncherController::loadVersions(bool online) {
    if (loadingVersions_) return;
    loadingVersions_ = true;
    emit stateChanged();
    const QString root = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("catalog");
    versionsWatcher_.setFuture(QtConcurrent::run([root, online] {
        QString notice;
        return qMakePair(releaseVersions(root, online, notice), notice);
    }));
}
void LauncherController::refreshVersions() { loadVersions(true); }
void LauncherController::selectInstance(const QString &id) {
    if (!storeReady_ || busy() || id == store_.selectedId()) return;
    QString error;
    if (!store_.select(id, error)) { fail(error); return; }
    ++generation_;
    current_ = {};
    checking_ = false;
    error_.clear();
    status_ = id.isEmpty() ? "请选择或创建实例" : "准备检查实例";
    refreshInstances();
    refreshJava();
    if (!id.isEmpty()) checkSelected();
}
void LauncherController::createInstance(const QString &name, const QString &version, const QString &directory) {
    if (!storeReady_ || busy() || loadingVersions_) return;
    const QString path = directory.trimmed();
    if (path.isEmpty() || !QDir::isAbsolutePath(path)) { fail("请输入完整的绝对目标目录"); return; }
    if (version.isEmpty()) { fail("请选择正式版本"); return; }
    QString id, error;
    if (!store_.add(name.trimmed(), version, QDir::cleanPath(path), {}, id, error)) { fail(error); return; }
    if (!store_.select(id, error)) { fail(error); return; }
    ++generation_;
    checking_ = false;
    current_ = {};
    error_.clear();
    refreshInstances();
    refreshJava();
    installSelected();
}
void LauncherController::renameSelected(const QString &name) {
    if (!selected() || busy()) return;
    QString error;
    if (!store_.rename(selectedId(), name.trimmed(), error)) { fail(error); return; }
    refreshInstances();
}
void LauncherController::checkSelected() {
    if (busy()) return;
    const auto *item = selected();
    error_.clear();
    ++generation_;
    const quint64 token = generation_;
    current_ = {};
    checking_ = item && !busy();
    progress_ = 0;
    progressTotal_ = 0;
    status_ = checking_ ? "正在检查 " + item->name : item ? "实例正在运行" : "请选择或创建实例";
    emit stateChanged();
    if (!checking_) return;
    const QString root = item->directory, version = item->versionId;
    auto *watcher = new QFutureWatcher<VersionStatus>(this);
    checks_.append(watcher);
    connect(watcher, &QFutureWatcher<VersionStatus>::finished, this, [this, watcher, token] {
        const auto result = watcher->result();
        checks_.removeAll(watcher);
        watcher->deleteLater();
        if (token != generation_ || busy()) return;
        current_ = result;
        checking_ = false;
        progress_ = result.files.checked;
        progressTotal_ = qMax(1, result.files.checked);
        status_ = selectedName() + " · " + result.message;
        refreshJava();
        recommendJava();
        emit stateChanged();
    });
    watcher->setFuture(QtConcurrent::run([this, root, version, token] {
        return inspectVersion(root, version, false, [this, token](int done, int total) {
            if (done == 1 || done == total || done % 25 == 0)
                QMetaObject::invokeMethod(this, [this, token, done, total] {
                    if (token == generation_) { progress_ = done; progressTotal_ = total; emit stateChanged(); }
                }, Qt::QueuedConnection);
        });
    }));
}
void LauncherController::installSelected() {
    const auto *item = selected();
    if (!storeReady_ || !item || busy() || checking_) return;
    ++generation_;
    current_ = {};
    error_.clear();
    const QString id = item->id;
    worker_ = new Installer(item->directory, item->versionId);
    progress_ = 0;
    progressTotal_ = 0;
    status_ = "正在安装 / 修复 " + item->name;
    emit stateChanged();
    auto *worker = worker_;
    connect(worker, &Installer::progress, this, [this, worker](int done, int total, const QString &detail) {
        if (worker != worker_ || !error_.isEmpty()) return;
        progress_ = done; progressTotal_ = total;
        status_ = QString("%1 / %2  %3").arg(done).arg(total).arg(detail);
        emit stateChanged();
    });
    connect(worker, &Installer::completed, this, [this, worker, id](bool ok, const QString &message) {
        if (worker != worker_) return;
        appendLog(message);
        if (!ok) fail(message);
        if (ok) {
            QString error;
            if (!store_.setPending(id, false, error)) fail(error);
            else refreshInstances();
        }
    });
    connect(worker, &QThread::finished, this, [this, worker, id] {
        if (worker != worker_) return;
        worker_ = nullptr;
        worker->deleteLater();
        emit stateChanged();
        if (selectedId() == id && error_.isEmpty()) checkSelected();
    });
    worker_->start();
}
void LauncherController::scanJava() {
    if (scanning_) return;
    scanning_ = true;
    emit stateChanged();
    const QString preferred = javaPath();
    javaWatcher_.setFuture(QtConcurrent::run([preferred] { return discoverJava(preferred); }));
}
void LauncherController::chooseJava(const QString &path) {
    if (!storeReady_ || !selected() || busy() || path.isEmpty()) return;
    const auto runtime = inspectJava(path);
    if (!runtime.major) { fail("无法识别该 Java 可执行文件：" + path); return; }
    QString error;
    if (!store_.setJavaPath(selectedId(), runtime.path, error)) { fail(error); return; }
    bool found = false;
    for (const auto &item : runtimes_) if (item.path == runtime.path) found = true;
    if (!found) runtimes_.append(runtime);
    refreshJava();
    refreshInstances();
    if (current_.plan.javaMajor && !runtime.compatible(current_.plan.javaMajor))
        fail(QString("此版本需要 Java %1，当前选择 Java %2").arg(current_.plan.javaMajor).arg(runtime.major));
}
void LauncherController::launchSelected() {
    const auto *item = selected();
    if (!item || busy() || checking_ || !current_.installed) return;
    const QString player = playerName_.trimmed();
    if (!QRegularExpression("^[A-Za-z0-9_]{3,16}$").match(player).hasMatch()) {
        fail("离线名字须为 3–16 位字母、数字或下划线"); return;
    }
    const auto java = inspectJava(item->javaPath);
    if (!java.compatible(current_.plan.javaMajor)) {
        fail(QString("此版本需要 Java %1，当前 Java %2").arg(current_.plan.javaMajor).arg(java.major)); return;
    }
    QStringList arguments;
    QString error;
    if (!launchArguments(current_.plan, player, arguments, error)) { fail(error); return; }
    error_.clear();
    game_.setWorkingDirectory(item->directory);
    game_.setProcessChannelMode(QProcess::MergedChannels);
    status_ = "正在启动 " + item->name;
    game_.start(java.path, arguments);
    emit stateChanged();
}
void LauncherController::cancel() {
    if (worker_) { worker_->cancel(); status_ = "正在取消安装…"; emit stateChanged(); }
    else if (game_.state() != QProcess::NotRunning) game_.kill();
}
