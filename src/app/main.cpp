#include "app/cli.h"
#include "launcher_controller.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

/** @description 有额外参数时进入无界面的命令行模式，否则建立 QML 界面及主线程控制器。
 * @param argc 参数数量；argv 操作系统传入的参数数组。
 * @throws 无主动异常；命令行透传退出码，QML 根对象加载失败返回 1，界面返回事件循环退出码。 */
int main(int argc, char **argv) {
    if (argc > 1) {
        QCoreApplication app(argc, argv);
        app.setApplicationName("NyakoMCL");
        return runCli(app);
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName("NyakoMCL");
    LauncherController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("launcher", &controller);
    engine.loadFromModule("NyakoMCL", "Main");
    if (engine.rootObjects().isEmpty()) return 1;
    return app.exec();
}
