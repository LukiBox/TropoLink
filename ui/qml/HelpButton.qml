import QtQuick

// Small round "?" — opens the detailed help dialog at the given topic.
// Routed through the Theme singleton signal so it works from any document.
Rectangle {
    id: btn
    property string topic: ""
    width: 16
    height: 16
    radius: 8
    color: area.containsMouse ? Theme.accent : "transparent"
    border.color: area.containsMouse ? Theme.accent : Theme.textDim
    border.width: 1

    Text {
        anchors.centerIn: parent
        text: "?"
        color: area.containsMouse ? "#101010" : Theme.textDim
        font.pixelSize: 10
        font.bold: true
    }
    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: Theme.openHelp(btn.topic)
    }
}
