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
    spacing: 12
    SectionTitle { text: "全局设置"; ink: page.ink }
    Label { Layout.fillWidth: true; text: "此页管理本机环境。实例显示名、离线玩家名和 Java 偏好请在各实例的“实例设置”中修改。当前没有其他持久化的全局偏好。"; color: page.muted; wrapMode: Text.WordWrap }
    FieldTitle { text: "本机 Java 环境（仅查看）" }
    Repeater {
        model: controller.javaRuntimes
        delegate: Label { required property var modelData; Layout.fillWidth: true; text: modelData.label; color: page.muted; wrapMode: Text.WrapAnywhere }
    }
    Label { visible: controller.javaRuntimes.length === 0; Layout.fillWidth: true; text: controller.scanning ? "正在扫描 Java…" : "尚未发现 Java，可点击扫描 Java。"; color: page.muted; wrapMode: Text.WordWrap }
    Label { Layout.fillWidth: true; text: "扫描只刷新环境列表，不会保存或替换任何实例的 Java。启动和实例检查仍可自动选择兼容环境。"; color: page.muted; wrapMode: Text.WordWrap }
    RowLayout {
        ActionButton { objectName: "scanJavaButton"; text: "扫描 Java"; enabled: !controller.scanning && !controller.checking && !controller.busy && !controller.settingsSaving; onClicked: controller.scanJava() }
        BusyIndicator { running: controller.scanning; visible: running; implicitWidth: 28; implicitHeight: 28 }
    }
}
