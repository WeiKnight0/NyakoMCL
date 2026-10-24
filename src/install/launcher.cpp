#include "launcher.h"
#include "java/java_runtime_provider.h"

#include <QCryptographicHash>
#include <QtConcurrent>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>
#include <QtEndian>
#include <zlib.h>

namespace {
quint16 u16(const QByteArray &bytes, int offset) { return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(bytes.constData() + offset)); }
quint32 u32(const QByteArray &bytes, int offset) { return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData() + offset)); }
}

Installer::Installer(QString root, QString id, QObject *parent)
    : QThread(parent), root_(QDir(root).absolutePath()), id_(std::move(id)) {}
void Installer::cancel() { cancelled_ = true; }
void Installer::prepareLaunch(QString player, QString preferred, QString runtimeRoot) {
    launching_ = true;
    player_ = std::move(player);
    preferred_ = std::move(preferred);
    runtimeRoot_ = std::move(runtimeRoot);
}
void Installer::run() {
    // QThread 对象仍属于创建线程；网络对象必须在 run 的工作线程内创建，不能跨线程复用。
    // completed 仅表示业务结果已写入，控制器等待 finished 后才读取结果并启动主线程游戏进程。
    QNetworkAccessManager network;
    stage_ = launching_ ? "检查文件" : "安装 / 修复";
    success_ = launching_ ? prepare(network) : install(network);
    message_ = cancelled_ ? "已取消" : success_ ? id_ + (launching_ ? " 启动准备完成" : " 安装校验完成") : stage_ + "失败：" + error_;
    emit completed(success_ && !cancelled_, message_);
}
bool Installer::prepare(QNetworkAccessManager &network) {
    emit progress(0, 0, stage_);
    result_ = inspectVersion(root_, id_, false, [this](int done, int total) {
        if (done == 1 || done == total || done % 100 == 0) emit progress(done, total, stage_);
    }, &cancelled_);
    if (cancelled_) return false;
    if (!result_.installed) {
        stage_ = "修复文件";
        emit progress(0, 0, stage_);
        if (!install(network)) return false;
    }
    stage_ = "查找兼容 Java";
    emit progress(0, 0, stage_);
    runtime_ = inspectJava(preferred_, &cancelled_);
    if (!runtime_.compatible(result_.plan.javaMajor)) {
        for (const auto &candidate : discoverJava({}, &cancelled_)) {
            if (candidate.compatible(result_.plan.javaMajor)) { runtime_ = candidate; break; }
        }
    }
    if (cancelled_) return false;
    if (!runtime_.compatible(result_.plan.javaMajor)) {
        stage_ = "下载官方 Java";
        emit progress(0, 0, stage_);
        runtime_ = provideJava(result_.plan.metadata.value("javaVersion").toObject(), runtimeRoot_,
            [this, &network](const QString &url, const QString &path, const QString &sha) { return fetchMetadata(network, url, path, sha); },
            [this](const QList<Download> &files) { return fetchFiles(files); }, cancelled_, error_,
            [this](const QString &stage) { emit progress(0, 0, stage); });
        if (!runtime_.compatible(result_.plan.javaMajor)) return false;
    }
    stage_ = "生成启动参数";
    emit progress(0, 0, stage_);
    return !cancelled_ && launchArguments(result_.plan, player_, arguments_, error_);
}
bool Installer::fetchFiles(const QList<Download> &files) {
    failed_ = false;
    error_.clear();
    total_ = files.size();
    done_ = 0;
    // 每个池任务创建自己的网络管理器；共享计数使用原子量，首个错误文本受互斥锁保护。
    // 等待整批任务退出后才返回，避免 Installer 结果发布时仍有下载写入。
    QThreadPool pool;
    pool.setMaxThreadCount(16);
    auto future = QtConcurrent::mappedReduced(&pool, files, [this](const Download &item) {
        QNetworkAccessManager manager;
        const bool success = fetch(item, manager);
        if (!success) failed_ = true;
        return success;
    }, [](bool &failures, bool success) { failures = failures || !success; },
    QtConcurrent::UnorderedReduce | QtConcurrent::SequentialReduce);
    future.waitForFinished();
    return !future.result() && !cancelled_ && !failed_;
}

