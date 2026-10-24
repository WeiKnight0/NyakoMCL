#include "cli.h"

#include "install/launcher.h"
#include "java/java_discovery.h"
#include "version/version_status.h"

#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QRegularExpression>
#include <cstdio>

int runCli(QCoreApplication &app) {
    const QStringList args = app.arguments();
    if (args.size() == 2 && args.at(1) == "--list-java") {
        const auto runtimes = discoverJava({});
        for (const auto &runtime : runtimes) std::fprintf(stdout, "%s\n", runtime.label().toUtf8().constData());
        return runtimes.isEmpty() ? 1 : 0;
    }
    if (args.size() == 4 && args.at(1) == "--install") {
        if (!validVersionId(args.at(3))) return 2;
        Installer installer(args.at(2), args.at(3));
        QObject::connect(&installer, &Installer::progress, &app, [](int done, int total, const QString &detail) {
            std::fprintf(stdout, "%d/%d %s\n", done, total, detail.toUtf8().constData()); std::fflush(stdout);
        });
        int result = 1;
        QObject::connect(&installer, &Installer::completed, &app, [&](bool success, const QString &message) {
            std::fprintf(stdout, "%s\n", message.toUtf8().constData()); std::fflush(stdout);
            result = success ? 0 : 1;
        });
        // completed 先排队更新业务退出码，finished 再退出事件循环；wait 保证栈上线程对象安全销毁。
        QObject::connect(&installer, &QThread::finished, &app, &QCoreApplication::quit);
        installer.start(); app.exec(); installer.wait(); return result;
    }
    if (args.size() == 4 && args.at(1) == "--check") {
        const auto status = inspectVersion(args.at(2), args.at(3), false);
        std::fprintf(stdout, "%s: %s (missing=%d damaged=%d checked=%d Java=%d)\n",
                     args.at(3).toUtf8().constData(), status.message.toUtf8().constData(),
                     status.files.missing, status.files.damaged, status.files.checked, status.plan.javaMajor);
        return status.installed ? 0 : 1;
    }
    if (args.size() == 6 && args.at(1) == "--launch") {
        if (!QRegularExpression("^[A-Za-z0-9_]{3,16}$").match(args.at(5)).hasMatch()) {
            std::fprintf(stderr, "离线名字须为 3-16 位字母、数字或下划线\n"); return 2;
        }
        const QString root = QDir(args.at(2)).absolutePath();
        const auto status = inspectVersion(root, args.at(3), false);
        if (!status.installed) { std::fprintf(stderr, "%s\n", status.message.toUtf8().constData()); return 2; }
        const auto java = inspectJava(args.at(4));
        if (!java.compatible(status.plan.javaMajor)) {
            std::fprintf(stderr, "需要 Java %d，当前 Java %d\n", status.plan.javaMajor, java.major); return 3;
        }
        QStringList command; QString error;
        if (!launchArguments(status.plan, args.at(5), command, error)) return 4;
        QProcess game;
        game.setWorkingDirectory(root); game.setProcessChannelMode(QProcess::MergedChannels);
        game.start(java.path, command);
        if (!game.waitForStarted(10000)) return 5;
        while (game.waitForReadyRead(1000) || game.state() != QProcess::NotRunning) {
            const QByteArray output = game.readAll();
            if (!output.isEmpty()) { std::fwrite(output.constData(), 1, output.size(), stdout); std::fflush(stdout); }
        }
        return game.exitCode();
    }
    std::fprintf(stderr, "用法: NyakoMCL --list-java | --install <数据目录> <版本ID> | --check <数据目录> <版本ID> | --launch <数据目录> <版本ID> <java路径> <离线名字>\n");
    return 2;
}
