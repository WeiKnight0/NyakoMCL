import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    visible: true
    width: 1060
    height: 740
    minimumWidth: 520
    minimumHeight: 500
    title: "PCLQt · Minecraft Java"
    color: "#f4f6f4"
    property int page: 0
    readonly property bool compact: width < 760
    readonly property color ink: "#20362e"
    readonly property color muted: "#5e756a"
    readonly property color accent: "#247855"

    palette.window: "#f4f6f4"
    palette.windowText: ink
    palette.button: "#e5ece7"
    palette.buttonText: ink
    palette.highlight: accent

    FileDialog {
        id: javaDialog
        title: "选择 Java 可执行文件"
        onAccepted: launcher.chooseJava(selectedFile.toLocalFile())
    }
    FolderDialog {
        id: targetDialog
        title: "选择完整目标目录（须为空）"
        onAccepted: targetDirectory.text = selectedFolder.toLocalFile()
    }
    Connections {
        target: launcher
        function onInstancesChanged() { renameInput.text = launcher.selectedName }
    }

    component NavButton: Button {
        property int destination: 0
        flat: window.page !== destination
        highlighted: window.page === destination
        onClicked: window.page = destination
        Layout.fillWidth: true
    }
    component SectionTitle: Label {
        font.pixelSize: 22
        font.bold: true
        color: window.ink
        Layout.fillWidth: true
    }
    component FieldTitle: Label {
        font.pixelSize: 13
        font.bold: true
        color: window.muted
        Layout.fillWidth: true
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            visible: !window.compact
            Layout.preferredWidth: visible ? 198 : 0
            Layout.fillHeight: true
            color: "#e7ede9"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8
                Label { text: "PCLQt"; font.pixelSize: 25; font.bold: true; color: window.ink; Layout.bottomMargin: 20 }
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
                Layout.preferredHeight: visible ? 92 : 0
                color: "#e7ede9"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4
                    Label { text: "PCLQt"; font.pixelSize: 18; font.bold: true; color: window.ink }
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
            ScrollView {
                id: content
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    width: Math.max(0, content.availableWidth - 48)
                    x: 24
                    spacing: 14
                    Item { Layout.preferredHeight: 12 }

                    ColumnLayout {
                        visible: window.page === 0
                        Layout.fillWidth: true
                        spacing: 14
                        SectionTitle { text: "启动游戏" }
                        FieldTitle { text: "当前实例" }
                        ComboBox {
                            id: launchPicker
                            Layout.fillWidth: true
                            model: launcher.instances
                            textRole: "name"
                            valueRole: "id"
                            currentIndex: {
                                for (let i = 0; i < count; ++i)
                                    if (model[i].id === launcher.selectedId) return i
                                return -1
                            }
                            displayText: currentIndex < 0 ? "没有实例" : currentText
                            enabled: launcher.storeReady && !launcher.busy
                            onActivated: launcher.selectInstance(currentValue)
                        }
                        Label {
                            Layout.fillWidth: true
                            text: launcher.selectedId ? launcher.selectedVersion + "  ·  " + (launcher.checking ? "检查中" : launcher.installed ? "可启动" : "需要安装或修复") : "尚无实例"
                            color: window.muted
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: launcher.selectedId !== ""
                            text: launcher.selectedDirectory
                            color: window.muted
                            wrapMode: Text.WrapAnywhere
                        }
                        Rectangle { Layout.fillWidth: true; height: 1; color: "#cbd7cf"; Layout.topMargin: 10 }
                        Label {
                            Layout.fillWidth: true
                            font.pixelSize: 17
                            color: launcher.installed && launcher.javaCompatible ? window.accent : window.ink
                            text: launcher.selectedId === "" ? "请先到下载页创建实例" : launcher.status
                            wrapMode: Text.WrapAnywhere
                        }
                        Label {
                            Layout.fillWidth: true
                            text: launcher.requiredJava ? "Java " + launcher.requiredJava + " · " + (launcher.javaCompatible ? "运行环境就绪" : "请在设置中选择兼容 Java") : "运行环境待检查"
                            color: launcher.requiredJava && !launcher.javaCompatible ? "#ae4135" : window.muted
                            wrapMode: Text.WrapAnywhere
                        }
                        Button {
                            text: "启动游戏"
                            highlighted: true
                            Layout.preferredWidth: Math.min(320, content.availableWidth - 48)
                            Layout.preferredHeight: 54
                            font.pixelSize: 18
                            enabled: launcher.storeReady && launcher.installed && launcher.javaCompatible && !launcher.busy
                            onClicked: launcher.launchSelected()
                        }
                        RowLayout {
                            Button { text: "检查状态"; enabled: launcher.selectedId !== "" && !launcher.busy && !launcher.checking; onClicked: launcher.checkSelected() }
                            Button { text: "安装 / 修复"; enabled: launcher.storeReady && launcher.selectedId !== "" && !launcher.busy && !launcher.checking; onClicked: launcher.installSelected() }
                        }
                    }

                    ColumnLayout {
                        visible: window.page === 1
                        Layout.fillWidth: true
                        spacing: 12
                        SectionTitle { text: "下载正式版" }
                        FieldTitle { text: "正式版本" }
                        RowLayout {
                            Layout.fillWidth: true
                            ComboBox {
                                id: versionPicker
                                Layout.fillWidth: true
                                model: launcher.versions
                                textRole: "label"
                                valueRole: "value"
                                displayText: currentIndex < 0 ? "暂无版本" : currentText
                            }
                            Button { text: "刷新"; enabled: !launcher.loadingVersions; onClicked: launcher.refreshVersions() }
                        }
                        BusyIndicator { running: launcher.loadingVersions; visible: running; implicitWidth: 28; implicitHeight: 28 }
                        FieldTitle { text: "实例显示名" }
                        TextField { id: displayName; Layout.fillWidth: true; placeholderText: "例如：生存世界" }
                        FieldTitle { text: "完整目标目录" }
                        RowLayout {
                            Layout.fillWidth: true
                            TextField {
                                id: targetDirectory
                                Layout.fillWidth: true
                                placeholderText: "D:/Games/MyInstance"
                            }
                            Button { text: "浏览…"; onClicked: targetDialog.open() }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: "可选择已有空目录，也可输入尚不存在的绝对目录；不会覆盖非空目录。"
                            color: window.muted
                            wrapMode: Text.WordWrap
                        }
                        Button {
                            text: "创建并安装"
                            highlighted: true
                            Layout.preferredHeight: 46
                            enabled: launcher.storeReady && !launcher.busy && !launcher.loadingVersions && versionPicker.currentIndex >= 0
                            onClicked: launcher.createInstance(displayName.text, versionPicker.currentValue, targetDirectory.text)
                        }
                    }

                    ColumnLayout {
                        visible: window.page === 2
                        Layout.fillWidth: true
                        spacing: 12
                        SectionTitle { text: "实例管理" }
                        Label { visible: launcher.instances.length === 0; text: "暂无实例，可到下载页创建。"; color: window.muted }
                        Repeater {
                            model: launcher.instances
                            delegate: Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                implicitHeight: instanceColumn.implicitHeight + 22
                                color: modelData.id === launcher.selectedId ? "#e0efe5" : "#eef2ef"
                                radius: 4
                                ColumnLayout {
                                    id: instanceColumn
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 11
                                    spacing: 5
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label { text: modelData.name; font.bold: true; color: window.ink; Layout.fillWidth: true; elide: Text.ElideRight }
                                        Label { text: modelData.pending ? "待安装" : "已登记"; color: window.muted }
                                        Button { text: "选择"; enabled: launcher.storeReady && !launcher.busy && modelData.id !== launcher.selectedId; onClicked: launcher.selectInstance(modelData.id) }
                                    }
                                    Label { text: "正式版本 " + modelData.version; color: window.muted }
                                    Label { Layout.fillWidth: true; text: modelData.directory; color: window.muted; wrapMode: Text.WrapAnywhere }
                                }
                            }
                        }
                        Rectangle { Layout.fillWidth: true; height: 1; color: "#cbd7cf"; visible: launcher.selectedId !== "" }
                        FieldTitle { text: "已选实例 · " + launcher.selectedVersion; visible: launcher.selectedId !== "" }
                        RowLayout {
                            visible: launcher.selectedId !== ""
                            Layout.fillWidth: true
                            TextField { id: renameInput; Layout.fillWidth: true; placeholderText: "实例显示名"; text: launcher.selectedName }
                            Button {
                                text: "重命名"
                                enabled: launcher.storeReady && !launcher.busy && renameInput.text.trim() !== "" && renameInput.text.trim() !== launcher.selectedName
                                onClicked: launcher.renameSelected(renameInput.text)
                            }
                        }
                        RowLayout {
                            visible: launcher.selectedId !== ""
                            Button { text: "检查状态"; enabled: !launcher.busy && !launcher.checking; onClicked: launcher.checkSelected() }
                            Button { text: "安装 / 修复"; enabled: launcher.storeReady && !launcher.busy && !launcher.checking; onClicked: launcher.installSelected() }
                        }
                    }

                    ColumnLayout {
                        visible: window.page === 3
                        Layout.fillWidth: true
                        spacing: 12
                        SectionTitle { text: "启动设置" }
                        FieldTitle { text: "离线名字" }
                        TextField {
                            Layout.fillWidth: true
                            placeholderText: "3–16 位字母、数字或下划线"
                            text: launcher.playerName
                            onEditingFinished: launcher.playerName = text
                        }
                        FieldTitle { text: "当前实例的 Java"; Layout.topMargin: 10 }
                        Label {
                            Layout.fillWidth: true
                            text: launcher.requiredJava ? "需要 Java " + launcher.requiredJava + " · " + (launcher.javaCompatible ? "当前选择兼容" : "当前选择不兼容或尚未选择") : "检查实例后显示所需 Java"
                            color: launcher.requiredJava && !launcher.javaCompatible ? "#ae4135" : window.muted
                            wrapMode: Text.WordWrap
                        }
                        ComboBox {
                            Layout.fillWidth: true
                            model: launcher.javaRuntimes
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: {
                                for (let i = 0; i < count; ++i)
                                    if (model[i].value === launcher.javaPath) return i
                                return -1
                            }
                            displayText: currentIndex < 0 ? "请选择 Java" : currentText
                            enabled: launcher.storeReady && launcher.selectedId !== "" && !launcher.busy
                            onActivated: launcher.chooseJava(currentValue)
                        }
                        Label { Layout.fillWidth: true; text: launcher.javaPath || "未配置 Java 路径"; color: window.muted; wrapMode: Text.WrapAnywhere }
                        RowLayout {
                            Button { text: "扫描 Java"; enabled: !launcher.scanning; onClicked: launcher.scanJava() }
                            Button { text: "浏览路径…"; enabled: launcher.storeReady && launcher.selectedId !== "" && !launcher.busy; onClicked: javaDialog.open() }
                            BusyIndicator { running: launcher.scanning; implicitWidth: 28; implicitHeight: 28 }
                        }
                    }
                    Item { Layout.preferredHeight: 12 }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: activity.implicitHeight + 24
                color: "#e7ede9"
                ColumnLayout {
                    id: activity
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 7
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: launcher.busy || launcher.checking ? "进行中" : "状态"; font.bold: true; color: window.ink }
                        Label { Layout.fillWidth: true; text: launcher.status; color: launcher.error ? "#ae4135" : window.muted; elide: Text.ElideRight }
                        Button { text: "取消"; visible: launcher.busy; onClicked: launcher.cancel() }
                        Button { text: logPane.visible ? "收起日志" : "展开日志"; onClicked: logPane.visible = !logPane.visible }
                    }
                    ProgressBar {
                        Layout.fillWidth: true
                        from: 0
                        to: Math.max(1, launcher.progressTotal)
                        value: launcher.progress
                        indeterminate: (launcher.busy || launcher.checking) && launcher.progressTotal === 0
                    }
                    Label { Layout.fillWidth: true; visible: launcher.error !== ""; text: launcher.error; color: "#ae4135"; wrapMode: Text.WrapAnywhere }
                    ScrollView {
                        id: logPane
                        visible: false
                        Layout.fillWidth: true
                        Layout.preferredHeight: 135
                        TextArea {
                            readOnly: true
                            text: launcher.log
                            wrapMode: Text.WrapAnywhere
                            font.family: "Consolas"
                            background: Rectangle { color: "#f4f6f4"; border.color: "#cbd7cf"; radius: 4 }
                        }
                    }
                }
            }
        }
    }
}