bool Installer::valid(const Download &item) const {
    if (item.size >= 0) return validFile(item, &cancelled_);
    QFile file(item.path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QCryptographicHash hash(QCryptographicHash::Sha1);
    while (!file.atEnd()) {
        if (cancelled_) return false;
        hash.addData(file.read(1024 * 1024));
    }
    return hash.result().toHex().compare(item.sha1.toLatin1(), Qt::CaseInsensitive) == 0;
}

bool Installer::fetch(const Download &item, QNetworkAccessManager &network) {
    if (cancelled_ || failed_) return false;
    if (!validDownload(item)) {
        QMutexLocker lock(&errorMutex_);
        if (error_.isEmpty()) error_ = "无效的下载元数据: " + item.path;
        return false;
    }
    if (valid(item)) { const int done = ++done_; emit progress(done, total_, item.path); return true; }
    const QString parentPath = QFileInfo(item.path).absolutePath();
    {
        QMutexLocker lock(&errorMutex_);
        if (!QDir().mkpath(parentPath) && !QDir(parentPath).exists()) {
            if (error_.isEmpty()) error_ = "无法创建下载目录：" + parentPath;
            return false;
        }
    }
    // 边接收边累计长度与 SHA-1，只有全部匹配才提交 QSaveFile；失败/取消不发布半个文件。
    for (int attempt = 1; attempt <= 3 && !cancelled_ && !failed_; ++attempt) {
        QSaveFile output(item.path);
        if (!output.open(QIODevice::WriteOnly)) {
            QMutexLocker lock(&errorMutex_);
            if (error_.isEmpty()) error_ = "无法写入文件：" + item.path + "\n" + output.errorString();
            return false;
        }
        QNetworkRequest request{QUrl(item.url)};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setTransferTimeout(30000);
        auto *reply = network.get(request);
        QEventLoop loop;
        QTimer poll;
        poll.setInterval(100);
        QCryptographicHash hash(QCryptographicHash::Sha1);
        qint64 length = 0;
        bool writeOk = true;
        auto consume = [&] {
            const QByteArray data = reply->readAll();
            if (!data.isEmpty()) {
                length += data.size();
                hash.addData(data);
                if (output.write(data) != data.size()) writeOk = false;
            }
        };
        connect(reply, &QIODevice::readyRead, &loop, consume);
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        connect(&poll, &QTimer::timeout, &loop, [&] { if (cancelled_ || failed_ || !writeOk) reply->abort(); });
        poll.start();
        loop.exec();
        consume();
        const bool ok = !cancelled_ && writeOk && reply->error() == QNetworkReply::NoError &&
                        length == item.size && hash.result().toHex().compare(item.sha1.toLatin1(), Qt::CaseInsensitive) == 0;
        const QString reason = reply->error() != QNetworkReply::NoError ? reply->errorString() :
            !writeOk ? "文件写入失败" : length != item.size ? "文件大小校验不匹配" : "SHA-1 校验不匹配";
        const QString failure = QString("下载或校验失败 (%1/3): %2: %3").arg(attempt).arg(item.url, reason);
        reply->deleteLater();
        if (ok && output.commit()) {
            const int done = ++done_;
            emit progress(done, total_, item.path);
            return true;
        }
        {
            QMutexLocker lock(&errorMutex_);
            if (attempt == 3 && error_.isEmpty()) error_ = ok ? "写入失败: " + item.path : failure;
        }
        output.cancelWriting();
        if (attempt < 3 && !cancelled_) QThread::msleep(300 * attempt);
    }
    return false;
}

bool Installer::fetchMetadata(QNetworkAccessManager &network, const QString &url, const QString &path, const QString &sha1) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    if (!sha1.isEmpty() && valid({url, sha1, path, -1})) return true;
    for (int attempt = 1; attempt <= 3 && !cancelled_ && !failed_; ++attempt) {
        QNetworkRequest request{QUrl(url)};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setTransferTimeout(30000);
        auto *reply = network.get(request);
        QEventLoop loop;
        QTimer poll;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        connect(&poll, &QTimer::timeout, &loop, [&] { if (cancelled_) reply->abort(); });
        poll.start(100);
        loop.exec();
        const QByteArray bytes = reply->readAll();
        const bool ok = !cancelled_ && reply->error() == QNetworkReply::NoError && !bytes.isEmpty() && bytes.size() < 10 * 1024 * 1024 &&
                        (sha1.isEmpty() || QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex().compare(sha1.toLatin1(), Qt::CaseInsensitive) == 0);
        const QString failure = QString("元数据获取失败 (%1/3): %2: %3").arg(attempt).arg(url, reply->errorString());
        reply->deleteLater();
        if (ok) {
            QSaveFile output(path);
            if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit()) { error_ = "元数据写入失败: " + path; return false; }
            error_.clear();
            return true;
        }
        error_ = failure;
        if (attempt < 3 && !cancelled_) QThread::msleep(300 * attempt);
    }
    return false;
}

