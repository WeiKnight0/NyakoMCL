#include "version_model.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSysInfo>
#include <QUrl>
#include <QtEndian>
#include <zlib.h>

namespace {
QString osName() {
#ifdef Q_OS_WIN
    return "windows";
#elif defined(Q_OS_MACOS)
    return "osx";
#else
    return "linux";
#endif
}
QString archName() { return sizeof(void *) == 8 ? "x64" : "x86"; }
bool sha(const QString &value) { return QRegularExpression("^[a-fA-F0-9]{40}$").match(value).hasMatch(); }
// 缺少 rules 默认允许；存在规则时默认拒绝，由最后一条匹配规则决定。
// 未支持的结构直接拒绝；当前离线模式不启用 features 中声明的任何功能。
bool rulesAllow(const QJsonValue &value, bool &allowed) {
    allowed = true;
    if (value.isUndefined()) return true;
    if (!value.isArray()) return false;
    allowed = false;
    for (const auto &entry : value.toArray()) {
        if (!entry.isObject()) return false;
        const auto rule = entry.toObject();
        if (rule.size() != 1 + int(rule.contains("os")) + int(rule.contains("features")) ||
            !rule.value("action").isString()) return false;
        const QString action = rule.value("action").toString();
        if (action != "allow" && action != "disallow") return false;
        bool matches = true;
        if (rule.contains("os")) {
            if (!rule.value("os").isObject()) return false;
            const auto os = rule.value("os").toObject();
            for (auto it = os.begin(); it != os.end(); ++it)
                if ((it.key() != "name" && it.key() != "arch" && it.key() != "version") || !it.value().isString()) return false;
            if (os.contains("name") && os.value("name").toString() != osName()) matches = false;
            if (os.contains("arch") && os.value("arch").toString() != archName()) matches = false;
            if (os.contains("version")) {
                const QRegularExpression regex(os.value("version").toString());
                if (!regex.isValid()) return false;
                if (!regex.match(QSysInfo::productVersion()).hasMatch()) matches = false;
            }
        }
        if (rule.contains("features")) {
            if (!rule.value("features").isObject()) return false;
            const auto features = rule.value("features").toObject();
            for (auto it = features.begin(); it != features.end(); ++it) {
                if (!it.value().isBool()) return false;
                if (it.value().toBool()) matches = false;
            }
        }
        if (matches) allowed = action == "allow";
    }
    return true;
}
Download descriptor(const QJsonObject &object, const QString &path) {
    return {object.value("url").toString(), object.value("sha1").toString(), path,
            object.value("size").isDouble() ? qint64(object.value("size").toDouble()) : -1};
}
// 未知占位符必须失败，不能将未展开的元数据传给 JVM；列表元素保持独立，不经 shell 拼接。
bool argumentValues(const QJsonArray &array, const QMap<QString, QString> &vars, QStringList &out) {
    auto append = [&](const QJsonValue &value) {
        if (!value.isString()) return false;
        QString text = value.toString();
        static const QRegularExpression placeholder("\\$\\{([^}]+)\\}");
        auto match = placeholder.globalMatch(text);
        while (match.hasNext()) if (!vars.contains(match.next().captured(1))) return false;
        for (auto it = vars.begin(); it != vars.end(); ++it) text.replace("${" + it.key() + "}", it.value());
        if (text.contains("${")) return false;
        out << text;
        return true;
    };
    for (const auto &value : array) {
        if (value.isString()) { if (!append(value)) return false; continue; }
        if (!value.isObject()) return false;
        const auto object = value.toObject();
        if (!object.contains("rules") || !object.contains("value") || object.size() != 2) return false;
        bool allowed;
        if (!rulesAllow(object.value("rules"), allowed)) return false;
        if (!allowed) continue;
        const auto v = object.value("value");
        if (v.isString()) { if (!append(v)) return false; }
        else if (v.isArray()) { for (const auto &part : v.toArray()) if (!append(part)) return false; }
        else return false;
    }
    return true;
}
quint16 u16(const QByteArray &b, int i) { return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(b.constData() + i)); }
quint32 u32(const QByteArray &b, int i) { return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(b.constData() + i)); }
}

