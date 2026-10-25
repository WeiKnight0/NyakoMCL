import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// 返回只负责导航，让下载草稿与后台任务在离开页面后继续保留。
ColumnLayout {
    id: page
    objectName: "tasksPage"
    required property var controller
    property string sourceLabel: "启动"
    property color ink: "#263530"
    property color muted: "#63736d"
    signal goBack()
    signal showLog()
    spacing: 12
    RowLayout {
        Layout.fillWidth: true
        SectionTitle { text: "任务管理"; ink: page.ink; Layout.fillWidth: true }
        ActionButton { objectName: "taskBack"; text: "返回" + page.sourceLabel; onClicked: page.goBack() }
    }
    TaskCard {
        objectName: "taskCard"
        Layout.fillWidth: true
        controller: page.controller
        onShowLog: page.showLog()
        onDismissed: page.goBack()
    }
    Label {
        objectName: "tasksEmpty"
        Layout.fillWidth: true
        visible: !page.controller.task.taskId
        text: "暂无任务。可返回启动页准备游戏，或在下载页创建实例。"
        color: page.muted
        wrapMode: Text.WordWrap
    }
}
