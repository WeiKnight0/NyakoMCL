#pragma once

class QCoreApplication;

/** @description 分派 Java 列举、安装、检查和离线启动命令，向标准输出/错误输出报告结果。
 * @param app 持有参数与安装任务事件循环的应用。
 * @throws 无主动异常；用法错误返回 2，列举/安装/检查失败通常返回 1；
 * 启动预检、Java、参数、进程启动失败分别返回 2/3/4/5，游戏退出后透传其退出码。 */
int runCli(QCoreApplication &app);
