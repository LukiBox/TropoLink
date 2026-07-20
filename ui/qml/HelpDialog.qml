import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Detailed help: topic list on the left, rich-text guide on the right.
// Content comes from the controller (bilingual, follows the app language).
Dialog {
    id: dialog
    modal: true
    width: Math.min(860, parent.width - 60)
    height: Math.min(640, parent.height - 60)
    anchors.centerIn: parent
    padding: 0

    readonly property var appCtl: controller
    property string currentTopic: "overview"
    property var topics: []

    function openTopic(topic) {
        topics = appCtl.helpTopics()
        currentTopic = topic.length > 0 ? topic : "overview"
        open()
    }

    background: Rectangle {
        color: Theme.panel
        border.color: Theme.border
        border.width: 1
    }

    header: Rectangle {
        height: 34
        color: Theme.panelAlt
        border.color: Theme.border
        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 12
            text: qsTr("TropoLink Help")
            color: Theme.text
            font.pixelSize: Theme.fontSize
            font.bold: true
        }
        Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 8
            width: 22
            height: 22
            color: closeArea.containsMouse ? Theme.panel : "transparent"
            border.color: closeArea.containsMouse ? Theme.border : "transparent"
            Text {
                anchors.centerIn: parent
                text: "✕"
                color: Theme.textDim
                font.pixelSize: 12
            }
            MouseArea {
                id: closeArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: dialog.close()
            }
        }
    }

    contentItem: RowLayout {
        spacing: 0

        // Topic list.
        Rectangle {
            Layout.preferredWidth: 230
            Layout.fillHeight: true
            color: Theme.panelAlt
            border.color: Theme.border

            ListView {
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                model: dialog.topics
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: 30
                    color: modelData.id === dialog.currentTopic ? Theme.panel
                           : topicArea.containsMouse ? Qt.alpha(Theme.panel, 0.5) : "transparent"
                    Rectangle {
                        visible: modelData.id === dialog.currentTopic
                        width: 3
                        height: parent.height
                        color: Theme.accent
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.right: parent.right
                        text: modelData.title
                        color: modelData.id === dialog.currentTopic ? Theme.text : Theme.textDim
                        font.pixelSize: Theme.fontSize
                        elide: Text.ElideRight
                    }
                    MouseArea {
                        id: topicArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: dialog.currentTopic = modelData.id
                    }
                }
            }
        }

        // Guide body.
        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: body.height + 32

            onContentHeightChanged: contentY = 0

            Text {
                id: body
                x: 18
                y: 14
                width: flick.width - 36
                text: dialog.appCtl.helpHtml(dialog.currentTopic)
                textFormat: Text.RichText
                wrapMode: Text.Wrap
                color: Theme.text
                font.pixelSize: Theme.fontSize + 1
                lineHeight: 1.25
            }
            ScrollBar.vertical: ScrollBar {}
        }
    }
}
