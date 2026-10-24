import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

// 界面只绑定控制器快照；实例写入和任务生命周期由主线程控制器统一协调。
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

    FileDialog {
        id: javaDialog
        title: "选择 Java 可执行文件"
        onAccepted: launcher.chooseJava(launcher.localFilePath(selectedFile))
    }
    FolderDialog {
        id: targetDialog
        title: "选择完整目标目录（须为空）"
        onAccepted: targetDirectory.text = launcher.localFilePath(selectedFolder)
    }
    Connections {
        target: launcher
        function onInstancesChanged() { renameInput.text = launcher.selectedName }
    }

    component ActionButton: Button {
        id: action
        implicitHeight: 38
        padding: 10
        font.pixelSize: 13
        font.weight: Font.DemiBold
        contentItem: Text {
            text: action.text
            font: action.font
            color: !action.enabled ? "#8b9790" : action.highlighted ? "#ffffff" : window.ink
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 6
            color: !action.enabled ? "#e8ece8" : action.highlighted ? (action.down ? "#18583d" : action.hovered ? "#2b8660" : window.accent) : action.down ? "#dce7df" : action.hovered ? "#f8fbf8" : "#edf1ed"
            border.color: action.highlighted ? "transparent" : window.line
            Behavior on color { ColorAnimation { duration: 90 } }
        }
    }
    component NavButton: Button {
        id: nav
        property int destination: 0
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
            Rectangle {
                visible: window.page === nav.destination && !window.compact
                width: 3; height: parent.height - 14; anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                color: window.accent; radius: 2
            }
        }
    }
    component SectionTitle: Label {
        font.pixelSize: 24
        font.weight: Font.DemiBold
        color: window.ink
        Layout.fillWidth: true
        elide: Text.ElideRight
    }
    component FieldTitle: Label {
        font.pixelSize: 13
        font.weight: Font.DemiBold
        color: window.muted
        Layout.fillWidth: true
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

                    ColumnLayout {
                        visible: window.page === 0
                        Layout.fillWidth: true
                        spacing: 14
                        SectionTitle { text: "启动游戏" }
                        Label { visible: launcher.selectedId === ""; Layout.fillWidth: true; text: launcher.instances.length ? "请选择一个实例，填写离线游戏用户名后启动。" : "还没有游戏实例，请先下载一个正式版本。"; color: window.muted; wrapMode: Text.WordWrap }
                        RowLayout {
                            visible: launcher.selectedId === ""
                            ActionButton { objectName: "downloadGuide"; text: "下载新实例"; highlighted: true; onClicked: window.page = 1 }
                            ActionButton { text: "选择已有实例"; visible: launcher.instances.length > 0; onClicked: window.page = 2 }
                        }
                        FieldTitle { text: "当前实例"; visible: launcher.instances.length > 0 }
                        ComboBox {
                            id: launchPicker
                            objectName: "launchInstancePicker"
                            visible: count > 0
                            Layout.fillWidth: true
                            model: launcher.instances
                            textRole: "name"
                            valueRole: "id"
                            currentIndex: {
                                for (let i = 0; i < count; ++i)
                                    if (model[i].id === launcher.selectedId) return i
                                return -1
                            }
                            displayText: currentIndex < 0 ? (count ? "请选择实例" : "没有实例") : currentText
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
                            text: launcher.requiredJava ? "Java " + launcher.requiredJava + " · " + (launcher.javaCompatible ? "运行环境就绪" : "启动时自动选择或下载兼容 Java") : "运行环境待检查"
                            color: launcher.requiredJava && !launcher.javaCompatible ? "#ae4135" : window.muted
                            wrapMode: Text.WrapAnywhere
                        }
                        FieldTitle { text: "离线游戏用户名 · " + launcher.selectedName; visible: launcher.selectedId !== "" }
                        TextField {
                            id: playerNameInput
                            objectName: "playerNameInput"
                            visible: launcher.selectedId !== ""
                            Layout.fillWidth: true
                            enabled: launcher.storeReady && !launcher.busy
                            placeholderText: "输入离线玩家名（1.20.3 起至多 16 字符）"
                            // 仅用户编辑时回写，实例切换触发的绑定刷新不会再次保存；也无需等待焦点离开才保存身份。
                            text: launcher.playerName
                            onTextEdited: launcher.playerName = text
                        }
                        ActionButton {
                            visible: launcher.selectedId !== ""
                            text: "启动游戏"
                            highlighted: true
                            Layout.preferredWidth: Math.min(320, content.availableWidth - 48)
                            Layout.preferredHeight: 54
                            font.pixelSize: 18
                            objectName: "launchButton"
                            enabled: launcher.storeReady && launcher.selectedId !== "" && !launcher.busy
                            onClicked: launcher.launchSelected()
                        }
                        RowLayout {
                            ActionButton { text: "检查状态"; enabled: launcher.selectedId !== "" && !launcher.busy && !launcher.checking; onClicked: launcher.checkSelected() }
                            ActionButton { text: "安装 / 修复"; enabled: launcher.storeReady && launcher.selectedId !== "" && !launcher.busy && !launcher.checking; onClicked: launcher.installSelected() }
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
                            ActionButton { text: "刷新"; enabled: !launcher.loadingVersions; onClicked: launcher.refreshVersions() }
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
                            ActionButton { text: "浏览…"; onClicked: targetDialog.open() }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: "可选择已有空目录，也可输入尚不存在的绝对目录；不会覆盖非空目录。"
                            color: window.muted
                            wrapMode: Text.WordWrap
                        }
                        ActionButton {
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
                                color: modelData.id === launcher.selectedId ? "#edf5ee" : "#ffffff"
                                border.color: modelData.id === launcher.selectedId ? "#9dcab0" : window.line
                                radius: 6
                                ColumnLayout {
                                    id: instanceColumn
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 11
                                    spacing: 5
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label { text: modelData.name; font.bold: true; color: window.ink; Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight }
                                        Label { text: modelData.pending ? "待安装" : "已登记"; color: window.muted }
                                        ActionButton { text: "选择"; enabled: launcher.storeReady && !launcher.busy && modelData.id !== launcher.selectedId; onClicked: launcher.selectInstance(modelData.id) }
                                    }
                                    Label { Layout.fillWidth: true; text: "正式版本 " + modelData.version; color: window.muted; elide: Text.ElideRight }
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
                            ActionButton {
                                text: "重命名"
                                enabled: launcher.storeReady && !launcher.busy && renameInput.text.trim() !== "" && renameInput.text.trim() !== launcher.selectedName
                                onClicked: launcher.renameSelected(renameInput.text)
                            }
                        }
                        RowLayout {
                            visible: launcher.selectedId !== ""
                            ActionButton { text: "检查状态"; enabled: !launcher.busy && !launcher.checking; onClicked: launcher.checkSelected() }
                            ActionButton { text: "安装 / 修复"; enabled: launcher.storeReady && !launcher.busy && !launcher.checking; onClicked: launcher.installSelected() }
                        }
                    }

                    ColumnLayout {
                        visible: window.page === 3
                        Layout.fillWidth: true
                        spacing: 12
                        SectionTitle { text: "启动设置" }
                        FieldTitle { text: "当前实例"; visible: launcher.instances.length > 0 }
                        ComboBox {
                            id: settingsPicker
                            objectName: "settingsInstancePicker"
                            visible: count > 0
                            Layout.fillWidth: true
                            model: launcher.instances
                            textRole: "name"
                            valueRole: "id"
                            currentIndex: model.findIndex(instance => instance.id === launcher.selectedId)
                            displayText: currentIndex < 0 ? "请选择实例" : currentText
                            enabled: launcher.storeReady && !launcher.busy
                            onActivated: launcher.selectInstance(currentValue)
                        }
                        Label {
                            Layout.fillWidth: true
                            text: launcher.selectedId ? "Java 仅保存到「" + launcher.selectedName + "」。离线游戏用户名请在启动页填写。" : "请先选择或下载实例，再保存 Java。现在可以扫描并查看已发现的运行时，不会保存任何选择。"
                            color: window.muted
                            wrapMode: Text.WordWrap
                        }
                        ActionButton { text: "下载新实例"; visible: launcher.selectedId === ""; onClicked: window.page = 1 }
                        FieldTitle { text: launcher.selectedId ? "当前实例的 Java" : "已发现的 Java（仅查看）"; Layout.topMargin: 10 }
                        Repeater {
                            model: launcher.selectedId === "" ? launcher.javaRuntimes : []
                            delegate: Label {
                                required property var modelData
                                Layout.fillWidth: true
                                text: modelData.label + "\n" + modelData.value
                                color: window.muted
                                wrapMode: Text.WrapAnywhere
                            }
                        }
                        Label { visible: launcher.selectedId === "" && launcher.javaRuntimes.length === 0; text: launcher.scanning ? "正在扫描 Java…" : "尚未发现 Java，可点击扫描 Java。"; color: window.muted }
                        Label {
                            Layout.fillWidth: true
                            text: launcher.requiredJava ? "需要 Java " + launcher.requiredJava + " · " + (launcher.javaCompatible ? "当前选择兼容" : "当前选择不兼容或尚未选择") : "检查实例后显示所需 Java"
                            color: launcher.requiredJava && !launcher.javaCompatible ? "#ae4135" : window.muted
                            wrapMode: Text.WordWrap
                        }
                        ComboBox {
                            id: javaPicker
                            objectName: "javaPicker"
                            visible: launcher.selectedId !== ""
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            model: launcher.javaRuntimes
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: launcher.javaRuntimes.findIndex(runtime => runtime.value === launcher.javaPath)
                            displayText: currentIndex < 0 ? "请选择 Java" : currentText
                            enabled: launcher.storeReady && !launcher.busy
                            onActivated: index => launcher.chooseJava(valueAt(index))
                            // 使用场景内 Popup，避免独立弹窗窗口带来焦点切换；保留 ComboBox 的键盘选择语义。
                            // Escape 或父控件外按下关闭；仅 onActivated 提交路径，焦点/高亮移动不写入实例。
                            popup: Popup {
                                y: javaPicker.height
                                width: javaPicker.width
                                height: Math.min(240, javaList.contentHeight + padding * 2)
                                padding: 1
                                popupType: Popup.Item
                                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                                contentItem: ListView {
                                    id: javaList
                                    clip: true
                                    model: javaPicker.delegateModel
                                    currentIndex: javaPicker.highlightedIndex
                                    ScrollIndicator.vertical: ScrollIndicator { }
                                }
                            }
                        }
                        Label { visible: launcher.selectedId !== ""; Layout.fillWidth: true; text: launcher.javaPath || "尚未选择 Java；检查后将自动推荐兼容版本"; color: window.muted; wrapMode: Text.WrapAnywhere }
                        RowLayout {
                            ActionButton { text: "扫描 Java"; enabled: !launcher.scanning; onClicked: launcher.scanJava() }
                            ActionButton { text: "浏览路径…"; visible: launcher.selectedId !== ""; enabled: launcher.storeReady && !launcher.busy; onClicked: javaDialog.open() }
                            BusyIndicator { running: launcher.scanning; implicitWidth: 28; implicitHeight: 28 }
                        }
                    }
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
                        Label { text: launcher.busy || launcher.checking ? "进行中" : "状态"; font.bold: true; color: window.ink }
                        Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: launcher.status; color: launcher.error ? "#ae4135" : window.muted; elide: Text.ElideRight }
                        ActionButton { text: launcher.running ? "结束游戏" : "取消"; visible: launcher.busy || launcher.checking || launcher.scanning; onClicked: launcher.cancel() }
                        ActionButton { text: logPane.visible ? "收起日志" : "展开日志"; onClicked: logPane.visible = !logPane.visible }
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