bool validVersionId(const QString &id) { return QRegularExpression("^[A-Za-z0-9][A-Za-z0-9._-]{0,79}$").match(id).hasMatch() && id != ".."; }
QString safePath(const QString &base, const QString &relative) {
    if (relative.isEmpty() || relative.startsWith('/') || relative.contains('\\') || relative.contains(':')) return {};
    for (const auto &segment : relative.split('/')) if (segment.isEmpty() || segment == "." || segment == "..") return {};
    return QDir(base).filePath(relative);
}
QString versionFile(const QString &root, const QString &id) {
    return validVersionId(id) ? QDir(root).filePath("versions/" + id + "/" + id + ".json") : QString();
}
bool validDownload(const Download &item) {
    const QUrl url(item.url);
    return url.isValid() && url.scheme() == "https" && !url.host().isEmpty() &&
           url.userInfo().isEmpty() && sha(item.sha1) && item.size >= 0 && !item.path.isEmpty();
}
bool validFile(const Download &item, const std::atomic_bool *cancel) {
    QFile file(item.path);
    if (!file.open(QIODevice::ReadOnly) || file.size() != item.size) return false;
    QCryptographicHash hash(QCryptographicHash::Sha1);
    while (!file.atEnd()) {
        if (cancel && *cancel) return false;
        hash.addData(file.read(1024 * 1024));
    }
    return hash.result().toHex().compare(item.sha1.toLatin1(), Qt::CaseInsensitive) == 0;
}

bool buildVersionPlan(const QJsonObject &version, const QString &root, const QString &id,
                      const QJsonObject &index, VersionPlan &plan, QString &error) {
    auto reject = [&](const QString &why) { error = "暂不支持此版本：" + why; return false; };
    if (!validVersionId(id) || version.value("id").toString() != id || version.value("type").toString() != "release" ||
        version.contains("inheritsFrom") || version.contains("minecraftArguments")) return reject("不是独立的现代正式版");
    const auto args = version.value("arguments").toObject();
    if (!args.value("jvm").isArray() || !args.value("game").isArray() ||
        version.value("mainClass").toString().isEmpty() || !version.value("libraries").isArray()) return reject("启动参数不完整");
    const int major = version.value("javaVersion").toObject().value("majorVersion").toInt();
    if (major < 8 || major > 50) return reject("缺少 Java 版本要求");
    plan = {};
    plan.id = id; plan.root = QDir(root).absolutePath(); plan.metadata = version; plan.javaMajor = major;
    plan.natives = QDir(plan.root).filePath("versions/" + id + "/natives");
    auto add = [&](const QJsonObject &object, const QString &path) {
        const Download download = descriptor(object, path);
        if (!validDownload(download)) return false;
        plan.files << download;
        return true;
    };
    if (!add(version.value("downloads").toObject().value("client").toObject(),
             QDir(plan.root).filePath("versions/" + id + "/" + id + ".jar"))) return reject("客户端文件缺少 URL、SHA-1 或大小");
    const QString libraries = QDir(plan.root).filePath("libraries");
    for (const auto &entry : version.value("libraries").toArray()) {
        if (!entry.isObject()) return reject("库条目格式无效");
        const auto lib = entry.toObject();
        bool allowed;
        if (!rulesAllow(lib.value("rules"), allowed)) return reject("未知的库平台规则");
        if (!allowed) continue;
        const auto files = lib.value("downloads").toObject();
        const auto artifact = files.value("artifact").toObject();
        if (!artifact.isEmpty()) {
            const QString path = safePath(libraries, artifact.value("path").toString());
            if (!add(artifact, path)) return reject("库文件缺少可信元数据或路径不安全");
            plan.classpath << path;
        }
        const auto natives = lib.value("natives").toObject();
        if (!natives.isEmpty()) {
            QString key = natives.value(osName()).toString();
            key.replace("${arch}", sizeof(void *) == 8 ? "64" : "32");
            if (!key.isEmpty()) {
                const auto native = files.value("classifiers").toObject().value(key).toObject();
                const QString path = safePath(libraries, native.value("path").toString());
                if (!add(native, path)) return reject("原生库缺少可信元数据或格式不支持");
                NativeArchive archive{plan.files.last(), {}};
                const auto extract = lib.value("extract").toObject();
                if (extract.contains("exclude") && !extract.value("exclude").isArray()) return reject("原生库排除规则无效");
                for (const auto &excluded : extract.value("exclude").toArray()) {
                    if (!excluded.isString() || excluded.toString().contains("..")) return reject("原生库排除规则无效");
                    archive.excludes << excluded.toString();
                }
                plan.archives << archive;
            }
        }
        if (artifact.isEmpty() && natives.isEmpty()) return reject("库缺少可用文件");
    }
    const auto asset = version.value("assetIndex").toObject();
    const QString assetId = asset.value("id").toString();
    if (!validVersionId(assetId) || !add(asset, QDir(plan.root).filePath("assets/indexes/" + assetId + ".json"))) return reject("资源索引元数据无效");
    if (index.isEmpty() || !index.value("objects").isObject() || index.value("objects").toObject().isEmpty()) return reject("资源索引为空或格式不正确");
    const auto objects = index.value("objects").toObject();
    for (auto it = objects.begin(); it != objects.end(); ++it) {
        const auto object = it.value().toObject();
        const QString hash = object.value("hash").toString();
        if (!sha(hash) || !object.value("size").isDouble()) return reject("资源对象元数据不完整");
        const QString path = QDir(plan.root).filePath("assets/objects/" + hash.left(2) + "/" + hash);
        if (!add(QJsonObject{{"url", "https://resources.download.minecraft.net/" + hash.left(2) + "/" + hash},
                             {"sha1", hash}, {"size", object.value("size")}}, path)) return reject("资源对象无效");
    }
    const auto logging = version.value("logging").toObject().value("client").toObject();
    if (!logging.isEmpty()) {
        const auto file = logging.value("file").toObject();
        const QString logId = file.value("id").toString();
        if (logging.value("type").toString() != "log4j2-xml" || !validVersionId(logId) ||
            !add(file, QDir(plan.root).filePath("assets/log_configs/" + logId))) return reject("日志配置格式不支持");
    }
    plan.classpath << plan.files.first().path;
    QStringList probe;
    if (!launchArguments(plan, "Player", probe, error)) return reject(error);
    return true;
}

