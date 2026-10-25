import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: card
    required property var controller
    readonly property var task: controller.task
    readonly property bool active: task.state === "running" || task.state === "cancelling"
    readonly property color resultColor: task.state === "failed" ? "#ae4135" : task.state === "succeeded" ? "#237451" : "#63736d"
    signal showLog()
    signal dismissed()
    visible: !!task.taskId
    implicitHeight: body.implicitHeight + 32
    color: "#ffffff"
    radius: 7
    border.color: "#d9dfda"
    function stateLabel(state) {
        return ({running: "进行中", cancelling: "正在取消…", succeeded: "已完成", failed: "失败", cancelled: "已取消"})[state] || "等待"
    }
    function bytes(value) {
        if (value >= 1048576) return (value / 1048576).toFixed(2) + " MiB"
        if (value >= 1024) return (value / 1024).toFixed(1) + " KiB"
        return Math.floor(value || 0) + " B"
    }
    ColumnLayout {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: (card.task.title || "") + " · " + (card.task.kind === "launch" ? "启动准备" : "安装 / 修复"); elide: Text.ElideRight; font.pixelSize: 18; font.bold: true; color: "#263530" }
            ActionButton { objectName: "cancelTask"; text: "取消"; visible: card.active; enabled: card.task.state === "running"; onClicked: card.controller.cancel() }
            ActionButton { objectName: "dismissTask"; text: "关闭"; visible: !card.active; onClicked: { card.controller.dismissTask(); card.dismissed() } }
        }
        Label { text: (card.task.version || "") + "  ·  " + card.stateLabel(card.task.state); color: card.resultColor }
        Label { text: card.task.stageLabel || "准备任务"; font.bold: true; color: "#263530" }
        Label {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            text: card.task.detail || ""; visible: text.length > 0; elide: Text.ElideMiddle; color: "#63736d"
            HoverHandler { id: detailHover }
            ToolTip.visible: detailHover.hovered
            ToolTip.text: text
        }
        ProgressBar { Layout.fillWidth: true; from: 0; to: Math.max(1, card.task.total || 0); value: card.task.done || 0; indeterminate: card.active && !card.task.total }
        Label { Layout.fillWidth: true; text: card.task.total > 0 ? "本阶段已处理 " + card.task.done + " / " + card.task.total + "，剩余 " + Math.max(0, card.task.total - card.task.done) : "本阶段数量未确定"; color: "#63736d"; wrapMode: Text.WordWrap }
        Label { Layout.fillWidth: true; text: "本阶段网络接收 " + card.bytes(card.task.receivedBytes) + " · " + card.bytes(card.task.bytesPerSecond) + "/s"; color: "#63736d"; wrapMode: Text.WordWrap }
        Label { Layout.fillWidth: true; visible: card.task.retryAttempt > 0 && card.active; text: "正在重试 " + card.task.retryAttempt + "/3 · " + (card.task.retryFile || ""); color: "#a16b24"; wrapMode: Text.WrapAnywhere }
        Repeater {
            model: card.task.stages || []
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                Label { text: modelData.state === "succeeded" ? "✓" : modelData.state === "running" ? "●" : "—"; color: modelData.state === "failed" ? "#ae4135" : "#237451" }
                Label { Layout.fillWidth: true; text: modelData.label; color: "#63736d"; wrapMode: Text.WordWrap }
                Label { text: card.stateLabel(modelData.state); color: "#63736d" }
            }
        }
        Label { Layout.fillWidth: true; visible: !card.active; text: card.task.message || ""; color: card.resultColor; wrapMode: Text.WrapAnywhere; textFormat: Text.PlainText }
        Flow {
            Layout.fillWidth: true; spacing: 8
            ActionButton { objectName: "copyTaskError"; visible: !!card.task.error; text: "复制错误"; onClicked: card.controller.copyTaskError() }
            ActionButton { objectName: "retryTask"; visible: card.task.state === "failed" || card.task.state === "cancelled"; enabled: !card.controller.busy && !card.controller.settingsSaving; text: card.task.kind === "launch" ? "重新启动" : "重新安装 / 修复"; onClicked: card.controller.retryTask() }
            ActionButton { objectName: "taskLog"; text: "查看日志"; onClicked: card.showLog() }
        }
        Label { Layout.fillWidth: true; text: "网络接收包含重试响应体；缓存命中只计入已处理文件。重试重新校验并复用完整文件。"; font.pixelSize: 12; color: "#63736d"; wrapMode: Text.WordWrap }
    }
}
