import QtQuick
import QtQuick.Controls.Basic

Button {
    id: action
    property color ink: "#263530"
    property color accent: "#237451"
    property color line: "#d9dfda"
    implicitHeight: 38
    padding: 10
    font.pixelSize: 13
    font.weight: Font.DemiBold
    contentItem: Text {
        text: action.text
        font: action.font
        color: !action.enabled ? "#8b9790" : action.highlighted ? "#ffffff" : action.ink
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 6
        color: !action.enabled ? "#e8ece8" : action.highlighted ? (action.down ? "#18583d" : action.hovered ? "#2b8660" : action.accent) : action.down ? "#dce7df" : action.hovered ? "#f8fbf8" : "#edf1ed"
        border.color: action.highlighted ? "transparent" : action.line
        Behavior on color { ColorAnimation { duration: 90 } }
    }
}
