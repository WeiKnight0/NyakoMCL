import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout {
    id: page
    required property var controller
    property bool compact: false
    property color ink: "#263530"
    property color muted: "#63736d"
    property color accent: "#237451"
    property color line: "#d9dfda"
    signal configure(string instanceId)
    spacing: 12
    SectionTitle { text: "实例管理"; ink: page.ink }
    Label { visible: controller.instances.length === 0; text: "暂无实例，可到下载页创建。"; color: page.muted }
    Repeater {
        model: controller.instances
        delegate: Rectangle {
            required property var modelData
            Layout.fillWidth: true
            implicitHeight: instanceColumn.implicitHeight + 22
            color: modelData.id === controller.selectedId ? "#edf5ee" : "#ffffff"
            border.color: modelData.id === controller.selectedId ? "#9dcab0" : page.line
            radius: 6
            ColumnLayout {
                id: instanceColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 11
                spacing: 5
                Label { text: modelData.name; font.bold: true; color: page.ink; Layout.fillWidth: true; elide: Text.ElideRight }
                Label { Layout.fillWidth: true; text: "正式版本 " + modelData.version + " · " + (modelData.pending ? "待安装" : "已登记"); color: page.muted }
                Label { Layout.fillWidth: true; text: modelData.directory; color: page.muted; wrapMode: Text.WrapAnywhere }
                RowLayout {
                    ActionButton { text: "选择"; enabled: controller.storeReady && !controller.busy && !controller.scanning && !controller.settingsSaving && modelData.id !== controller.selectedId; onClicked: controller.selectInstance(modelData.id) }
                    ActionButton { objectName: "configure-" + modelData.id; text: "实例设置"; enabled: controller.settingsAvailable; onClicked: page.configure(modelData.id) }
                }
            }
        }
    }
    RowLayout {
        visible: !!controller.selectedId
        ActionButton { text: "检查状态"; enabled: controller.settingsAvailable; onClicked: controller.checkSelected() }
        ActionButton { text: "安装 / 修复"; enabled: controller.settingsAvailable; onClicked: controller.installSelected() }
    }
}
