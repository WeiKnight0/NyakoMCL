#pragma once

#include <QList>
#include <QString>

/** @description 保存实例的稳定身份、独立目录及离线玩家和 Java 偏好。
 * @param 无；pending 仅表示登记后的待安装状态，不替代磁盘完整性检查。
 * @throws 无主动异常。 */
struct InstanceRecord {
    QString id;
    QString name;
    QString versionId;
    QString directory;
    QString javaPath;
    bool pending = true;
    QString playerName = "Player";
};

/** @description 管理实例登记，先原子保存候选记录，再替换内存状态；调用方负责串行访问。
 * @param 无；登记文件损坏时须保留原文件并停止写入。
 * @throws 无主动异常；操作失败使用 bool 和 error 报告。 */
class InstanceStore {
public:
    /** @description 选择登记位置，构造时不读取或创建文件。
     * @param filePath 登记文件路径；空值使用应用数据目录的 instances.json。
     * @throws 无主动异常。 */
    explicit InstanceStore(QString filePath = {});

    /** @description 读取当前内存记录的只读视图，修改操作后引用可能失效。
     * @param 无。
     * @throws 无主动异常。 */
    const QList<InstanceRecord> &instances() const { return instances_; }
    /** @description 读取持久化选择的稳定 ID，空值代表未选择。
     * @param 无。
     * @throws 无主动异常。 */
    const QString &selectedId() const { return selectedId_; }
    /** @description 读取当前登记文件的路径供诊断使用。
     * @param 无。
     * @throws 无主动异常。 */
    const QString &filePath() const { return filePath_; }
    /** @description 按稳定 ID 查找记录；后续修改可能使返回指针失效。
     * @param id 实例登记 ID。
     * @throws 无主动异常；未找到返回 nullptr。 */
    const InstanceRecord *find(const QString &id) const;

    /** @description 校验并载入完整登记；文件不存在视为空登记，失败后禁用保存。
     * @param error 接收读取、格式或记录校验失败原因。
     * @throws 无主动异常；失败返回 false。 */
    bool load(QString &error);
    /** @description 原子保存已成功加载的登记，拒绝替换符号链接文件。
     * @param error 接收写入失败原因。
     * @throws 无主动异常；未加载或保存失败返回 false。 */
    bool save(QString &error) const;
    /** @description 登记隔离的新实例，不覆盖非空目录，成功后才公开新 ID。
     * @param name 显示名；versionId 官方版本标识；directory 独立绝对目录；javaPath 初始 Java 路径。
     * @param newId 接收稳定 ID；error 接收失败原因；playerName 初始离线玩家名。
     * @throws 无主动异常；校验或保存失败返回 false 且不替换内存记录。 */
    bool add(QString name, QString versionId, QString directory, QString javaPath,
             QString &newId, QString &error, QString playerName = "Player");
    /** @description 更新已有身份的记录，迁移目录时重新执行空目录与可写性检查。
     * @param record 完整候选记录；error 接收失败原因。
     * @throws 无主动异常；身份不存在、校验或保存失败返回 false。 */
    bool update(const InstanceRecord &record, QString &error);
    /** @description 持久化当前选择，允许清空选择。
     * @param id 已登记 ID 或空字符串；error 接收失败原因。
     * @throws 无主动异常；未知 ID 或保存失败返回 false。 */
    bool select(const QString &id, QString &error);
    /** @description 修改显示名而保留身份和磁盘目录。
     * @param id 已登记 ID；name 新显示名；error 接收失败原因。
     * @throws 无主动异常；查找、校验或保存失败返回 false。 */
    bool rename(const QString &id, const QString &name, QString &error);
    /** @description 保存实例的 Java 偏好；可执行性由调用方探测。
     * @param id 已登记 ID；javaPath Java 路径；error 接收失败原因。
     * @throws 无主动异常；查找或保存失败返回 false。 */
    bool setJavaPath(const QString &id, const QString &javaPath, QString &error);
    /** @description 持久化待安装标记，不执行文件完整性检查。
     * @param id 已登记 ID；pending 待安装状态；error 接收失败原因。
     * @throws 无主动异常；查找或保存失败返回 false。 */
    bool setPending(const QString &id, bool pending, QString &error);

private:
    bool commit(const QList<InstanceRecord> &records, const QString &selectedId, QString &error);
    bool validate(const InstanceRecord &record, const QList<InstanceRecord> &records,
                  bool newDirectory, QString &error) const;
    bool loaded_ = false;
    QString filePath_;
    QList<InstanceRecord> instances_;
    QString selectedId_;
};
