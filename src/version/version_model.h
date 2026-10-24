#pragma once

#include <QJsonObject>
#include <QStringList>
#include <QList>
#include <atomic>
#include <functional>

/** @description 将下载来源、目标路径和完整性约束绑定为一个校验单元。
 * @param 无；size 为 -1 表示尚未提供长度，不能通过 validDownload。
 * @throws 无主动异常；由校验函数返回有效性。 */
struct Download {
    QString url, sha1, path;
    qint64 size = -1;
};
/** @description 记录需解压并校验的原生库及官方前缀排除规则。
 * @param 无。
 * @throws 无主动异常。 */
struct NativeArchive {
    Download file;
    QStringList excludes;
};
/** @description 保存现代独立正式版的文件计划与参数展开上下文，不表示文件已安装。
 * @param 无；所有目标路径归属于 root 对应的实例。
 * @throws 无主动异常。 */
struct VersionPlan {
    QString id, root, natives;
    int javaMajor = 0;
    QJsonObject metadata;
    QList<Download> files;
    QList<NativeArchive> archives;
    QStringList classpath;
};
/** @description 汇总文件及已解压原生库检查结果，取消时保留已完成的统计。
 * @param 无。
 * @throws 无主动异常。 */
struct FileStatus {
    int missing = 0, damaged = 0, checked = 0;
    QStringList examples;
    bool cancelled = false;
    /** @description 判断本次检查是否完整完成且没有缺失或损坏。
     * @param 无。
     * @throws 无主动异常。 */
    bool ready() const { return !cancelled && missing == 0 && damaged == 0; }
};

/** @description 限制版本标识为可用于目录名的字符和长度。
 * @param id 外部版本标识。
 * @throws 无主动异常；不合法返回 false。 */
bool validVersionId(const QString &id);
/** @description 生成实例内版本元数据的位置。
 * @param root 实例根目录；id 版本标识。
 * @throws 无主动异常；标识不合法返回空字符串。 */
QString versionFile(const QString &root, const QString &id);
/** @description 拒绝绝对路径、盘符及路径穿越；仅做词法检查，不解析符号链接。
 * @param base 可信目标根目录；relative 外部提供的斜杠分隔相对路径。
 * @throws 无主动异常；路径不安全返回空字符串。 */
QString safePath(const QString &base, const QString &relative);
/** @description 检查 HTTPS 来源、SHA-1、长度和目标路径是否齐备，不访问网络或磁盘。
 * @param item 待下载文件描述。
 * @throws 无主动异常；描述不合法返回 false。 */
bool validDownload(const Download &item);
/** @description 按长度和流式 SHA-1 验证本地文件。
 * @param item 文件位置及预期完整性；cancel 可选的协作取消标记。
 * @throws 无主动异常；不可读、校验失败或取消返回 false。 */
bool validFile(const Download &item, const std::atomic_bool *cancel = nullptr);
/** @description 将受支持的独立现代正式版元数据解析成实例内计划，并预检参数规则。
 * @param version 版本 JSON；root 实例目录；id 预期版本；index 资源索引 JSON。
 * @param plan 接收计划，失败时可能只填充部分内容；error 接收不支持的原因。
 * @throws 无主动异常；未知规则、路径或元数据不完整返回 false。 */
bool buildVersionPlan(const QJsonObject &version, const QString &root, const QString &id,
                      const QJsonObject &index, VersionPlan &plan, QString &error);
/** @description 检查下载文件的 SHA-1 及原生库解压结果的长度和 CRC。
 * @param plan 已解析计划；cancel 可选取消标记；progress 在调用线程报告已检查数和总数。
 * @throws 无主动异常；失败计入返回统计，取消通过 cancelled 表示。 */
FileStatus evaluatePlan(const VersionPlan &plan, const std::atomic_bool *cancel = nullptr,
                        const std::function<void(int, int)> &progress = {});
/** @description 为离线身份展开 JVM 与游戏参数，保留参数列表边界供 QProcess 使用。
 * @param plan 版本计划；name 离线玩家名；arguments 接收参数，失败时可能部分写入；error 接收原因。
 * @throws 无主动异常；未知占位符或规则返回 false。 */
bool launchArguments(const VersionPlan &plan, const QString &name, QStringList &arguments, QString &error);
/** @description 按 Minecraft 的 OfflinePlayer 命名空间生成确定性的离线 UUID。
 * @param name 玩家名，大小写参与身份计算。
 * @throws 无主动异常。 */
QString offlineUuid(const QString &name);
