#pragma once

#include "instance/instance_store.h"
#include "java/java_discovery.h"
#include "version/version_status.h"
#include <QFutureWatcher>
#include <QObject>
#include <QProcess>
#include <QVariantList>
#include <QUrl>
#include <QTimer>
#include <QElapsedTimer>
#include <memory>

class Installer;

/** @description 主线程协调实例持久化、后台准备与游戏进程，向 QML 提供状态快照。
 * @param 无；所有界面入口在对象所属线程调用。
 * @throws 无主动异常；业务失败由状态属性报告。 */
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
    Q_PROPERTY(QString playerName READ playerName NOTIFY stateChanged)
    Q_PROPERTY(bool settingsAvailable READ settingsAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool settingsSaving READ settingsSaving NOTIFY stateChanged)
    Q_PROPERTY(QString settingsError READ settingsError NOTIFY stateChanged)
    Q_PROPERTY(QString settingsDisabledReason READ settingsDisabledReason NOTIFY stateChanged)
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
    Q_PROPERTY(QVariantMap task READ task NOTIFY taskChanged)
public:
    /** @description 读取独立于环境扫描状态的冻结任务快照。
     * @param 无。
     * @throws 无主动异常。 */
    QVariantMap task() const { return task_; }
    /** @description 按上次任务冻结的实例 ID 重走安装或启动入口，复用校验通过的文件。
     * @param 无。
     * @throws 无主动异常；冲突或实例丢失时通过错误提示报告。 */
    Q_INVOKABLE void retryTask();
    /** @description 将当前任务完整错误复制到系统剪贴板。
     * @param 无。
     * @throws 无主动异常。 */
    Q_INVOKABLE void copyTaskError();
    /** @description 关闭已经结束的任务卡，运行中拒绝关闭。
     * @param 无。
     * @throws 无主动异常。 */
    Q_INVOKABLE void dismissTask();
    /** @description 通知任务快照变化，供任务卡重新读取。
     * @param 无。
     * @throws 无主动异常。 */
    Q_SIGNAL void taskChanged();
    /** @description 每次任务结束仅发送一次非阻塞结果提示。
     * @param message 本次最终说明。
     * @throws 无主动异常。 */
    Q_SIGNAL void taskNotice(const QString &message);
    /** @description 要求新任务进入用户视野。
     * @param 无。
     * @throws 无主动异常。 */
    Q_SIGNAL void taskStarted();
    /** @description 加载登记，接线异步任务并开始环境检查。
     * @param parent QObject 所有者。
     * @throws 无主动异常；加载失败禁用写入。 */
    explicit LauncherController(QObject *parent = nullptr);
    /** @description 取消并等待后台任务，结束所属游戏进程。
     * @param 无。
     * @throws 无主动异常。 */
    ~LauncherController() override;
    /** @description 判断登记是否成功加载，防止覆盖无法读取的数据。
     * @param 无。
     * @throws 无主动异常。 */
    bool storeReady() const { return storeReady_; }
    /** @description 返回以稳定 ID 标识的实例展示快照。
     * @param 无。
     * @throws 无主动异常。 */
    QVariantList instances() const;
    /** @description 返回当前选中 ID，空值代表未选择。
     * @param 无。
     * @throws 无主动异常。 */
    QString selectedId() const { return store_.selectedId(); }
    /** @description 返回已加载的正式版清单，刷新时保留旧快照。
     * @param 无。
     * @throws 无主动异常。 */
    QVariantList versions() const { return versions_; }
    /** @description 返回本机 Java 中性列表，不绑定任一实例需求。
     * @param 无。
     * @throws 无主动异常。 */
    QVariantList javaRuntimes() const { return javaRuntimes_; }
    /** @description 读取所选实例显示名，未选择时为空。
     * @param 无。
     * @throws 无主动异常。 */
    QString selectedName() const;
    /** @description 读取所选实例固定的版本 ID，未选择时为空。
     * @param 无。
     * @throws 无主动异常。 */
    QString selectedVersion() const;
    /** @description 读取所选实例独立目录，未选择时为空。
     * @param 无。
     * @throws 无主动异常。 */
    QString selectedDirectory() const;
    /** @description 读取所选实例 Java 偏好，空值表示自动选择。
     * @param 无。
     * @throws 无主动异常。 */
    QString javaPath() const;
    /** @description 返回最近检查得到的 Java 主版本，尚未解析时为零。
     * @param 无。
     * @throws 无主动异常。 */
    int requiredJava() const { return current_.plan.javaMajor; }
    /** @description 判断保存路径是否对应已探测且主版本匹配的运行时。
     * @param 无。
     * @throws 无主动异常。 */
    bool javaCompatible() const;
    /** @description 读取所选实例离线玩家名，未选择时为空。
     * @param 无。
     * @throws 无主动异常。 */
    QString playerName() const;
    /** @description 判断当前任务状态是否允许打开或提交实例配置。
     * @param 无。
     * @throws 无主动异常。 */
    bool settingsAvailable() const { return storeReady_ && !busy() && !checking_ && !scanning_ && !settingsSaving_; }
    /** @description 判断本次配置是否正在验证 Java 或提交，防止重复保存。
     * @param 无。
     * @throws 无主动异常。 */
    bool settingsSaving() const { return settingsSaving_; }
    /** @description 读取配置保存错误，供保留草稿的窗口展示。
     * @param 无。
     * @throws 无主动异常。 */
    QString settingsError() const { return settingsError_; }
    /** @description 读取配置禁用原因，供不可操作时解释等待条件。
     * @param 无。
     * @throws 无主动异常。 */
    QString settingsDisabledReason() const;
    /** @description 读取最近业务阶段或完成提示。
     * @param 无。
     * @throws 无主动异常。 */
    QString status() const { return status_; }
    /** @description 读取当前业务错误，空值表示无错误。
     * @param 无。
     * @throws 无主动异常。 */
    QString error() const { return error_; }
    /** @description 读取限长任务与游戏合并日志。
     * @param 无。
     * @throws 无主动异常。 */
    QString log() const { return log_; }
    /** @description 判断准备线程或游戏进程仍占用实例，不含检查和扫描。
     * @param 无。
     * @throws 无主动异常。 */
    bool busy() const { return worker_ || running(); }
    /** @description 判断游戏进程正在启动或运行。
     * @param 无。
     * @throws 无主动异常。 */
    bool running() const { return game_.state() != QProcess::NotRunning; }
    /** @description 判断当前实例文件检查仍有效进行中。
     * @param 无。
     * @throws 无主动异常。 */
    bool checking() const { return checking_; }
    /** @description 判断全局 Java 环境扫描尚未完成。
     * @param 无。
     * @throws 无主动异常。 */
    bool scanning() const { return scanning_; }
    /** @description 判断正式版本清单后台加载尚未完成。
     * @param 无。
     * @throws 无主动异常。 */
    bool loadingVersions() const { return loadingVersions_; }
    /** @description 返回最近文件检查的可启动结论，检查期间隐藏旧结论。
     * @param 无。
     * @throws 无主动异常。 */
    bool installed() const { return !checking_ && current_.installed; }
    /** @description 返回当前阶段已完成数量，不是跨阶段累计值。
     * @param 无。
     * @throws 无主动异常。 */
    int progress() const { return progress_; }
    /** @description 返回当前阶段总量，零表示不定进度。
     * @param 无。
     * @throws 无主动异常。 */
    int progressTotal() const { return progressTotal_; }
    /** @description 将文件对话框 URL 转为本地路径。
     * @param url 用户选择的文件或目录。
     * @throws 无主动异常；非本地 URL 返回空路径。 */
    Q_INVOKABLE QString localFilePath(const QUrl &url) const { return url.toLocalFile(); }
    /** @description 冻结编辑目标并返回一次性草稿快照，失败不打开窗口。
     * @param id 待编辑实例的稳定 ID，不影响当前选择。
     * @throws 无主动异常；失败经 settingsError 报告并返回空映射。 */
    Q_INVOKABLE QVariantMap beginInstanceSettings(const QString &id);
    /** @description 验证草稿与可选 Java 后，只替换冻结目标的可编辑字段并原子保存一次。
     * @param session 打开时令牌；name 显示名；player 离线名；javaPath 空值代表自动选择。
     * @throws 无主动异常；失败保留会话，通过 settingsError 报告。 */
    Q_INVOKABLE void saveInstanceSettings(const QString &session, const QString &name, const QString &player, const QString &javaPath);
    /** @description 丢弃会话并使迟到探测失效，不影响其他后台任务。
     * @param session 要取消的编辑令牌。
     * @throws 无主动异常。 */
    Q_INVOKABLE void cancelInstanceSettings(const QString &session);
    /** @description 后台刷新正式版本清单。
     * @param 无。
     * @throws 无主动异常；网络提示写日志。 */
    Q_INVOKABLE void refreshVersions();
    /** @description 持久化选择并检查新实例，旧检查按代际失效。
     * @param id 已登记 ID 或空值。
     * @throws 无主动异常；失败经 error 报告。 */
    Q_INVOKABLE void selectInstance(const QString &id);
    /** @description 登记独立空目录实例并开始安装。
     * @param name 显示名；version 现代正式版标识；directory 完整绝对目标目录。
     * @throws 无主动异常；失败经 error 报告。 */
    Q_INVOKABLE void createInstance(const QString &name, const QString &version, const QString &directory);
    /** @description 后台检查当前实例并允许推荐兼容 Java。
     * @param 无。
     * @throws 无主动异常；失败经状态提示。 */
    Q_INVOKABLE void checkSelected();
    /** @description 安装或修复当前实例。
     * @param 无。
     * @throws 无主动异常；失败经 error 报告。 */
    Q_INVOKABLE void installSelected();
    /** @description 只刷新本机 Java 中性列表，不修改实例偏好。
     * @param 无。
     * @throws 无主动异常；任务忙碌时忽略。 */
    Q_INVOKABLE void scanJava();
    /** @description 校验玩家名后自动修复、选择 Java 并启动游戏。
     * @param 无。
     * @throws 无主动异常；失败经 error 报告。 */
    Q_INVOKABLE void launchSelected();
    /** @description 请求取消准备、检查、扫描或结束游戏。
     * @param 无。
     * @throws 无主动异常；后台清理前保持忙碌状态。 */
    Q_INVOKABLE void cancel();
