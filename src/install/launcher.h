#pragma once

#include "version/version_status.h"
#include "java/java_discovery.h"
#include <QMutex>
#include <QNetworkAccessManager>
#include <QThread>
#include <QStringList>
#include <QVariantMap>
#include <atomic>

/** @description 在工作线程安装原版或准备冻结的离线启动输入，不在该线程启动游戏。
 * @param 无；QThread 对象仍归创建线程，结果应在 finished 后读取。
 * @throws 无主动异常；业务失败通过完成消息和结果状态报告。 */
class Installer : public QThread {
    Q_OBJECT
public:
    /** @description 固定任务所属目录与版本，构造后尚未开始工作。
     * @param root 实例目录；id 官方版本标识；parent QObject 所有者。
     * @throws 无主动异常；版本校验在任务内进行。 */
    explicit Installer(QString root, QString id, QObject *parent = nullptr);
    /** @description 请求协作取消，下载轮询和校验循环观察原子标记后退出。
     * @param 无。
     * @throws 无主动异常；调用不等待工作线程结束。 */
    void cancel();
    /** @description 在 start 前冻结离线启动所需输入，避免后台读取可变界面状态。
     * @param player 本任务离线身份；preferred 已保存的 Java 候选；runtimeRoot 托管运行时目录。
     * @throws 无主动异常；必须在工作开始前调用。 */
    void prepareLaunch(QString player, QString preferred, QString runtimeRoot);
    /** @description 读取任务业务结果；还须结合取消状态判断能否启动。
     * @param 无；应在 finished 后读取。
     * @throws 无主动异常。 */
    bool succeeded() const { return success_; }
    /** @description 查询取消请求是否已发出，不表示线程已经退出。
     * @param 无。
     * @throws 无主动异常。 */
    bool cancelled() const { return cancelled_; }
    /** @description 读取最终阶段及失败或取消提示。
     * @param 无；应在 finished 后读取。
     * @throws 无主动异常。 */
    QString message() const { return message_; }
    /** @description 读取安装或准备后的文件检查结果。
     * @param 无；应在 finished 后读取，失败结果可能不完整。
     * @throws 无主动异常。 */
    VersionStatus result() const { return result_; }
    /** @description 读取启动准备选中的兼容 Java，安装任务不填充该结果。
     * @param 无；应在 finished 后读取。
     * @throws 无主动异常。 */
    JavaRuntime runtime() const { return runtime_; }
    /** @description 读取已按离线身份展开的启动参数列表。
     * @param 无；应在启动准备成功并 finished 后读取。
     * @throws 无主动异常。 */
    QStringList arguments() const { return arguments_; }
    /** @description 在互斥锁内读取真实业务阶段与响应体累计量，供主线程低频采样。
     * @param 无；可在工作线程运行期间调用。
     * @throws 无主动异常。 */
    QVariantMap taskSnapshot() const;
signals:
    /** @description 从工作线程或下载池报告进度，界面接收方应排队处理。
     * @param done 已完成数量；total 总数，零表示阶段未定量；detail 当前阶段或文件。
     * @throws 无主动异常。 */
    void progress(int done, int total, QString detail);
    /** @description 在 run 返回前报告业务完成；线程生命周期结束仍以 finished 为准。
     * @param success 成功且未取消；message 最终说明。
     * @throws 无主动异常。 */
    void completed(bool success, QString message);
protected:
    void run() override;
private:
    bool fetch(const Download &item, QNetworkAccessManager &network);
    bool fetchMetadata(QNetworkAccessManager &network, const QString &url, const QString &path, const QString &sha1 = {});
    bool install(QNetworkAccessManager &network);
    bool valid(const Download &item) const;
    bool extractNatives(const QString &archive, const QStringList &excludes, const QString &destination);
    void beginStage(const QString &id, const QString &label, int total = 0);
    void updateTask(int done, int total, const QString &detail);
    void received(qint64 bytes);
    void retry(const QString &file, int attempt);
    mutable QMutex taskMutex_;
    QVariantMap snapshot_;
    QVariantList stages_;
    QString root_, id_;
    std::atomic_bool cancelled_{false};
    std::atomic_bool failed_{false};
    std::atomic_int done_{0};
    int total_ = 0;
    QMutex errorMutex_;
    bool fetchFiles(const QList<Download> &files);
    bool prepare(QNetworkAccessManager &network);
    QString error_, message_, player_, preferred_, runtimeRoot_, stage_;
    bool launching_ = false, success_ = false;
    VersionStatus result_;
    JavaRuntime runtime_;
    QStringList arguments_;
};