// 只接受有界、单卷且非加密/符号链接的 ZIP 条目；写入前检查相对路径、解压长度与 CRC。
// 不信任归档文件名或中央目录中的偏移，避免越界读取和向实例外释放文件。
bool Installer::extractNatives(const QString &archive, const QStringList &excludes, const QString &destination) {
    QFile file(archive);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 100 * 1024 * 1024) { error_ = "无法读取 native ZIP: " + archive; return false; }
    const QByteArray zip = file.readAll();
    int end = -1;
    for (int i = zip.size() - 22; i >= qMax(0, zip.size() - 65557); --i)
        if (u32(zip, i) == 0x06054b50 && i + 22 + u16(zip, i + 20) == zip.size()) { end = i; break; }
    if (end < 0 || u16(zip, end + 4) != 0 || u16(zip, end + 6) != 0) { error_ = "无效的 native ZIP"; return false; }
    const quint32 count = u16(zip, end + 10), centralSize = u32(zip, end + 12), central = u32(zip, end + 16);
    if (quint64(central) + centralSize > quint64(end) || count == 0xffff) { error_ = "不支持的 native ZIP"; return false; }
    quint64 pos = central;
    for (quint32 index = 0; index < count; ++index) {
        if (cancelled_) return false;
        if (pos + 46 > quint64(zip.size()) || u32(zip, int(pos)) != 0x02014b50) { error_ = "损坏的 native ZIP"; return false; }
        const quint16 method = u16(zip, int(pos + 10)), nameLength = u16(zip, int(pos + 28));
        const quint16 extraLength = u16(zip, int(pos + 30)), commentLength = u16(zip, int(pos + 32));
        const quint32 crc = u32(zip, int(pos + 16)), packed = u32(zip, int(pos + 20)), unpacked = u32(zip, int(pos + 24));
        const quint32 local = u32(zip, int(pos + 42));
        if (pos + 46 + nameLength + extraLength + commentLength > quint64(zip.size()) ||
            packed > 100 * 1024 * 1024 || unpacked > 100 * 1024 * 1024 ||
            (u16(zip, int(pos + 8)) & 1) || ((u32(zip, int(pos + 38)) >> 16) & 0xf000) == 0xa000) {
            error_ = "不支持的 native ZIP 条目"; return false;
        }
        const QString name = QString::fromUtf8(zip.constData() + pos + 46, nameLength);
        pos += 46 + nameLength + extraLength + commentLength;
        if (name.endsWith('/') || name.startsWith("META-INF/", Qt::CaseInsensitive)) continue;
        bool excluded = false;
        for (const QString &prefix : excludes) if (name.startsWith(prefix, Qt::CaseInsensitive)) excluded = true;
        if (excluded) continue;
        const QString target = safePath(destination, name);
        if (target.isEmpty() || quint64(local) + 30 > quint64(zip.size()) || u32(zip, local) != 0x04034b50) { error_ = "非法的 native ZIP 路径"; return false; }
        const quint64 start = quint64(local) + 30 + u16(zip, local + 26) + u16(zip, local + 28);
        if (start + packed > quint64(zip.size())) { error_ = "损坏的 native ZIP 数据"; return false; }
        QByteArray plain;
        if (method == 0) plain = QByteArray(zip.constData() + start, packed);
        else if (method == 8) {
            plain.resize(unpacked);
            z_stream stream{};
            stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(zip.constData() + start));
            stream.avail_in = packed;
            stream.next_out = reinterpret_cast<Bytef *>(plain.data());
            stream.avail_out = unpacked;
            if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) { error_ = "ZIP 解压初始化失败"; return false; }
            const int status = inflate(&stream, Z_FINISH);
            inflateEnd(&stream);
            if (status != Z_STREAM_END || stream.total_out != unpacked) { error_ = "native 解压失败"; return false; }
        } else { error_ = "不支持的 ZIP 压缩方式"; return false; }
        if (plain.size() != unpacked || crc32(0, reinterpret_cast<const Bytef *>(plain.constData()), plain.size()) != crc) { error_ = "native CRC 校验失败"; return false; }
        QDir().mkpath(QFileInfo(target).absolutePath());
        QSaveFile output(target);
        if (!output.open(QIODevice::WriteOnly) || output.write(plain) != plain.size() || !output.commit()) { error_ = "native 写入失败: " + target; return false; }
    }
    return true;
}

