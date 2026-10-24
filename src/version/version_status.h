#pragma once
#include "version_model.h"

/** @description 区分元数据可解析与磁盘可启动状态，并保留修复提示及检查统计。
 * @param 无；supported 不代表 installed，取消时 installed 保持 false。
 * @throws 无主动异常。 */
struct VersionStatus {
    VersionPlan plan;
    FileStatus files;
    QString message;
    bool supported = false;
    bool installed = false;
};
/** @description 同步检查实例元数据和文件；在线获取只用于检查，不替代安装落盘。
 * @param root 实例目录；id 版本标识；online 是否尝试获取远端版本和资源索引。
 * @param progress 在调用线程报告检查进度；cancel 可选协作取消标记。
 * @throws 无主动异常；失败及取消通过返回状态的 message 和 installed 表示。 */
VersionStatus inspectVersion(const QString &root, const QString &id, bool online,
                             const std::function<void(int, int)> &progress = {},
                             const std::atomic_bool *cancel = nullptr);
/** @description 合并官方 1.13 起正式版清单和存在元数据的本地目录，保留离线选择入口。
 * @param root 清单缓存及本地版本目录；online 是否刷新官方清单；message 接收网络或缓存提示。
 * @param cancel 可选取消标记，影响网络请求。
 * @throws 无主动异常；网络失败回退缓存，本地补入的标识仍须后续检查是否受支持。 */
QStringList releaseVersions(const QString &root, bool online, QString &message,
                            const std::atomic_bool *cancel = nullptr);
