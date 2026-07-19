import QtQuick
import QtQuick.Layouts

// Collapsible titled section used across the docked panels.
ColumnLayout {
    id: section
    property string title: ""
    property bool expanded: true
    default property alias content: inner.data
    spacing: 4
    Layout.fillWidth: true

    Rectangle {
        Layout.fillWidth: true
        height: 24
        color: Theme.panelAlt
        border.color: Theme.border
        border.width: 1

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 8
            text: (section.expanded ? "▾ " : "▸ ") + section.title
            color: Theme.text
            font.pixelSize: Theme.fontSize
            font.bold: true
        }
        MouseArea {
            anchors.fill: parent
            onClicked: section.expanded = !section.expanded
        }
    }
    ColumnLayout {
        id: inner
        visible: section.expanded
        Layout.fillWidth: true
        Layout.leftMargin: 4
        Layout.rightMargin: 4
        spacing: 4
    }
}
