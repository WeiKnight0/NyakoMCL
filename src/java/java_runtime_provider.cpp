#include "java_runtime_provider.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSysInfo>
#include <QTemporaryDir>

namespace {
QJsonObject read(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
}
QString platform() {
    const auto arch = QSysInfo::currentCpuArchitecture();
#ifdef Q_OS_WIN
    return arch == "arm64" ? "windows-arm64" : sizeof(void *) == 8 ? "windows-x64" : "windows-x86";
#elif defined(Q_OS_MACOS)
    return arch == "arm64" ? "mac-os-arm64" : "mac-os";
#else
    return sizeof(void *) == 8 ? "linux" : "linux-i386";
#endif
}
QString executable(const QString &root) {
#ifdef Q_OS_WIN
    return QDir(root).filePath("bin/java.exe");
#elif defined(Q_OS_MACOS)
    return QDir(root).filePath("jre.bundle/Contents/Home/bin/java");
#else
    return QDir(root).filePath("bin/java");
#endif
}
}
JavaRuntime provideJava(const QJsonObject &version, const QString &root,
                        const std::function<bool(const QString &, const QString &, const QString &)> &metadata,
                        const std::function<bool(const QList<Download> &)> &files,
                        const std::atomic_bool &cancel, QString &error,
                        const std::function<void(const QString &, const QString &)> &stage) {
    if (cancel) return {};
    const int major = version.value("majorVersion").toInt();
    if (major < 8 || major > 50) { error = QString("不支持的 Java major：%1，请手动选择兼容 Java").arg(major); return {}; }
    if (!QDir().mkpath(root)) { error = "无法创建 Java 运行时目录：" + root; return {}; }
    // 暂存放在发布根目录内，使最后一次重命名无需跨卷；任何提前返回均由临时目录清理未发布内容。
    QTemporaryDir staging(QDir(root).filePath(".staging-XXXXXX"));
    if (!staging.isValid()) { error = "无法创建 Java 暂存目录"; return {}; }
    if (stage) stage("java-index", "获取 Java 目录");
    const QString indexPath = staging.filePath("index.json");
    if (!metadata("https://piston-meta.mojang.com/v1/products/java-runtime/2ec0cc96c44e5a76b9c8b7c39df7210883d12871/all.json", indexPath, {})) return {};
    const QString component = version.value("component").toString();
    const auto entries = read(indexPath).value(platform()).toObject().value(component).toArray();
    if (entries.isEmpty() || component == "minecraft-java-exe") {
        error = "官方没有适配此平台的完整 " + component + "，请在设置中选择兼容 Java"; return {};
    }
    const auto descriptor = entries.first().toObject().value("manifest").toObject();
    const QString manifestPath = staging.filePath("manifest.json");
    const Download manifest{descriptor.value("url").toString(), descriptor.value("sha1").toString(), manifestPath,
                            qint64(descriptor.value("size").toDouble(-1))};
    if (!validDownload(manifest)) { error = "官方 Java manifest 描述无效"; return {}; }
    if (stage) stage("java-manifest", "获取 Java 文件清单");
    if (!files({manifest})) return {};
    const QString payload = staging.filePath("payload");
    const auto entriesByPath = read(manifestPath).value("files").toObject();
    QList<Download> downloads;
    QStringList executables;
    QList<QPair<QString, QString>> links;
    for (auto it = entriesByPath.begin(); it != entriesByPath.end(); ++it) {
        if (cancel) return {};
        const QString path = safePath(payload, it.key());
        const auto item = it.value().toObject();
        const QString type = item.value("type").toString();
        if (path.isEmpty()) { error = "Java manifest 包含不安全路径：" + it.key(); return {}; }
        if (type == "directory") {
            if (!QDir().mkpath(path)) { error = "无法创建 Java 目录：" + path; return {}; }
        } else if (type == "file") {
            const auto raw = item.value("downloads").toObject().value("raw").toObject();
            Download download{raw.value("url").toString(), raw.value("sha1").toString(), path, qint64(raw.value("size").toDouble(-1))};
            if (!validDownload(download)) { error = "Java 文件缺少 raw 校验元数据：" + it.key(); return {}; }
            downloads.append(download);
            if (item.value("executable").toBool()) executables.append(path);
        } else if (type == "link") {
            const QString target = item.value("target").toString();
            const QString resolved = QDir::cleanPath(QFileInfo(path).dir().filePath(target));
            if (QDir::isAbsolutePath(target) || target.contains('\\') || target.contains(':') || !resolved.startsWith(payload + '/')) {
                error = "Java 链接越界：" + it.key(); return {};
            }
            links.append({path, resolved});
        } else { error = "不支持的 Java manifest 类型：" + type; return {}; }
    }
    if (downloads.isEmpty()) { error = "官方 Java manifest 没有文件"; return {}; }
    if (stage) stage("java-files", "下载官方 Java 文件");
    if (!files(downloads)) return {};
    if (stage) stage("java-verify", "检查 Java 运行时");
    for (const auto &path : executables)
        if (!QFile::setPermissions(path, QFile::permissions(path) | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther)) {
            error = "无法设置 Java 可执行权限：" + path; return {};
        }
    for (const auto &link : links)
        if (!QFile::link(link.second, link.first)) { error = "无法创建 Java 链接：" + link.first; return {}; }
    const auto runtime = inspectJava(executable(payload), &cancel);
    if (cancel) return {};
    if (!runtime.compatible(version.value("majorVersion").toInt())) { error = "官方下载的 Java 探测失败或版本不兼容"; return {}; }
    // 用组件与清单摘要区分发布版本；已存在目录只探测复用，拒绝覆盖损坏目录。
    // 文件校验、权限、链接和实际 Java 探测全部完成后才将 payload 暴露给后续扫描。
    const QString destination = safePath(root, component + "-" + manifest.sha1);
    if (destination.isEmpty()) { error = "官方 Java 组件名称无效"; return {}; }
    if (QDir(destination).exists()) {
        const auto existing = inspectJava(executable(destination), &cancel);
        if (existing.compatible(runtime.major)) return existing;
        error = "已发布 Java 运行时损坏，请移除后重试：" + destination; return {};
    }
    if (stage) stage("java-publish", "发布官方 Java");
    if (cancel) return {};
    if (!QDir().rename(payload, destination)) { error = "无法发布 Java 运行时：" + destination; return {}; }
    return inspectJava(executable(destination), &cancel);
}
