import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// 外壳只协调常驻页导航；表单草稿由所属页面或独立窗口持有。
ApplicationWindow {
    id: window
    visible: true
    width: 1060
    height: 740
    minimumWidth: 510
    minimumHeight: 500
    title: "NyakoMCL · Minecraft Java"
    color: "#f5f6f3"
    property int page: 0
    readonly property int tasksPage: 4
    property int taskSourcePage: 0
    readonly property string taskSourceLabel: ["启动", "下载", "实例", "设置"][taskSourcePage]
    readonly property bool compact: width < 780
    readonly property color ink: "#263530"
    readonly property color muted: "#63736d"
    readonly property color accent: "#237451"
    readonly property color line: "#d9dfda"
    palette.window: color
    palette.windowText: ink
    palette.button: "#edf1ed"
    palette.buttonText: ink
    palette.highlight: accent
    onPageChanged: {
        content.contentItem.contentY = 0
        pageEntrance.restart()
    }
    // 已在任务页时不覆盖来源，重试与重复查看仍能返回原页面。
    function viewTask() {
        if (window.page === window.tasksPage) return
        window.taskSourcePage = window.page
        window.page = window.tasksPage
    }
    function returnFromTask() {
        window.page = window.taskSourcePage
    }
    Connections {
        target: launcher
        function onTaskStarted() { window.viewTask() }
        function onTaskNotice(message) { notice.text = message; notice.visible = true; noticeTimer.restart() }
    }
    Timer { id: noticeTimer; interval: 4500; onTriggered: notice.visible = false }
    InstanceSettingsWindow { id: instanceSettings; controller: launcher; transientParent: window }
    component NavButton: Button {
        id: nav
        property int destination: 0
        objectName: "nav" + destination + (window.compact ? "Compact" : "Wide")
        Layout.fillWidth: true
        implicitHeight: window.compact ? 38 : 44
        padding: 8
        font.pixelSize: 14
        font.weight: window.page === destination ? Font.DemiBold : Font.Normal
        onClicked: window.page = destination
        contentItem: Text {
            text: nav.text
            font: nav.font
            color: window.page === nav.destination ? window.accent : window.muted
            horizontalAlignment: window.compact ? Text.AlignHCenter : Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 5
            color: window.page === nav.destination ? "#dcece1" : nav.hovered ? "#e5eae5" : "transparent"
            Rectangle { visible: window.page === nav.destination && !window.compact; width: 3; height: parent.height - 14; anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; color: window.accent; radius: 2 }
        }
    }
    ParallelAnimation {
        id: pageEntrance
        NumberAnimation { target: pageSurface; property: "opacity"; from: 0.65; to: 1; duration: 140; easing.type: Easing.OutQuad }
        NumberAnimation { target: pageOffset; property: "y"; from: 7; to: 0; duration: 140; easing.type: Easing.OutQuad }
    }
    RowLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            visible: !window.compact
            Layout.preferredWidth: visible ? 204 : 0
            Layout.fillHeight: true
            color: "#e9eee9"
            Rectangle { anchors.right: parent.right; height: parent.height; width: 1; color: window.line }
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 7
                Label { text: "NyakoMCL"; font.pixelSize: 27; font.weight: Font.Bold; color: window.ink; Layout.bottomMargin: 2 }
                Label { text: "MINECRAFT JAVA"; font.pixelSize: 10; color: window.muted; Layout.bottomMargin: 26 }
                NavButton { text: "启动"; destination: 0 }
                NavButton { text: "下载"; destination: 1 }
                NavButton { text: "实例"; destination: 2 }
                NavButton { text: "设置"; destination: 3 }
                Item { Layout.fillHeight: true }
                Label { text: "Minecraft Java · 离线"; color: window.muted; font.pixelSize: 12 }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            Rectangle {
                visible: window.compact
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? 86 : 0
                color: "#e9eee9"
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: window.line }
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 5
                    Label { text: "NyakoMCL  /  Minecraft Java"; font.pixelSize: 17; font.weight: Font.DemiBold; color: window.ink }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        NavButton { text: "启动"; destination: 0 }
                        NavButton { text: "下载"; destination: 1 }
                        NavButton { text: "实例"; destination: 2 }
                        NavButton { text: "设置"; destination: 3 }
                    }
                }
            }
            Label {
                id: notice
                objectName: "taskNotice"
                visible: false
                Layout.fillWidth: true
                Layout.margins: 8
                padding: 10
                maximumLineCount: 2
                elide: Text.ElideRight
                wrapMode: Text.WrapAnywhere
                textFormat: Text.PlainText
                color: launcher.task.state === "failed" ? "#ae4135" : launcher.task.state === "succeeded" ? window.accent : window.muted
                background: Rectangle {
                    color: launcher.task.state === "failed" ? "#f8e8e5" : launcher.task.state === "succeeded" ? "#e2eee4" : "#edf0ed"
                    border.color: launcher.task.state === "failed" ? "#d7aaa3" : launcher.task.state === "succeeded" ? "#a5c2ae" : "#cbd3ce"
                    radius: 6
                }
            }
            Item {
                id: pageSurface
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollView {
                    id: content
                    anchors.fill: parent
                    contentWidth: availableWidth
                    clip: true
                    ColumnLayout {
                        id: pageOffset
                        width: Math.max(0, content.availableWidth - (window.compact ? 32 : 64))
                        x: window.compact ? 16 : 32
                        spacing: 14
                        Item { Layout.preferredHeight: 18 }
                        LaunchPage { visible: window.page === 0; Layout.fillWidth: true; controller: launcher; compact: window.compact; ink: window.ink; muted: window.muted; accent: window.accent; line: window.line; onNavigate: destination => window.page = destination; onConfigure: instanceId => instanceSettings.openFor(instanceId) }
                        DownloadPage { visible: window.page === 1; Layout.fillWidth: true; controller: launcher; compact: window.compact; ink: window.ink; muted: window.muted; accent: window.accent; line: window.line }
                        TasksPage { visible: window.page === window.tasksPage; Layout.fillWidth: true; controller: launcher; sourceLabel: window.taskSourceLabel; ink: window.ink; muted: window.muted; onGoBack: window.returnFromTask(); onShowLog: logPane.visible = true }
                        InstancesPage { visible: window.page === 2; Layout.fillWidth: true; controller: launcher; compact: window.compact; ink: window.ink; muted: window.muted; accent: window.accent; line: window.line; onConfigure: instanceId => instanceSettings.openFor(instanceId) }
                        SettingsPage { visible: window.page === 3; Layout.fillWidth: true; controller: launcher; compact: window.compact; ink: window.ink; muted: window.muted; accent: window.accent; line: window.line }
                        Item { Layout.preferredHeight: 18 }
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: activity.implicitHeight + 24
                color: "#e9eee9"
                Rectangle { width: parent.width; height: 1; color: window.line }
                ColumnLayout {
                    id: activity
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 7
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: launcher.task.taskId ? "任务" : "状态"; font.bold: true; color: window.ink }
                        Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: launcher.task.taskId && !launcher.running ? launcher.task.title + " · " + ({running: "进行中", cancelling: "正在取消…", succeeded: "已完成", failed: "失败", cancelled: "已取消"})[launcher.task.state] : launcher.status; color: launcher.task.state === "failed" ? "#ae4135" : window.muted; elide: Text.ElideRight }
                        ActionButton { objectName: "viewTask"; text: "查看任务"; visible: !!launcher.task.taskId; onClicked: window.viewTask() }
                        ActionButton { text: launcher.running ? "结束游戏" : "取消"; visible: launcher.running || (!launcher.task.taskId && (launcher.checking || launcher.scanning)); onClicked: launcher.cancel() }
                        ActionButton { text: logPane.visible ? "收起日志" : "展开日志"; onClicked: logPane.visible = !logPane.visible }
                    }
                    Label { objectName: "settingsDisabledReason"; Layout.fillWidth: true; visible: !!launcher.settingsDisabledReason; text: launcher.settingsDisabledReason; color: window.muted; wrapMode: Text.WordWrap }
                    Label { Layout.fillWidth: true; visible: launcher.error !== "" && !launcher.task.taskId; text: launcher.error; color: "#ae4135"; wrapMode: Text.WrapAnywhere }
                    ScrollView {
                        id: logPane
                        visible: false
                        Layout.fillWidth: true
                        Layout.preferredHeight: 135
                        TextArea { readOnly: true; text: launcher.log; wrapMode: Text.WrapAnywhere; font.family: "Consolas"; background: Rectangle { color: "#f4f6f4"; border.color: "#cbd7cf"; radius: 4 } }
                    }
                }
            }
        }
    }
}
