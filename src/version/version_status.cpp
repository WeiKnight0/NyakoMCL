#include "version_status.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QEventLoop>
#include <QTimer>
#include <QVersionNumber>

namespace {
QByteArray readFile(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
QByteArray get(const QString &url, const std::atomic_bool *cancel = nullptr) {
    if (QUrl(url).scheme() != "https") return {};
    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(20000);
    QNetworkReply *reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] { if (cancel && *cancel) reply->abort(); });
    poll.start(50);
    loop.exec();
    const QByteArray bytes = reply->error() == QNetworkReply::NoError && reply->size() < 10 * 1024 * 1024 ? reply->readAll() : QByteArray();
    delete reply;
    return bytes;
}
QJsonObject manifest(const QString &root, bool online, QString &message, const std::atomic_bool *cancel = nullptr) {
    const QString path = QDir(root).filePath("versions/manifest.json");
    QByteArray bytes;
    if (online) {
        bytes = get("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json", cancel);
        if (!QJsonDocument::fromJson(bytes).object().value("versions").isArray()) bytes.clear();
        if (!bytes.isEmpty()) {
            QDir().mkpath(QFileInfo(path).absolutePath());
            QSaveFile out(path);
            if (out.open(QIODevice::WriteOnly) && out.write(bytes) == bytes.size()) out.commit();
            message = "官方版本列表已更新";
        }
    }
    if (bytes.isEmpty()) {
        bytes = readFile(path);
        message = bytes.isEmpty() ? "无法获取官方清单，仍可选择本地已安装版本" : "网络不可用，使用本地缓存清单（可能已过期）";
    }
    return QJsonDocument::fromJson(bytes).object();
}
}
QStringList releaseVersions(const QString &root, bool online, QString &message, const std::atomic_bool *cancel) {
    QStringList ids;
    const auto data = manifest(root, online, message, cancel);
    for (const auto &value : data.value("versions").toArray()) {
        const auto entry = value.toObject();
        if (entry.value("type").toString() == "release" && validVersionId(entry.value("id").toString()) && QVersionNumber::fromString(entry.value("id").toString()) >= QVersionNumber(1, 13)) ids << entry.value("id").toString();
    }
    QDir versions(QDir(root).filePath("versions"));
    for (const auto &id : versions.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        if (validVersionId(id) && QFile::exists(versionFile(root, id)) && !ids.contains(id)) ids.prepend(id);
    return ids;
}
VersionStatus inspectVersion(const QString &root, const QString &id, bool online,
                             const std::function<void(int, int)> &progress, const std::atomic_bool *cancel) {
    VersionStatus result;
    if (!validVersionId(id)) { result.message = "版本 ID 无效"; return result; }
    const QString path = versionFile(root, id);
    QByteArray bytes = readFile(path);
    QString notice;
    const auto data = manifest(root, false, notice);
    QJsonObject selected;
    for (const auto &value : data.value("versions").toArray())
        if (value.toObject().value("id").toString() == id && value.toObject().value("type").toString() == "release") { selected = value.toObject(); break; }
    if (online && !selected.isEmpty() && QUrl(selected.value("url").toString()).scheme() == "https") {
        const QByteArray remote = get(selected.value("url").toString(), cancel);
        if (QCryptographicHash::hash(remote, QCryptographicHash::Sha1).toHex().compare(selected.value("sha1").toString().toLatin1(), Qt::CaseInsensitive) == 0)
            bytes = remote;
        else if (bytes.isEmpty()) { result.message = "版本元数据下载或 SHA-1 校验失败"; return result; }
    }
    if (bytes.isEmpty()) { result.message = selected.isEmpty() ? "未安装：本地无元数据，官方清单未载入" : "未安装：请先安装此版本"; return result; }
    if (!selected.isEmpty() && QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex().compare(selected.value("sha1").toString().toLatin1(), Qt::CaseInsensitive) != 0) {
        result.message = "版本元数据已损坏或本地清单过期，请刷新后修复"; return result;
    }
    const auto version = QJsonDocument::fromJson(bytes).object();
    const auto descriptor = version.value("assetIndex").toObject();
    const QString indexId = descriptor.value("id").toString();
    if (!validVersionId(indexId)) { result.message = "暂不支持此版本：资源索引元数据无效"; return result; }
    const QString indexPath = QDir(root).filePath("assets/indexes/" + indexId + ".json");
    QByteArray index = readFile(indexPath);
    if (online && !descriptor.isEmpty()) {
        const Download download{descriptor.value("url").toString(), descriptor.value("sha1").toString(), indexPath,
                                descriptor.value("size").isDouble() ? qint64(descriptor.value("size").toDouble()) : -1};
        const QByteArray remote = validDownload(download) ? get(download.url, cancel) : QByteArray();
        if (remote.size() == download.size && QCryptographicHash::hash(remote, QCryptographicHash::Sha1).toHex().compare(download.sha1.toLatin1(), Qt::CaseInsensitive) == 0) index = remote;
    }
    if (index.isEmpty()) { result.message = "资源索引尚未安装或网络不可用，请安装 / 修复"; return result; }
    QString error;
    if (!buildVersionPlan(version, root, id, QJsonDocument::fromJson(index).object(), result.plan, error)) {
        result.message = error; return result;
    }
    result.supported = true;
    result.files = evaluatePlan(result.plan, cancel, progress);
    if (cancel && *cancel) { result.message = "已取消检查"; return result; }
    // 在线获取成功仅证明远端数据可解析；可启动结论仍要求本地元数据和资源索引通过校验。
    const bool localMetadata = QFile::exists(path) && (selected.isEmpty() ||
        QCryptographicHash::hash(readFile(path), QCryptographicHash::Sha1).toHex().compare(selected.value("sha1").toString().toLatin1(), Qt::CaseInsensitive) == 0);
    const Download localIndex{descriptor.value("url").toString(), descriptor.value("sha1").toString(), indexPath,
                              qint64(descriptor.value("size").toDouble(-1))};
    const bool localAssets = validDownload(localIndex) && validFile(localIndex, cancel);
    result.installed = !(cancel && *cancel) && result.files.ready() && localMetadata && localAssets;
    result.message = result.installed ? "可启动" : !QFile::exists(path) ? "未安装：元数据可用，请安装此版本" :
        !localMetadata || !localAssets ? "需要修复：本地元数据或资源索引缺失/损坏" :
        QString("需要修复：缺失 %1，损坏 %2；%3")
            .arg(result.files.missing).arg(result.files.damaged).arg(result.files.examples.join(", "));
    return result;
}