FileStatus evaluatePlan(const VersionPlan &plan, const std::atomic_bool *cancel,
                        const std::function<void(int, int)> &progress) {
    FileStatus status;
    for (const auto &file : plan.files) {
        if (cancel && *cancel) break;
        if (!QFileInfo::exists(file.path)) {
            ++status.missing;
            if (status.examples.size() < 5) status.examples << QDir(plan.root).relativeFilePath(file.path);
        } else if (!validFile(file, cancel)) {
            ++status.damaged;
            if (status.examples.size() < 5) status.examples << QDir(plan.root).relativeFilePath(file.path);
        }
        ++status.checked;
        if (progress) progress(status.checked, plan.files.size() + plan.archives.size());
    }
    // ZIP 本体的 SHA-1 正确仍不足以证明 native 可用，还需按中央目录逐项核对解压文件的长度与 CRC。
    for (const auto &archive : plan.archives) {
        if (cancel && *cancel) break;
        QFile file(archive.file.path);
        if (file.open(QIODevice::ReadOnly) && validFile(archive.file, cancel)) {
            const QByteArray bytes = file.readAll();
            int end = -1;
            for (int i = bytes.size() - 22; i >= qMax(0, bytes.size() - 65557); --i)
                if (u32(bytes, i) == 0x06054b50 && i + 22 + u16(bytes, i + 20) == bytes.size()) { end = i; break; }
            bool healthy = end >= 0 && u16(bytes, end + 4) == 0 && u16(bytes, end + 6) == 0;
            if (healthy) {
                quint64 pos = u32(bytes, end + 16);
                for (int i = 0; i < u16(bytes, end + 10) && healthy; ++i) {
                    if (pos + 46 > quint64(bytes.size()) || u32(bytes, int(pos)) != 0x02014b50) { healthy = false; break; }
                    const int len = u16(bytes, int(pos + 28)), extra = u16(bytes, int(pos + 30)), comment = u16(bytes, int(pos + 32));
                    if (pos + 46 + len + extra + comment > quint64(bytes.size())) { healthy = false; break; }
                    const QString name = QString::fromUtf8(bytes.constData() + pos + 46, len);
                    pos += 46 + len + extra + comment;
                    bool skip = name.endsWith('/') || name.startsWith("META-INF/", Qt::CaseInsensitive);
                    for (const QString &prefix : archive.excludes) if (name.startsWith(prefix, Qt::CaseInsensitive)) skip = true;
                    if (skip) continue;
                    const QString target = safePath(plan.natives, name);
                    QFile extracted(target);
                    if (target.isEmpty() || !extracted.open(QIODevice::ReadOnly) ||
                        extracted.size() != u32(bytes, int(pos - len - extra - comment - 46 + 24))) { healthy = false; break; }
                    const QByteArray plain = extracted.readAll();
                    if (crc32(0, reinterpret_cast<const Bytef *>(plain.constData()), plain.size()) !=
                        u32(bytes, int(pos - len - extra - comment - 46 + 16))) healthy = false;
                }
            }
            if (!healthy) {
                ++status.damaged;
                if (status.examples.size() < 5) status.examples << "natives/" + archive.file.sha1.left(8);
            }
        }
        ++status.checked;
        if (progress) progress(status.checked, plan.files.size() + plan.archives.size());
    }
    status.cancelled = cancel && *cancel;
    return status;
}

