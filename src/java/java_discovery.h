#pragma once

#include <QString>
#include <QList>
#include <atomic>

/** @description 保存实际执行 java -version 得到的运行时信息；major 为零代表无效探测。
 * @param 无；architecture 是输出推断的展示信息，不参与兼容判定。
 * @throws 无主动异常。 */
struct JavaRuntime {
    QString path;
    QString version;
    QString architecture;
    int major = 0;

    /** @description 按版本元数据要求精确匹配主版本，不假设高版本向后兼容。
     * @param requiredMajor 游戏要求的 Java 主版本。
     * @throws 无主动异常。 */
    bool compatible(int requiredMajor) const { return major == requiredMajor; }
    /** @description 为选择列表生成包含路径和兼容提示的文本。
     * @param requiredMajor 需求主版本；零表示不显示不兼容提示。
     * @throws 无主动异常。 */
    QString label(int requiredMajor = 0) const;
};

/** @description 同步探测可执行文件并限制进程等待时间，界面调用应放到后台线程。
 * @param path 候选 Java 路径；cancel 可选取消标记，取消时终止探测进程。
 * @throws 无主动异常；无效文件、超时、非正常退出或取消返回 major 为零的运行时。 */
JavaRuntime inspectJava(const QString &path, const std::atomic_bool *cancel = nullptr);
/** @description 从已发布运行时、保存路径及系统常见位置探测 Java，按规范路径去重。
 * @param preferredPath 保存的候选路径，位于托管目录候选之后；cancel 可选取消标记。
 * @throws 无主动异常；无效候选被跳过，取消返回已发现的部分结果。 */
QList<JavaRuntime> discoverJava(const QString &preferredPath, const std::atomic_bool *cancel = nullptr);
