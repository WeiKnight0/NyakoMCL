import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Label {
    property color ink: "#263530"
    font.pixelSize: 24
    font.weight: Font.DemiBold
    color: ink
    Layout.fillWidth: true
    elide: Text.ElideRight
}
