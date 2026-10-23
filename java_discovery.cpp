#include "java_discovery.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

namespace {
QString executableName() {
#ifdef Q_OS_WIN
    return "java.exe";
#else
    return "java";
#endif
}

void appendChildren(QStringList &paths, const QString &parent) {
    QDir dir(parent);
    if (!dir.exists()) return;
    for (const QFileInfo &entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        paths << QDir(entry.absoluteFilePath()).filePath("bin/" + executableName());
#ifdef Q_OS_MACOS
        paths << QDir(entry.absoluteFilePath()).filePath("Contents/Home/bin/java");
#endif
    }
}
}

QString JavaRuntime::label(int requiredMajor) const {
    return QString("Java %1 (%2)  |  %3%4")
        .arg(version, architecture, QDir::toNativeSeparators(path),
             requiredMajor && !compatible(requiredMajor) ? QString("  [需要 Java %1]").arg(requiredMajor) : QString());
}

JavaRuntime inspectJava(const QString &path) {
    QFileInfo info(path);
    if (!info.isFile() || !info.isExecutable()) return {};
    JavaRuntime runtime;
    runtime.path = info.canonicalFilePath();
    if (runtime.path.isEmpty()) return {};
    QProcess process;
    process.start(runtime.path, {"-version"});
    if (!process.waitForStarted(2500)) return {};
    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished(1000);
        return {};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) return {};
    const QString output = QString::fromLocal8Bit(process.readAllStandardError() + process.readAllStandardOutput());
    const auto version = QRegularExpression("(?:java|openjdk) version \\\"([^\\\"]+)\\\"", QRegularExpression::CaseInsensitiveOption).match(output);
    if (!version.hasMatch()) return {};
    runtime.version = version.captured(1);
    const QString first = runtime.version.section('.', 0, 0);
    runtime.major = first == "1" ? runtime.version.section('.', 1, 1).toInt() : first.toInt();
    if (runtime.major <= 0) return {};
    runtime.architecture = output.contains(QRegularExpression("64[- ]?[Bb]it|aarch64|arm64|x86_64")) ? "64 位" : "32 位/未知";
    return runtime;
}

QList<JavaRuntime> discoverJava(const QString &preferredPath) {
    QStringList candidates;
    if (!preferredPath.isEmpty()) candidates << preferredPath;
    const auto environment = QProcessEnvironment::systemEnvironment();
    const QString home = environment.value("JAVA_HOME");
    if (!home.isEmpty()) candidates << QDir(home).filePath("bin/" + executableName());
    const QString separator = QDir::listSeparator();
    for (const QString &entry : environment.value("PATH").split(separator, Qt::SkipEmptyParts))
        candidates << QDir(entry).filePath(executableName());
#ifdef Q_OS_WIN
    for (const QString &base : {environment.value("ProgramFiles"), environment.value("ProgramFiles(x86)")}) {
        appendChildren(candidates, QDir(base).filePath("Java"));
        appendChildren(candidates, QDir(base).filePath("Eclipse Adoptium"));
        appendChildren(candidates, QDir(base).filePath("Microsoft"));
    }
    appendChildren(candidates, QDir::homePath() + "/.jdks");
#elif defined(Q_OS_MACOS)
    appendChildren(candidates, "/Library/Java/JavaVirtualMachines");
    appendChildren(candidates, QDir::homePath() + "/Library/Java/JavaVirtualMachines");
    appendChildren(candidates, QDir::homePath() + "/.jdks");
    candidates << "/usr/bin/java";
#else
    appendChildren(candidates, "/usr/lib/jvm");
    appendChildren(candidates, QDir::homePath() + "/.jdks");
    candidates << "/usr/bin/java";
#endif
    QList<JavaRuntime> result;
    QSet<QString> seen;
    for (const QString &candidate : candidates) {
        QFileInfo info(candidate);
        const QString key = info.canonicalFilePath();
        if (key.isEmpty() || seen.contains(key)) continue;
        seen.insert(key);
        JavaRuntime runtime = inspectJava(candidate);
        if (runtime.major > 0) result << runtime;
    }
    return result;
}
