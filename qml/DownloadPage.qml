import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

ColumnLayout {
    id: page
    required property var controller
    property bool compact: false
    property color ink: "#263530"
    property color muted: "#63736d"
    property color accent: "#237451"
    property color line: "#d9dfda"
    spacing: 12
    FolderDialog {
        id: targetDialog
        title: "选择完整目标目录（须为空）"
        onAccepted: targetDirectory.text = controller.localFilePath(selectedFolder)
    }
    SectionTitle { text: "下载正式版"; ink: page.ink }
    FieldTitle { text: "正式版本" }
    RowLayout {
        Layout.fillWidth: true
        ComboBox { id: versionPicker; Layout.fillWidth: true; model: controller.versions; textRole: "label"; valueRole: "value"; displayText: currentIndex < 0 ? "暂无版本" : currentText }
        ActionButton { text: "刷新"; enabled: !controller.loadingVersions; onClicked: controller.refreshVersions() }
    }
    BusyIndicator { running: controller.loadingVersions; visible: running; implicitWidth: 28; implicitHeight: 28 }
    FieldTitle { text: "实例显示名" }
    TextField { id: displayName; objectName: "downloadName"; Layout.fillWidth: true; placeholderText: "例如：生存世界" }
    FieldTitle { text: "完整目标目录" }
    RowLayout {
        Layout.fillWidth: true
        TextField { id: targetDirectory; objectName: "downloadDirectory"; Layout.fillWidth: true; placeholderText: "D:/Games/MyInstance" }
        ActionButton { text: "浏览…"; onClicked: targetDialog.open() }
    }
    Label { Layout.fillWidth: true; text: "可选择已有空目录，也可输入尚不存在的绝对目录；不会覆盖非空目录。"; color: page.muted; wrapMode: Text.WordWrap }
    ActionButton { text: "创建并安装"; highlighted: true; Layout.preferredHeight: 46; enabled: controller.storeReady && !controller.busy && !controller.settingsSaving && !controller.loadingVersions && versionPicker.currentIndex >= 0; onClicked: controller.createInstance(displayName.text, versionPicker.currentValue, targetDirectory.text) }
}
