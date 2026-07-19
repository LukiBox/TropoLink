import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Results: geometry block, model-comparison table with the spread highlighted,
// budget waterfall, availability block with diversity selector.
ScrollView {
    id: panel
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    component KV: RowLayout {
        property string k: ""
        property string v: ""
        property string provKey: ""
        property color valueColor: Theme.text
        Layout.fillWidth: true
        Text {
            text: k
            color: Theme.textDim
            font.pixelSize: Theme.fontSizeSmall
            Layout.preferredWidth: 150
            MouseArea {
                id: kvHover
                anchors.fill: parent
                hoverEnabled: true
            }
            Rectangle {
                visible: kvHover.containsMouse && provKey.length > 0
                y: parent.height + 2
                z: 99
                width: provText.implicitWidth + 12
                height: provText.implicitHeight + 8
                color: Theme.panelAlt
                border.color: Theme.border
                Text {
                    id: provText
                    anchors.centerIn: parent
                    text: controller.provenance(provKey)
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeSmall
                }
            }
        }
        Text {
            text: v
            color: valueColor
            font.pixelSize: Theme.fontSizeSmall
            font.family: Theme.mono
            Layout.fillWidth: true
        }
    }

    ColumnLayout {
        width: panel.availableWidth
        spacing: 6

        PanelSection {
            title: qsTr("Geometry")
            property var g: controller.geometry
            KV { k: qsTr("Distance");      v: (controller.geometry.distanceKm ?? 0).toFixed(3) + " km"; provKey: "distance" }
            KV { k: qsTr("Azimuth A→B");   v: (controller.geometry.azimuthAB ?? 0).toFixed(2) + "°"; provKey: "azimuth" }
            KV { k: qsTr("Azimuth B→A");   v: (controller.geometry.azimuthBA ?? 0).toFixed(2) + "°"; provKey: "azimuth" }
            KV { k: qsTr("Takeoff A");     v: (controller.geometry.takeoffAMrad ?? 0).toFixed(2) + " mrad"; provKey: "takeoff" }
            KV { k: qsTr("Takeoff B");     v: (controller.geometry.takeoffBMrad ?? 0).toFixed(2) + " mrad"; provKey: "takeoff" }
            KV {
                k: qsTr("Scatter angle θ")
                v: (controller.geometry.thetaMrad ?? 0).toFixed(2) + " mrad ("
                   + (controller.geometry.thetaDeg ?? 0).toFixed(3) + "°)"
                provKey: "theta"
            }
            KV { k: qsTr("Volume base AMSL"); v: (controller.geometry.cvBaseM ?? 0).toFixed(0) + " m"; provKey: "cv" }
            KV { k: qsTr("Volume above terrain"); v: (controller.geometry.cvAboveTerrainM ?? 0).toFixed(0) + " m"; provKey: "cv" }
            KV { k: qsTr("Slant range A / B"); v: (controller.geometry.slantAKm ?? 0).toFixed(2) + " / " + (controller.geometry.slantBKm ?? 0).toFixed(2) + " km"; provKey: "cv" }
            KV {
                k: qsTr("Direct path")
                v: (controller.geometry.directObstructed ?? false) ? qsTr("obstructed (troposcatter)") : qsTr("NOT obstructed")
                valueColor: (controller.geometry.directObstructed ?? false) ? Theme.good : Theme.warn
            }
        }

        PanelSection {
            title: qsTr("Model comparison")
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Model"); color: Theme.textDim; font.pixelSize: Theme.fontSizeSmall; font.bold: true; Layout.preferredWidth: 120 }
                    Text { text: qsTr("Median"); color: Theme.textDim; font.pixelSize: Theme.fontSizeSmall; font.bold: true; Layout.preferredWidth: 62 }
                    Text { text: qsTr("99.9%"); color: Theme.textDim; font.pixelSize: Theme.fontSizeSmall; font.bold: true; Layout.preferredWidth: 62 }
                    Text { text: qsTr("Lc"); color: Theme.textDim; font.pixelSize: Theme.fontSizeSmall; font.bold: true; Layout.fillWidth: true }
                }
                Repeater {
                    model: controller.modelRows
                    RowLayout {
                        Layout.fillWidth: true
                        property var row: modelData
                        Text {
                            text: (row.isPrimary ? "▶ " : "") + row.name
                            color: row.isPrimary ? Theme.accent : Theme.text
                            font.pixelSize: Theme.fontSizeSmall
                            font.bold: row.isPrimary
                            Layout.preferredWidth: 120
                            elide: Text.ElideRight
                            MouseArea {
                                id: rowHover
                                anchors.fill: parent
                                hoverEnabled: true
                            }
                            Rectangle {
                                visible: rowHover.containsMouse
                                y: parent.height + 2
                                z: 99
                                width: citeText.implicitWidth + 12
                                height: citeText.implicitHeight + 8
                                color: Theme.panelAlt
                                border.color: Theme.border
                                Text {
                                    id: citeText
                                    anchors.centerIn: parent
                                    text: row.citation + (row.issues.length > 0 ? "\n" + row.issues : "")
                                          + (row.note.length > 0 ? "\n" + row.note : "")
                                    color: Theme.text
                                    font.pixelSize: Theme.fontSizeSmall
                                }
                            }
                        }
                        Text {
                            text: row.valid ? row.medianDb.toFixed(1) : qsTr("OUT OF RANGE")
                            color: row.valid ? Theme.text : Theme.bad
                            font.pixelSize: Theme.fontSizeSmall
                            font.family: Theme.mono
                            Layout.preferredWidth: 62
                        }
                        Text {
                            text: row.valid && !row.isFspl ? row.annual[3].toFixed(1) : "—"
                            color: Theme.text
                            font.pixelSize: Theme.fontSizeSmall
                            font.family: Theme.mono
                            Layout.preferredWidth: 62
                        }
                        Text {
                            text: row.isFspl ? "—" : row.couplingDb.toFixed(1)
                            color: Theme.textDim
                            font.pixelSize: Theme.fontSizeSmall
                            font.family: Theme.mono
                            Layout.fillWidth: true
                        }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 20
                    color: Theme.panelAlt
                    border.color: Theme.accent
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("Spread: %1 dB (±%2 dB uncertainty)")
                                  .arg(controller.spreadDb.toFixed(1))
                                  .arg((controller.spreadDb / 2).toFixed(1))
                        color: Theme.accent
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Primary model")
                        color: Theme.textDim
                        font.pixelSize: Theme.fontSizeSmall
                    }
                    ComboBox {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 22
                        model: ["ITU-R P.617-5", "NBS TN101", "ITM (Longley-Rice)"]
                        currentIndex: controller.primaryModelIndex
                        font.pixelSize: Theme.fontSizeSmall
                        onActivated: (i) => controller.primaryModelIndex = i
                    }
                }
            }
        }

        PanelSection {
            title: qsTr("Link budget")
            WaterfallChart {
                Layout.fillWidth: true
                Layout.preferredHeight: 190
            }
            KV { k: qsTr("EIRP");       v: (controller.budget.eirp ?? 0).toFixed(1) + " dBm" }
            KV { k: qsTr("Median RSL"); v: (controller.budget.rsl ?? 0).toFixed(1) + " dBm" }
            KV { k: qsTr("Noise floor"); v: (controller.budget.noise ?? 0).toFixed(1) + " dBm"; provKey: "noise" }
            KV { k: qsTr("Median SNR"); v: (controller.budget.snr ?? 0).toFixed(1) + " dB" }
            KV { k: qsTr("Required SNR"); v: (controller.budget.requiredSnr ?? 0).toFixed(1) + " dB" }
            KV {
                k: qsTr("Fade margin")
                v: (controller.budget.margin ?? 0).toFixed(1) + " dB"
                valueColor: (controller.budget.margin ?? 0) >= 0 ? Theme.good : Theme.bad
            }
        }

        PanelSection {
            title: qsTr("Availability")
            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("Diversity")
                    color: Theme.textDim
                    font.pixelSize: Theme.fontSizeSmall
                    Layout.preferredWidth: 108
                }
                ComboBox {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22
                    model: [qsTr("none"), qsTr("space"), qsTr("frequency"), qsTr("angle"), qsTr("quad (space+freq)")]
                    currentIndex: controller.diversityIndex
                    font.pixelSize: Theme.fontSizeSmall
                    onActivated: (i) => controller.diversityIndex = i
                }
            }
            KV {
                k: qsTr("Annual availability")
                v: (controller.availability.annual ?? 0).toFixed(4) + " %"
                valueColor: (controller.availability.annual ?? 0) >= controller.targetAvailability ? Theme.good : Theme.bad
                provKey: "availability"
            }
            KV {
                k: qsTr("Worst-month availability")
                v: (controller.availability.worstMonth ?? 0).toFixed(4) + " %"
                provKey: "worstmonth"
            }
            KV {
                k: qsTr("Diversity gain @ target")
                v: (controller.availability.divGain ?? 0).toFixed(1) + " dB"
            }
            KV {
                k: qsTr("Spacing H / V")
                v: (controller.availability.sepH ?? 0).toFixed(1) + " / " + (controller.availability.sepV ?? 0).toFixed(1) + " m"
                provKey: "separation"
            }
            KV {
                k: qsTr("Frequency separation")
                v: (controller.availability.sepFMhz ?? 0).toFixed(1) + " MHz"
                provKey: "separation"
            }
            CurveItem {
                Layout.fillWidth: true
                Layout.preferredHeight: 170
                points: controller.availabilityCurve(controller.targetWorstMonth)
                currentMargin: controller.budget.margin ?? 0
                targetAvailability: controller.targetAvailability
                darkTheme: Theme.dark
            }
        }
    }
}
