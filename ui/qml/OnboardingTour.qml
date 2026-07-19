import QtQuick
import QtQuick.Layouts

// Minimal first-run tour: five steps over the main surfaces.
Rectangle {
    id: tour
    anchors.fill: parent
    color: "#c0000000"
    visible: false
    property int step: 0

    readonly property var steps: [
        { title: qsTr("Map"), body: qsTr("Drag the A/B pins or right-click to place sites. Search accepts DD, DMS, MGRS and UTM. Scroll to zoom.") },
        { title: qsTr("Sites & Radio"), body: qsTr("Coordinates in all formats, antenna heights AGL, frequency, power (accepts '500 W' or '57 dBm'), gains, modulation.") },
        { title: qsTr("Profile"), body: qsTr("Terrain with effective-earth curvature, both masts, horizon rays and the shaded common-volume lens. Hover to sync with the map.") },
        { title: qsTr("Results"), body: qsTr("Geometry, the model comparison with its spread — the spread is your uncertainty — the budget waterfall and availability with diversity.") },
        { title: qsTr("Report"), body: qsTr("One click produces the bilingual PDF link-design report with the audit page and a reproducible content hash.") }
    ]

    function open() { step = 0; visible = true; forceActiveFocus() }

    MouseArea {
        anchors.fill: parent
        onClicked: {}
    }
    Rectangle {
        anchors.centerIn: parent
        width: 460
        height: content.implicitHeight + 40
        color: Theme.panel
        border.color: Theme.accent
        border.width: 1

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 20
            spacing: 10
            Text {
                text: (tour.step + 1) + "/" + tour.steps.length + "  " + tour.steps[tour.step].title
                color: Theme.accent
                font.pixelSize: 15
                font.bold: true
            }
            Text {
                Layout.fillWidth: true
                text: tour.steps[tour.step].body
                color: Theme.text
                font.pixelSize: Theme.fontSize
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("Skip (Esc)")
                    color: Theme.textDim
                    font.pixelSize: Theme.fontSizeSmall
                    MouseArea { anchors.fill: parent; onClicked: tour.visible = false }
                }
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 90
                    height: 26
                    color: Theme.accent
                    Text {
                        anchors.centerIn: parent
                        text: tour.step < tour.steps.length - 1 ? qsTr("Next") : qsTr("Finish")
                        color: "#101010"
                        font.bold: true
                        font.pixelSize: Theme.fontSizeSmall
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (tour.step < tour.steps.length - 1)
                                tour.step++
                            else
                                tour.visible = false
                        }
                    }
                }
            }
        }
    }
    Keys.onEscapePressed: visible = false
}