bool Installer::install(QNetworkAccessManager &network) {
    if (!validVersionId(id_)) { error_ = "版本 ID 无效"; return false; }
    const QString manifestPath = QDir(root_).filePath("versions/manifest.json");
    emit progress(0, 0, "获取官方版本列表");
    if (!fetchMetadata(network, "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json", manifestPath)) {
        QFile cached(manifestPath);
        if (!cached.open(QIODevice::ReadOnly)) return false;
        error_.clear();
    }
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly)) { error_ = "无法读取版本列表"; return false; }
    const auto manifestJson = QJsonDocument::fromJson(manifestFile.readAll()).object();
    QJsonObject selected;
    for (const auto &entry : manifestJson.value("versions").toArray())
        if (entry.toObject().value("id").toString() == id_ && entry.toObject().value("type").toString() == "release") { selected = entry.toObject(); break; }
    if (selected.isEmpty()) { error_ = "官方列表中没有该正式版：" + id_; return false; }
    const QString metadataPath = versionFile(root_, id_);
    const QString sha1 = selected.value("sha1").toString();
    const QString url = selected.value("url").toString();
    if (!QRegularExpression("^[a-fA-F0-9]{40}$").match(sha1).hasMatch() ||
        QUrl(url).scheme() != "https") { error_ = "无效的版本条目"; return false; }
    if (!fetchMetadata(network, url, metadataPath, sha1)) return false;
    QFile version(metadataPath);
    if (!version.open(QIODevice::ReadOnly)) { error_ = "无法读取版本 JSON"; return false; }
    const auto json = QJsonDocument::fromJson(version.readAll()).object();
    const auto index = json.value("assetIndex").toObject();
    const QString indexId = index.value("id").toString();
    if (!validVersionId(indexId)) { error_ = "暂不支持此版本：资源索引 ID 无效"; return false; }
    const Download assetIndex{index.value("url").toString(), index.value("sha1").toString(),
                              QDir(root_).filePath("assets/indexes/" + indexId + ".json"),
                              index.value("size").isDouble() ? qint64(index.value("size").toDouble()) : -1};
    if (!validDownload(assetIndex)) { error_ = "暂不支持此版本：资源索引元数据不完整"; return false; }
    total_ = 1;
    if (!fetch(assetIndex, network)) return false;
    QFile assetFile(assetIndex.path);
    if (!assetFile.open(QIODevice::ReadOnly)) { error_ = "无法读取资源索引"; return false; }
    VersionPlan plan;
    if (!buildVersionPlan(json, root_, id_, QJsonDocument::fromJson(assetFile.readAll()).object(), plan, error_)) return false;
    if (!fetchFiles(plan.files)) return false;
    if (!plan.archives.isEmpty()) {
        if (!QDir().mkpath(plan.natives)) { error_ = "无法创建原生库目录"; return false; }
        for (const auto &archive : plan.archives)
            if (!extractNatives(archive.file.path, archive.excludes, plan.natives)) return false;
    }
    const auto status = evaluatePlan(plan, &cancelled_);
    if (!status.ready()) { error_ = QString("安装后校验失败：缺失 %1，损坏 %2：%3")
        .arg(status.missing).arg(status.damaged).arg(status.examples.join(", ")); return false; }
    result_.plan = plan;
    result_.files = status;
    result_.supported = result_.installed = !cancelled_;
    result_.message = "可启动";
    return !cancelled_;
}
