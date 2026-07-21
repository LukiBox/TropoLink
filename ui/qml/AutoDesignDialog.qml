import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Short summary of what the auto-design changed and why.
Dialog {
    id: dialog
    parent: Overlay.overlay
    modal: true
    width: Math.min(560, (parent ? parent.width : 700) - 60)
    anchors.centerIn: parent
    padding: 0

    property var outcome: ({ changes: [] })

    function showResult(r) {
        outcome = r
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
            text: qsTr("Auto-design result")
            color: Theme.text
            font.pixelSize: Theme.fontSize
            font.bold: true
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // Headline: what the design achieves.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: headline.implicitHeight + 16
            color: dialog.outcome.feasible ? Qt.alpha(Theme.good, 0.14)
                                          : Qt.alpha(Theme.warn, 0.14)
            Text {
                id: headline
                anchors.fill: parent
                anchors.margins: 8
                wrapMode: Text.Wrap
                color: Theme.text
                font.pixelSize: Theme.fontSize
                text: dialog.outcome.feasible
                      ? qsTr("Meets the target: %1 % annual availability, %2 dB fade margin.")
                            .arg((dialog.outcome.availabilityAnnual !== undefined
                                  ? dialog.outcome.availabilityAnnual : 0).toFixed(4))
                            .arg((dialog.outcome.marginDb !== undefined
                                  ? dialog.outcome.marginDb : 0).toFixed(1))
                      : (dialog.outcome.note !== undefined ? dialog.outcome.note : "")
            }
        }

        // The changes.
        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(360, Math.max(40, contentHeight))
            clip: true
            model: dialog.outcome.changes !== undefined ? dialog.outcome.changes : []
            delegate: ColumnLayout {
                required property var modelData
                width: ListView.view ? ListView.view.width : 0
                spacing: 1

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    Layout.topMargin: 8
                    spacing: 6
                    Text {
                        text: {
                            const f = modelData.field
                            if (f === "frequency") return qsTr("Frequency")
                            if (f === "diameter") return qsTr("Antenna diameter")
                            if (f === "gain") return qsTr("Antenna gain")
                            if (f === "txPower") return qsTr("TX power")
                            if (f === "modulation") return qsTr("Modulation")
                            return f
                        }
                        color: Theme.textDim
                        font.pixelSize: Theme.fontSizeSmall
                        Layout.preferredWidth: 120
                    }
                    Text {
                        text: modelData.from
                        color: Theme.textDim
                        font.family: Theme.mono
                        font.pixelSize: Theme.fontSizeSmall
                        font.strikeout: true
                    }
                    Text { text: "→"; color: Theme.textDim; font.pixelSize: Theme.fontSizeSmall }
                    Text {
                        text: modelData.to
                        color: Theme.accent
                        font.family: Theme.mono
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                }
                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: 130
                    Layout.rightMargin: 10
                    Layout.bottomMargin: 4
                    text: modelData.reason
                    color: Theme.textDim
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.Wrap
                }
            }
        }

        Text {
            visible: dialog.outcome.changes !== undefined && dialog.outcome.changes.length === 0
            Layout.fillWidth: true
            Layout.margins: 10
            text: qsTr("Nothing to change — the current radio is already the best "
                       + "configuration for this path.")
            color: Theme.textDim
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.Wrap
        }

        // Why not a bigger dish — the coupling-loss point, when it applies.
        Text {
            visible: dialog.outcome.rejectedDiameterM !== undefined
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.bottomMargin: 6
            text: dialog.outcome.rejectedDiameterM !== undefined
                  ? qsTr("A %1 m dish was rejected: at these gains the aperture-to-medium "
                         + "coupling loss (%2 dB) grows faster than the gain, so it would need "
                         + "%3 dB more transmit power, not less.")
                        .arg(dialog.outcome.rejectedDiameterM.toFixed(1))
                        .arg((dialog.outcome.couplingDb !== undefined
                              ? dialog.outcome.couplingDb : 0).toFixed(1))
                        .arg(dialog.outcome.rejectedPenaltyDb.toFixed(1))
                  : ""
            color: Theme.warn
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.Wrap
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 8
            spacing: 8
            Text {
                text: qsTr("Values are applied; every field remains editable.")
                color: Theme.textDim
                font.pixelSize: Theme.fontSizeSmall
            }
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Close")
                font.pixelSize: Theme.fontSize
                onClicked: dialog.close()
            }
        }
    }
}