signals:
    /** @description 通知对应快照变化，接收者重新读取属性。
     * @param 无。
     * @throws 无主动异常。 */
    void instancesChanged();
    /** @description 通知正式版清单已替换，接收者重新匹配版本选择。
     * @param 无。
     * @throws 无主动异常。 */
    void versionsChanged();
    /** @description 通知中性 Java 环境列表已更新。
     * @param 无。
     * @throws 无主动异常。 */
    void javaChanged();
    /** @description 通知业务状态与属性快照需要重新读取。
     * @param 无。
     * @throws 无主动异常。 */
    void stateChanged();
    /** @description 只在原子提交成功后要求所属窗口关闭。
     * @param session 已提交的会话令牌。
     * @throws 无主动异常。 */
    void instanceSettingsSaved(const QString &session);
private:
    void fail(const QString &message);
    void appendLog(const QString &message);
    void refreshInstances();
    void refreshJava();
    void recommendJava();
    void loadVersions(bool online);
    void startTask(bool launch, const QString &instanceId = {});
    void sampleTask();
    void finishTask(const QString &state, const QString &message);
    QVariantMap task_;
    QTimer taskTimer_;
    QElapsedTimer taskClock_;
    qint64 sampledBytes_ = 0, sampledAt_ = 0;
    int sampledStage_ = 0;
    void stopChecks();
    QString settingsBlockReason(const QString &id) const;
    void commitInstanceSettings(const QString &session, const QString &name, const QString &player, const QString &path);
    std::shared_ptr<std::atomic_bool> checkCancel_ = std::make_shared<std::atomic_bool>(false);
    std::shared_ptr<std::atomic_bool> scanCancel_ = std::make_shared<std::atomic_bool>(false);
    std::shared_ptr<std::atomic_bool> settingsCancel_ = std::make_shared<std::atomic_bool>(false);
    std::atomic_bool catalogCancel_{false};
    bool taskCancelled_ = false, gameStopped_ = false, settingsSaving_ = false;
    QString gameName_, settingsSession_, editingInstanceId_, settingsError_;
    const InstanceRecord *selected() const { return store_.find(store_.selectedId()); }
    InstanceStore store_;
    QProcess game_;
    Installer *worker_ = nullptr;
    QList<QFutureWatcher<VersionStatus> *> checks_;
    QList<QFutureWatcher<JavaRuntime> *> settingsProbes_;
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
