#pragma once

#include "java_discovery.h"
#include "version/version_model.h"
#include <QJsonObject>
#include <functional>

/** @description 在同根暂存目录获取完整官方 Java，校验并探测兼容后重命名发布。
 * @param version 版本元数据的 Java 主版本与组件要求。
 * @param root 应用管理的运行时目录，不是实例目录。
 * @param metadata 在当前工作线程下载元数据的回调；files 校验并下载文件批次的回调。
 * @param cancel 协作取消标记；error 接收失败原因，取消时可能保持原值。
 * @param stage 发布前在调用线程报告阶段的可选回调。
 * @throws 无主动异常；失败或取消返回无效运行时，回调异常不在此捕获。 */
JavaRuntime provideJava(const QJsonObject &version, const QString &root,
                        const std::function<bool(const QString &, const QString &, const QString &)> &metadata,
                        const std::function<bool(const QList<Download> &)> &files,
                        const std::atomic_bool &cancel, QString &error,
                        const std::function<void(const QString &)> &stage = {});
