#pragma once

#include "instance/instance_store.h"
#include "java/java_discovery.h"
#include "version/version_status.h"

#include <QFutureWatcher>
#include <QObject>
#include <QProcess>
#include <QVariantList>
#include <QUrl>
#include <memory>

class Installer;

/** @description 在主线程协调实例持久化、后台准备与游戏进程，向 QML 提供状态快照。
 * @param 无；界面入口及 getter 应在对象所属线程调用。
 * @throws 无主动异常；业务失败写入 error 并发出 stateChanged。 */
class LauncherController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool storeReady READ storeReady NOTIFY stateChanged)
    Q_PROPERTY(QVariantList instances READ instances NOTIFY instancesChanged)
    Q_PROPERTY(QString selectedId READ selectedId NOTIFY stateChanged)
    Q_PROPERTY(QVariantList versions READ versions NOTIFY versionsChanged)
    Q_PROPERTY(QVariantList javaRuntimes READ javaRuntimes NOTIFY javaChanged)
    Q_PROPERTY(QString selectedName READ selectedName NOTIFY stateChanged)
    Q_PROPERTY(QString selectedVersion READ selectedVersion NOTIFY stateChanged)
    Q_PROPERTY(QString selectedDirectory READ selectedDirectory NOTIFY stateChanged)
    Q_PROPERTY(QString javaPath READ javaPath NOTIFY stateChanged)
    Q_PROPERTY(int requiredJava READ requiredJava NOTIFY stateChanged)
    Q_PROPERTY(bool javaCompatible READ javaCompatible NOTIFY stateChanged)
    Q_PROPERTY(QString playerName READ playerName WRITE setPlayerName NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString log READ log NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(bool checking READ checking NOTIFY stateChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY stateChanged)
    Q_PROPERTY(bool loadingVersions READ loadingVersions NOTIFY stateChanged)
    Q_PROPERTY(bool installed READ installed NOTIFY stateChanged)
    Q_PROPERTY(int progress READ progress NOTIFY stateChanged)
    Q_PROPERTY(int progressTotal READ progressTotal NOTIFY stateChanged)
public:
    /** @description 加载登记并接线主线程回调，开始版本清单、Java 扫描和所选实例检查。
     * @param parent QObject 所有者。
     * @throws 无主动异常；登记加载失败经 error 报告并禁止写入。 */
    explicit LauncherController(QObject *parent = nullptr);
    /** @description 请求取消并等待所有后台任务，结束所属游戏进程，防止回调访问已销毁状态。
     * @param 无。
     * @throws 无主动异常。 */
    ~LauncherController() override;

    /** @description 判断登记是否成功加载，从而允许界面写入。
     * @param 无。
     * @throws 无主动异常。 */
    bool storeReady() const { return storeReady_; }
    /** @description 返回适用于 QML 的实例快照，使用 ID 而非显示名标识实例。
     * @param 无。
     * @throws 无主动异常。 */
    QVariantList instances() const;
    /** @description 读取当前选择的稳定 ID，空值代表未选择。
     * @param 无。
     * @throws 无主动异常。 */
    QString selectedId() const { return store_.selectedId(); }
    /** @description 返回已加载的版本选择列表，加载期间仍可能是旧快照。
     * @param 无。
     * @throws 无主动异常。 */
    QVariantList versions() const { return versions_; }
    /** @description 返回探测到的 Java 列表及针对当前需求的展示标签。
     * @param 无。
     * @throws 无主动异常。 */
    QVariantList javaRuntimes() const { return javaRuntimes_; }
    /** @description 读取所选实例显示名，未选择时为空。
     * @param 无。
     * @throws 无主动异常。 */
    QString selectedName() const;
    /** @description 读取所选实例固定的版本标识，未选择时为空。
     * @param 无。
     * @throws 无主动异常。 */
    QString selectedVersion() const;
    /** @description 读取所选实例的独立数据目录，未选择时为空。
     * @param 无。
     * @throws 无主动异常。 */
    QString selectedDirectory() const;
    /** @description 读取实例保存的 Java 偏好路径，未选择时为空。
     * @param 无。
     * @throws 无主动异常。 */
    QString javaPath() const;
    /** @description 读取最近解析计划的 Java 主版本，尚无计划时为零。
     * @param 无。
     * @throws 无主动异常。 */
    int requiredJava() const { return current_.plan.javaMajor; }
    /** @description 判断保存路径是否对应已探测且主版本精确匹配的 Java。
     * @param 无。
     * @throws 无主动异常。 */
    bool javaCompatible() const;
    /** @description 读取所选实例的离线玩家名，未选择时为空。
     * @param 无。
     * @throws 无主动异常。 */
    QString playerName() const;
    /** @description 将文件对话框 URL 转换为本地路径，避免直接使用 URL 文本作为文件名。
     * @param url 用户选择的文件或目录 URL。
     * @throws 无主动异常；非本地 URL 返回空路径。 */
    Q_INVOKABLE QString localFilePath(const QUrl &url) const { return url.toLocalFile(); }
    /** @description 即时保存实例离线身份；最终名称规则在启动时校验。
     * @param name 正在编辑的离线玩家名。
     * @throws 无主动异常；忙碌或无实例时忽略，保存失败经 error 报告。 */
    void setPlayerName(const QString &name);
    /** @description 读取供界面展示的最近阶段或完成提示。
     * @param 无。
     * @throws 无主动异常。 */
    QString status() const { return status_; }
    /** @description 读取当前业务错误，空值表示尚无错误提示。
     * @param 无。
     * @throws 无主动异常。 */
    QString error() const { return error_; }
    /** @description 读取限长的任务与游戏合并日志。
     * @param 无。
     * @throws 无主动异常。 */
    QString log() const { return log_; }
    /** @description 判断准备线程尚未清理或游戏进程正在启动/运行；不含独立检查和扫描。
     * @param 无。
     * @throws 无主动异常。 */
    bool busy() const { return worker_ || running(); }
    /** @description 判断 QProcess 是否尚未回到 NotRunning，包含启动阶段。
     * @param 无。
     * @throws 无主动异常。 */
    bool running() const { return game_.state() != QProcess::NotRunning; }
    /** @description 判断当前实例的后台文件检查是否有效进行中。
     * @param 无。
     * @throws 无主动异常。 */
    bool checking() const { return checking_; }
    /** @description 判断 Java 扫描或手动路径探测是否尚未完成。
     * @param 无。
     * @throws 无主动异常。 */
    bool scanning() const { return scanning_; }
    /** @description 判断版本清单后台加载是否尚未完成。
     * @param 无。
     * @throws 无主动异常。 */
    bool loadingVersions() const { return loadingVersions_; }
    /** @description 返回最近检查的可启动结论，检查期间不公开旧的已安装状态。
     * @param 无。
     * @throws 无主动异常。 */
    bool installed() const { return !checking_ && current_.installed; }
    /** @description 读取当前阶段已完成的数量，不是跨阶段累计值。
     * @param 无。
     * @throws 无主动异常。 */
    int progress() const { return progress_; }
    /** @description 读取当前阶段总量，零代表界面应展示不定进度。
     * @param 无。
     * @throws 无主动异常。 */
    int progressTotal() const { return progressTotal_; }

    /** @description 后台刷新官方版本列表，已有刷新进行时忽略重复请求。
     * @param 无。
     * @throws 无主动异常；网络与缓存提示写入日志。 */
    Q_INVOKABLE void refreshVersions();
    /** @description 持久化选择并使旧检查失效，再检查新实例。
     * @param id 已登记 ID 或空值。
     * @throws 无主动异常；忙碌或扫描时忽略，保存失败经 error 报告。 */
    Q_INVOKABLE void selectInstance(const QString &id);
    /** @description 登记独立空目录的现代正式版实例，选择后自动开始安装。
     * @param name 显示名；version 1.13 起版本标识；directory 完整绝对目标目录。
     * @throws 无主动异常；不满足入口状态时忽略，校验或持久化失败经 error 报告。 */
    Q_INVOKABLE void createInstance(const QString &name, const QString &version, const QString &directory);
    /** @description 修改当前实例显示名，保留稳定身份与数据目录。
     * @param name 新显示名，保存前去除首尾空白。
     * @throws 无主动异常；无实例或忙碌时忽略，保存失败经 error 报告。 */
    Q_INVOKABLE void renameSelected(const QString &name);
    /** @description 异步校验当前实例；代际标记防止旧结果覆盖新选择。
     * @param 无。
     * @throws 无主动异常；忙碌时忽略，检查失败经状态提示报告。 */
    Q_INVOKABLE void checkSelected();
    /** @description 启动当前实例安装或修复，成功后保存待安装状态。
     * @param 无。
     * @throws 无主动异常；无可用实例或忙碌时忽略，任务失败经 error 报告。 */
    Q_INVOKABLE void installSelected();
    /** @description 后台探测本地 Java，完成后尝试为当前实例推荐精确兼容版本。
     * @param 无。
     * @throws 无主动异常；扫描或任务忙碌时忽略。 */
    Q_INVOKABLE void scanJava();
    /** @description 后台验证用户路径并保存至当前实例，不强制主版本兼容。
     * @param path 用户选择的 Java 可执行文件。
     * @throws 无主动异常；无效可执行文件或保存失败经 error 报告。 */
    Q_INVOKABLE void chooseJava(const QString &path);
    /** @description 校验离线名称后启动后台修复、Java 选择和参数准备，finished 后在主线程创建进程。
     * @param 无。
     * @throws 无主动异常；名称、准备或进程失败经 error 报告。 */
    Q_INVOKABLE void launchSelected();
    /** @description 请求取消准备/检查/扫描，或终止正在运行的游戏；任务结束前保持忙碌状态。
     * @param 无。
     * @throws 无主动异常；协作取消不会同步等待线程结束。 */
    Q_INVOKABLE void cancel();
signals:
    /** @description 通知实例快照发生变化，界面应按 ID 重新匹配选择。
     * @param 无。
     * @throws 无主动异常。 */
    void instancesChanged();
    /** @description 通知版本清单快照已替换。
     * @param 无。
     * @throws 无主动异常。 */
    void versionsChanged();
    /** @description 通知 Java 列表或其兼容提示需重新读取。
     * @param 无。
     * @throws 无主动异常。 */
    void javaChanged();
    /** @description 通知属性快照需刷新；由对象所属线程发送。
     * @param 无。
     * @throws 无主动异常。 */
    void stateChanged();
private:
    void fail(const QString &message);
    void appendLog(const QString &message);
    void refreshInstances();
    void refreshJava();
    void recommendJava();
    void loadVersions(bool online);
    void startTask(bool launch);
    void stopChecks();
    std::shared_ptr<std::atomic_bool> checkCancel_ = std::make_shared<std::atomic_bool>(false);
    std::shared_ptr<std::atomic_bool> scanCancel_ = std::make_shared<std::atomic_bool>(false);
    std::atomic_bool catalogCancel_{false};
    bool taskCancelled_ = false, gameStopped_ = false;
    QString gameName_;
    QString manualJava_;
    const InstanceRecord *selected() const { return store_.find(store_.selectedId()); }

    InstanceStore store_;
    QProcess game_;
    Installer *worker_ = nullptr;
    QList<QFutureWatcher<VersionStatus> *> checks_;
    QFutureWatcher<QPair<QStringList, QString>> versionsWatcher_;
    QFutureWatcher<QList<JavaRuntime>> javaWatcher_;
    QList<JavaRuntime> runtimes_;
    QVariantList versions_, javaRuntimes_;
    VersionStatus current_;
    QString status_, error_, log_;
    int progress_ = 0, progressTotal_ = 1;
    quint64 generation_ = 0;
    bool storeReady_ = false, checking_ = false, scanning_ = false, loadingVersions_ = false;
};
