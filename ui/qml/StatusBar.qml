import QtQuick
import QtQuick.Layouts

// Status: terrain coverage, active model, compute time, air-gap indicator.
Rectangle {
    height: 24
    color: Theme.panelAlt
    border.color: Theme.border

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 18

        Text {
            text: controller.terrainCovered ? qsTr("TERRAIN OK") : qsTr("NO TERRAIN")
            color: controller.terrainCovered ? Theme.good : Theme.warn
            font.pixelSize: Theme.fontSizeSmall
            font.bold: true
        }
        Text {
            text: [qsTr("P.617-5"), qsTr("TN101"), qsTr("ITM")][controller.primaryModelIndex]
                  + " " + qsTr("primary")
            color: Theme.textDim
            font.pixelSize: Theme.fontSizeSmall
        }
        Text {
            text: qsTr("compute %1 ms").arg(controller.computeTimeMs.toFixed(0))
            color: Theme.textDim
            font.pixelSize: Theme.fontSizeSmall
            font.family: Theme.mono
        }
        Text {
            visible: controller.busy
            text: qsTr("computing…")
            color: Theme.accent
            font.pixelSize: Theme.fontSizeSmall
        }
        Text {
            Layout.fillWidth: true
            text: controller.statusMessage
            color: Theme.textDim
            font.pixelSize: Theme.fontSizeSmall
            elide: Text.ElideRight
        }
        Text {
            visible: controller.airgap
            text: qsTr("AIR-GAP")
            color: Theme.good
            font.pixelSize: Theme.fontSizeSmall
            font.bold: true
        }
    }
}