QString offlineUuid(const QString &name) {
    QByteArray bytes = QCryptographicHash::hash(("OfflinePlayer:" + name).toUtf8(), QCryptographicHash::Md5);
    bytes[6] = char((uchar(bytes[6]) & 0x0f) | 0x30);
    bytes[8] = char((uchar(bytes[8]) & 0x3f) | 0x80);
    const QString hex = bytes.toHex();
    return hex.mid(0, 8) + "-" + hex.mid(8, 4) + "-" + hex.mid(12, 4) + "-" + hex.mid(16, 4) + "-" + hex.mid(20);
}
bool launchArguments(const VersionPlan &plan, const QString &name, QStringList &arguments, QString &error) {
    const auto version = plan.metadata;
    const QMap<QString, QString> vars = {
        {"auth_player_name", name}, {"version_name", plan.id}, {"game_directory", plan.root},
        {"assets_root", QDir(plan.root).filePath("assets")}, {"assets_index_name", version.value("assetIndex").toObject().value("id").toString()},
        {"auth_uuid", offlineUuid(name)}, {"auth_access_token", "0"}, {"clientid", ""}, {"auth_xuid", ""},
        {"user_type", "legacy"}, {"version_type", "release"}, {"natives_directory", plan.natives},
        {"launcher_name", "NyakoMCL"}, {"launcher_version", "1.0"}, {"classpath", plan.classpath.join(QDir::listSeparator())}
    };
    arguments.clear();
    const auto source = version.value("arguments").toObject();
    bool allowed;
    if (!argumentValues(source.value("jvm").toArray(), vars, arguments)) { error = "JVM 参数含未知占位符或规则"; return false; }
    const auto logging = version.value("logging").toObject().value("client").toObject();
    if (!logging.isEmpty()) {
        const QString arg = logging.value("argument").toString();
        if (arg.count("${path}") != 1) { error = "日志参数格式不支持"; return false; }
        QString expanded = arg;
        expanded.replace("${path}", QUrl::fromLocalFile(QDir(plan.root).filePath(
            "assets/log_configs/" + logging.value("file").toObject().value("id").toString())).toString());
        arguments << expanded;
    }
    arguments << version.value("mainClass").toString();
    if (!argumentValues(source.value("game").toArray(), vars, arguments)) { error = "游戏参数含未知占位符或规则"; return false; }
    return true;
}
