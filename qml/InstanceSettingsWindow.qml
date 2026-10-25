import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Window {
    id: editor
    objectName: "instanceSettingsWindow"
    required property var controller
    property string session: ""
    property string editingInstanceId: ""
    property string version: ""
    property string directory: ""
    width: 560
    height: 650
    minimumWidth: 440
    minimumHeight: 480
    title: "实例设置 · NyakoMCL"
    color: "#f5f6f3"
    modality: Qt.WindowModal
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    // 只在打开时复制一次快照；列表刷新及当前实例切换不能重置编辑中的草稿。
    function openFor(instanceId) {
        if (visible) { requestActivate(); return }
        const snapshot = controller.beginInstanceSettings(instanceId)
        if (!snapshot.session) return
        session = snapshot.session
        editingInstanceId = snapshot.id
        version = snapshot.version
        directory = snapshot.directory
        displayName.text = snapshot.name
        playerName.text = snapshot.playerName
        javaPath.text = snapshot.javaPath
        x = transientParent.x + (transientParent.width - width) / 2
        y = transientParent.y + Math.max(0, (transientParent.height - height) / 2)
        show()
        requestActivate()
        displayName.forceActiveFocus()
    }
    onClosing: {
        javaDialog.close()
        controller.cancelInstanceSettings(session)
        session = ""
    }
    Connections {
        target: controller
        function onInstanceSettingsSaved(savedSession) { if (savedSession === editor.session) editor.close() }
    }
    FileDialog {
        id: javaDialog
        objectName: "settingsJavaDialog"
        title: "选择 Java 可执行文件"
        onAccepted: { if (editor.visible && editor.session) javaPath.text = controller.localFilePath(selectedFile) }
    }
    Shortcut { sequence: "Escape"; enabled: editor.visible && !javaPicker.popup.visible; onActivated: editor.close() }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12
        SectionTitle { text: "实例设置" }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: parent.width
                spacing: 10
                enabled: !controller.settingsSaving
                FieldTitle { text: "实例显示名" }
                TextField { id: displayName; objectName: "settingsDisplayName"; Layout.fillWidth: true }
                FieldTitle { text: "离线玩家名" }
                TextField { id: playerName; objectName: "settingsPlayerName"; Layout.fillWidth: true; placeholderText: "可暂留空；启动前需要配置" }
                FieldTitle { text: "Java 运行环境" }
                ComboBox {
                    id: javaPicker
                    objectName: "settingsJavaPicker"
                    Layout.fillWidth: true
                    model: [{label: "自动选择（不保存指定路径）", value: ""}].concat(controller.javaRuntimes)
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: model.findIndex(runtime => runtime.value === javaPath.text)
                    displayText: currentIndex < 0 ? "自定义 Java 路径" : currentText
                    onActivated: javaPath.text = currentValue
                    // 保持场景内 Popup 的键盘语义，移动高亮不修改草稿，确认选项才应用。
                    popup: Popup {
                        objectName: "settingsJavaPopup"
                        y: javaPicker.height
                        width: javaPicker.width
                        height: Math.min(220, javaList.contentHeight + padding * 2)
                        padding: 1
                        popupType: Popup.Item
                        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                        contentItem: ListView { id: javaList; clip: true; model: javaPicker.delegateModel; currentIndex: javaPicker.highlightedIndex; ScrollIndicator.vertical: ScrollIndicator {} }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    TextField { id: javaPath; objectName: "settingsJavaPath"; Layout.fillWidth: true; placeholderText: "留空自动选择 Java" }
                    ActionButton { objectName: "browseSettingsJava"; text: "浏览…"; onClicked: javaDialog.open() }
                }
                Label { Layout.fillWidth: true; text: "保存时验证 Java 可执行文件。不兼容的可用 Java 仍可保存；启动时自动选择或下载兼容环境。"; color: "#63736d"; wrapMode: Text.WordWrap }
                FieldTitle { text: "版本（只读）" }
                Label { Layout.fillWidth: true; text: editor.version; color: "#63736d" }
                FieldTitle { text: "目录（只读）" }
                Label { Layout.fillWidth: true; text: editor.directory; color: "#63736d"; wrapMode: Text.WrapAnywhere }
            }
        }
        Label { objectName: "settingsError"; Layout.fillWidth: true; visible: !!controller.settingsError; text: controller.settingsError; color: "#ae4135"; wrapMode: Text.WordWrap }
        RowLayout {
            Layout.fillWidth: true
            Label { text: controller.settingsSaving ? "正在验证 Java 并保存…" : "取消将丢弃所有未保存更改"; color: "#63736d"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            ActionButton { objectName: "cancelSettings"; text: "取消"; onClicked: editor.close() }
            ActionButton { objectName: "saveSettings"; text: "保存"; highlighted: true; enabled: controller.settingsAvailable && !!editor.session; onClicked: controller.saveInstanceSettings(editor.session, displayName.text, playerName.text, javaPath.text) }
        }
    }
}
