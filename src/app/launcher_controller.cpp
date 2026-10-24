#include "launcher_controller.h"
#include "install/launcher.h"

#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QVersionNumber>
#include <QScopeGuard>
#include <QtConcurrent>
#include <algorithm>

namespace {
QVariantMap entry(const QString &label, const QString &value) { return {{"label", label}, {"value", value}}; }
}
LauncherController::LauncherController(QObject *parent) : QObject(parent) {
    QString error;
    if (!store_.load(error)) fail("实例登记加载失败，已禁用写入：" + error);
    else { storeReady_ = true; refreshInstances(); }
    connect(&game_, &QProcess::started, this, [this] {
        status_ = gameName_ + " · 游戏进程已启动（离线模式）";
        appendLog(status_);
        emit stateChanged();
    });
    connect(&game_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (!gameStopped_ && error != QProcess::Crashed) fail("启动进程失败：" + game_.errorString());
    });
    connect(&game_, &QProcess::readyRead, this, [this] { appendLog(QString::fromLocal8Bit(game_.readAll())); });
    connect(&game_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus exit) {
        appendLog(QString("进程退出：%1 (%2)").arg(code).arg(exit == QProcess::NormalExit ? "normal" : "crashed"));
        if (gameStopped_) status_ = "已结束游戏";
        else if (code || exit != QProcess::NormalExit) { fail(QString("游戏异常退出：%1，请查看日志").arg(code)); return; }
        else status_ = "游戏正常退出";
        emit stateChanged();
    });
    connect(&versionsWatcher_, &QFutureWatcher<QPair<QStringList, QString>>::finished, this, [this] {
        const auto result = versionsWatcher_.result();
        versions_.clear();
        for (const auto &id : result.first) versions_.append(entry(id, id));
        loadingVersions_ = false;
        appendLog(result.second);
        emit versionsChanged(); emit stateChanged();
    });
    connect(&javaWatcher_, &QFutureWatcher<QList<JavaRuntime>>::finished, this, [this] {
        scanning_ = false;
        if (*scanCancel_ || busy()) { manualJava_.clear(); emit stateChanged(); return; }
        const bool manual = !manualJava_.isEmpty();
        if (!manual) runtimes_ = javaWatcher_.result();
        else for (const auto &runtime : javaWatcher_.result()) {
            runtimes_.erase(std::remove_if(runtimes_.begin(), runtimes_.end(), [&runtime](const auto &item) { return item.path == runtime.path; }), runtimes_.end());
            runtimes_.append(runtime);
        }
        if (!manualJava_.isEmpty()) {
            const auto result = javaWatcher_.result();
            QString error;
            if (result.isEmpty()) fail("无法识别该 Java 可执行文件：" + manualJava_);
            else if (!store_.setJavaPath(selectedId(), result.first().path, error)) fail(error);
            else { error_.clear(); status_ = "已选择 Java " + result.first().version; }
            manualJava_.clear();
        }
        refreshJava();
        if (!manual) recommendJava();
        refreshInstances();
        appendLog(QString("检测到 %1 个 Java 运行时").arg(runtimes_.size()));
    });
    loadVersions(true); scanJava();
    if (selected()) checkSelected();
}
LauncherController::~LauncherController() {
    stopChecks(); *scanCancel_ = true; catalogCancel_ = true;
    if (worker_) { worker_->cancel(); worker_->wait(); delete worker_; worker_ = nullptr; }
    if (game_.state() != QProcess::NotRunning) { game_.kill(); game_.waitForFinished(5000); }
    versionsWatcher_.waitForFinished(); javaWatcher_.waitForFinished();
    for (auto *watcher : checks_) { watcher->waitForFinished(); delete watcher; }
}
QVariantList LauncherController::instances() const {
    QVariantList list;
    for (const auto &item : store_.instances()) list.append(QVariantMap{{"id", item.id}, {"name", item.name}, {"version", item.versionId}, {"directory", item.directory}, {"pending", item.pending}});
    return list;
}
QString LauncherController::selectedName() const { return selected() ? selected()->name : QString(); }
QString LauncherController::selectedVersion() const { return selected() ? selected()->versionId : QString(); }
QString LauncherController::selectedDirectory() const { return selected() ? selected()->directory : QString(); }
QString LauncherController::javaPath() const { return selected() ? selected()->javaPath : QString(); }
bool LauncherController::javaCompatible() const {
    return std::any_of(runtimes_.begin(), runtimes_.end(), [this](const auto &runtime) { return runtime.path == javaPath() && runtime.compatible(requiredJava()); });
}
QString LauncherController::playerName() const { return selected() ? selected()->playerName : QString(); }
void LauncherController::setPlayerName(const QString &name) {
    if (!storeReady_ || !selected() || busy() || name == playerName()) return;
    QString error;
    auto record = *selected(); record.playerName = name;
    if (!store_.update(record, error)) { fail(error); return; }
    emit stateChanged();
}
void LauncherController::appendLog(const QString &message) {
    if (message.isEmpty()) return;
    log_ += message + (message.endsWith('\n') ? "" : "\n");
    if (log_.size() > 60000) log_ = log_.right(50000);
    emit stateChanged();
}
void LauncherController::fail(const QString &message) { error_ = message; status_ = message; appendLog("错误：" + message); emit stateChanged(); }
void LauncherController::refreshInstances() { emit instancesChanged(); emit stateChanged(); }
void LauncherController::refreshJava() {
    javaRuntimes_.clear();
    for (const auto &runtime : runtimes_) javaRuntimes_.append(entry(runtime.label(requiredJava()), runtime.path));
    emit javaChanged();
}
void LauncherController::recommendJava() {
    if (!storeReady_ || !selected() || busy() || javaCompatible() || !requiredJava()) return;
    for (const auto &runtime : runtimes_) {
        if (!runtime.compatible(requiredJava())) continue;
        QString error;
        if (!store_.setJavaPath(selectedId(), runtime.path, error)) fail(error);
        else refreshInstances();
        return;
    }
}
void LauncherController::loadVersions(bool online) {
    if (loadingVersions_) return;
    loadingVersions_ = true; emit stateChanged();
    const QString root = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("catalog");
    versionsWatcher_.setFuture(QtConcurrent::run([this, root, online] { QString notice; const auto versions = releaseVersions(root, online, notice, &catalogCancel_); return qMakePair(versions, notice); }));
}
void LauncherController::refreshVersions() { loadVersions(true); }
void LauncherController::stopChecks() { ++generation_; *checkCancel_ = true; checking_ = false; }
void LauncherController::selectInstance(const QString &id) {
    if (!storeReady_ || busy() || scanning_ || id == selectedId()) return;
    QString error;
    if (!store_.select(id, error)) { fail(error); return; }
    stopChecks(); current_ = {}; error_.clear();
    status_ = id.isEmpty() ? "请选择或创建实例" : "准备检查实例";
    refreshInstances(); refreshJava();
    if (!id.isEmpty()) checkSelected();
}
void LauncherController::createInstance(const QString &name, const QString &version, const QString &directory) {
    if (!storeReady_ || busy() || loadingVersions_) return;
    const QString path = directory.trimmed();
    if (path.isEmpty() || !QDir::isAbsolutePath(path)) { fail("请输入完整的绝对目标目录"); return; }
    if (QVersionNumber::fromString(version) < QVersionNumber(1, 13)) { fail("本轮仅支持 1.13 起使用现代启动参数的独立正式版"); return; }
    QString id, error;
    if (!store_.add(name.trimmed(), version, QDir::cleanPath(path), {}, id, error) || !store_.select(id, error)) { fail(error); return; }
    stopChecks(); current_ = {}; error_.clear(); refreshInstances(); refreshJava(); installSelected();
}
void LauncherController::renameSelected(const QString &name) {
    if (!selected() || busy()) return;
    QString error;
    if (!store_.rename(selectedId(), name.trimmed(), error)) { fail(error); return; }
    refreshInstances();
}
// 取消只发出协作请求；旧检查仍可能排队回传，因此每轮同时捕获独立取消标记和代际编号。
// 所有界面状态写入都回到控制器线程，目录/版本则按启动检查时的值捕获。
void LauncherController::checkSelected() {
    if (busy()) return;
    stopChecks(); error_.clear(); current_ = {};
    if (!selected()) { status_ = "请选择或创建实例"; emit stateChanged(); return; }
    checking_ = true; progress_ = 0; progressTotal_ = 0; status_ = "正在检查 " + selectedName(); emit stateChanged();
    const auto token = generation_;
    checkCancel_ = std::make_shared<std::atomic_bool>(false);
    const auto cancel = checkCancel_;
    const QString root = selectedDirectory(), version = selectedVersion();
    auto *watcher = new QFutureWatcher<VersionStatus>(this); checks_.append(watcher);
    connect(watcher, &QFutureWatcher<VersionStatus>::finished, this, [this, watcher, token, cancel] {
        const auto result = watcher->result(); checks_.removeAll(watcher); watcher->deleteLater();
        if (token != generation_ || *cancel || busy()) return;
        current_ = result; checking_ = false; progress_ = result.files.checked; progressTotal_ = qMax(1, progress_);
        status_ = selectedName() + " · " + result.message; refreshJava(); recommendJava(); emit stateChanged();
    });
    watcher->setFuture(QtConcurrent::run([this, root, version, token, cancel] {
        return inspectVersion(root, version, false, [this, token](int done, int total) {
            if (done == 1 || done == total || done % 100 == 0) QMetaObject::invokeMethod(this, [this, token, done, total] {
                if (token == generation_) { progress_ = done; progressTotal_ = total; emit stateChanged(); }
            }, Qt::QueuedConnection);
        }, cancel.get());
    }));
}
void LauncherController::installSelected() { startTask(false); }
void LauncherController::startTask(bool launch) {
    if (!storeReady_ || !selected() || busy()) return;
    const auto item = *selected();
    stopChecks(); *scanCancel_ = true;
    taskCancelled_ = false; error_.clear(); progress_ = 0; progressTotal_ = 0;
    auto *worker = new Installer(item.directory, item.versionId); worker_ = worker;
    if (launch) worker->prepareLaunch(item.playerName.trimmed(), item.javaPath,
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("runtime"));
    status_ = launch ? "正在检查文件" : "正在安装 / 修复 " + item.name;
    connect(worker, &Installer::progress, this, [this, worker](int done, int total, const QString &detail) {
        if (worker != worker_ || taskCancelled_) return;
        progress_ = done; progressTotal_ = total; status_ = detail; emit stateChanged();
    });
    // 使用 finished 而非 run 内的 completed：此时工作线程已停止修改结果。
    // 捕获实例快照防止身份串用；清理守卫覆盖所有提前返回，取消确认前不能释放 busy。
    connect(worker, &QThread::finished, this, [this, worker, item, launch] {
        if (worker != worker_) return;
        worker->deleteLater();
        const auto cleanup = qScopeGuard([this] { worker_ = nullptr; emit stateChanged(); });
        if (taskCancelled_ || worker->cancelled()) { status_ = "已取消"; error_.clear(); return; }
        appendLog(worker->message());
        if (taskCancelled_) { status_ = "已取消"; return; }
        if (!worker->succeeded()) { fail(worker->message()); return; }
        if (selectedId() != item.id) { fail("任务所属实例已改变"); return; }
        QString error;
        auto updated = item; updated.pending = false;
        if (launch) updated.javaPath = worker->runtime().path;
        if (!store_.update(updated, error)) { fail(error); return; }
        if (!launch) { current_ = worker->result(); status_ = worker->message(); refreshInstances(); return; }
        current_ = worker->result();
        runtimes_.append(worker->runtime()); refreshJava();
        if (taskCancelled_) { status_ = "已取消"; return; }
        gameName_ = item.name; gameStopped_ = false;
        game_.setWorkingDirectory(item.directory); game_.setProcessChannelMode(QProcess::MergedChannels);
        status_ = "正在启动进程 " + item.name;
        game_.start(worker->runtime().path, worker->arguments());
        refreshInstances();
    });
    emit stateChanged(); worker->start();
}
void LauncherController::scanJava() {
    if (scanning_ || busy()) return;
    scanning_ = true; scanCancel_ = std::make_shared<std::atomic_bool>(false); emit stateChanged();
    const auto cancel = scanCancel_; const QString preferred = javaPath();
    javaWatcher_.setFuture(QtConcurrent::run([preferred, cancel] { return discoverJava(preferred, cancel.get()); }));
}
void LauncherController::chooseJava(const QString &path) {
    if (!storeReady_ || !selected() || busy() || scanning_ || path.isEmpty()) return;
    manualJava_ = path; scanning_ = true; scanCancel_ = std::make_shared<std::atomic_bool>(false); emit stateChanged();
    const auto cancel = scanCancel_;
    javaWatcher_.setFuture(QtConcurrent::run([path, cancel] { const auto runtime = inspectJava(path, cancel.get()); return runtime.major ? QList<JavaRuntime>{runtime} : QList<JavaRuntime>{}; }));
}
void LauncherController::launchSelected() {
    if (!storeReady_ || !selected() || busy()) return;
    const QString player = playerName().trimmed();
    if (player.isEmpty()) { fail("玩家名不能为空！"); return; }
    if (player.contains('"') || player.contains(QRegularExpression("[\\x00-\\x1f\\x7f]"))) { fail("玩家名不能包含英文引号或控制字符！"); return; }
    if (QVersionNumber::fromString(selectedVersion()) >= QVersionNumber(1, 20, 3) && player.size() > 16) {
        fail("自 1.20.3 起，玩家名至多只能包含 16 个字符！"); return;
    }
    if (!QRegularExpression("^[0-9A-Za-z_]+$").match(player).hasMatch()) appendLog("提示：非标准玩家名可能无法加入部分服务器");
    startTask(true);
}
void LauncherController::cancel() {
    if (worker_) { taskCancelled_ = true; worker_->cancel(); status_ = "正在取消准备…"; }
    else if (game_.state() != QProcess::NotRunning) { gameStopped_ = true; game_.kill(); }
    else if (checking_) { stopChecks(); status_ = "已取消检查"; }
    if (scanning_) {
        *scanCancel_ = true;
        if (!busy() && !checking_) status_ = "已取消 Java 扫描";
    }
    emit stateChanged();
}
