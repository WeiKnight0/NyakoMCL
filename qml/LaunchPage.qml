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
    signal navigate(int destination)
    signal configure(string instanceId)
    spacing: 14
    SectionTitle { text: "启动游戏"; ink: page.ink }
    Label { visible: !controller.selectedId; Layout.fillWidth: true; text: controller.instances.length ? "请选择一个实例，配置离线玩家名后启动。" : "还没有游戏实例，请先下载一个正式版本。"; color: page.muted; wrapMode: Text.WordWrap }
    RowLayout {
        visible: !controller.selectedId
        ActionButton { objectName: "downloadGuide"; text: "下载新实例"; highlighted: true; onClicked: page.navigate(1) }
        ActionButton { text: "选择已有实例"; visible: controller.instances.length > 0; onClicked: page.navigate(2) }
    }
    FieldTitle { text: "当前实例"; visible: controller.instances.length > 0 }
    ComboBox {
        objectName: "launchInstancePicker"
        visible: count > 0
        Layout.fillWidth: true
        model: controller.instances
        textRole: "name"
        valueRole: "id"
        currentIndex: controller.instances.findIndex(instance => instance.id === controller.selectedId)
        displayText: currentIndex < 0 ? "请选择实例" : currentText
        enabled: controller.storeReady && !controller.busy && !controller.scanning && !controller.settingsSaving
        onActivated: controller.selectInstance(currentValue)
    }
    Label { Layout.fillWidth: true; text: controller.selectedId ? controller.selectedVersion + "  ·  " + (controller.checking ? "检查中" : controller.installed ? "可启动" : "需要安装或修复") : "尚无实例"; color: page.muted }
    Label { Layout.fillWidth: true; visible: !!controller.selectedId; text: controller.selectedDirectory; color: page.muted; wrapMode: Text.WrapAnywhere }
    Rectangle { Layout.fillWidth: true; height: 1; color: page.line; Layout.topMargin: 10 }
    Label { Layout.fillWidth: true; font.pixelSize: 17; color: controller.installed && controller.javaCompatible ? page.accent : page.ink; text: controller.selectedId ? controller.status : "请先到下载页创建实例"; wrapMode: Text.WrapAnywhere }
    Label { Layout.fillWidth: true; text: controller.requiredJava ? "Java " + controller.requiredJava + " · " + (controller.javaCompatible ? "运行环境就绪" : "启动时自动选择或下载兼容 Java") : "运行环境待检查"; color: controller.requiredJava && !controller.javaCompatible ? "#ae4135" : page.muted; wrapMode: Text.WrapAnywhere }
    Label { visible: !!controller.selectedId; Layout.fillWidth: true; text: "离线玩家名：" + (controller.playerName || "尚未配置") + "\nJava：" + (controller.javaPath || "自动选择"); color: page.muted; wrapMode: Text.WrapAnywhere }
    ActionButton { objectName: "instanceSettingsButton"; visible: !!controller.selectedId; text: "实例设置"; enabled: controller.settingsAvailable; onClicked: page.configure(controller.selectedId) }
    ActionButton {
        objectName: "launchButton"
        visible: !!controller.selectedId
        text: "启动游戏"
        highlighted: true
        Layout.preferredWidth: Math.min(320, page.width)
        Layout.preferredHeight: 54
        font.pixelSize: 18
        enabled: controller.storeReady && !!controller.selectedId && !controller.busy && !controller.settingsSaving
        onClicked: controller.launchSelected()
    }
    RowLayout {
        ActionButton { text: "检查状态"; enabled: !!controller.selectedId && controller.settingsAvailable; onClicked: controller.checkSelected() }
        ActionButton { text: "安装 / 修复"; enabled: !!controller.selectedId && controller.settingsAvailable; onClicked: controller.installSelected() }
    }
}
