import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Sites & Radio panel: coordinates in all formats, antenna heights, radio
// parameters with unit-aware fields, climate/k-factor. Defaults everywhere; an
// expert can override every constant.
ScrollView {
    id: panel
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    AutoDesignDialog { id: autoDesignDialog }

    ColumnLayout {
        width: panel.availableWidth
        spacing: 6

        component SiteEditor: PanelSection {
            id: siteSection
            property int siteIndex: 0
            property double lat: 0
            property double lon: 0
            property double agl: 4

            RowLayout {
                Layout.fillWidth: true
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22
                    color: Theme.inputBg
                    border.color: coordInput.activeFocus ? Theme.accent : Theme.border
                    TextInput {
                        id: coordInput
                        anchors.fill: parent
                        anchors.margins: 3
                        color: Theme.text
                        font.family: Theme.mono
                        font.pixelSize: Theme.fontSizeSmall
                        verticalAlignment: TextInput.AlignVCenter
                        selectByMouse: true
                        // 'text' must not be bound directly: typing into a TextInput
                        // breaks the binding for good, after which the field would
                        // never follow the model again (dragging the pin, loading a
                        // project). Track the canonical string separately and push it
                        // in unless the operator has uncommitted edits sitting in the
                        // field — merely having the caret here must not freeze it.
                        property string canonical:
                            controller.coordinateFormats(siteSection.lat, siteSection.lon).dd
                        property bool edited: false
                        onTextEdited: edited = true
                        onCanonicalChanged: if (!edited) text = canonical
                        Component.onCompleted: text = canonical
                        onActiveFocusChanged: if (!activeFocus) { edited = false; text = canonical }
                        onAccepted: {
                            controller.parseCoordinateToSite(siteSection.siteIndex, text)
                            // Accepted: show the canonical form of whatever the site is
                            // now. Rejected: the site is unchanged, so this restores it
                            // and the status bar carries the parse error.
                            edited = false
                            text = canonical
                        }
                    }
                }
            }
            // Live conversion between the four formats.
            Text {
                Layout.fillWidth: true
                property var f: controller.coordinateFormats(siteSection.lat, siteSection.lon)
                text: "DMS  " + f.dms + "\nMGRS " + f.mgrs + "\nUTM  " + f.utm
                color: Theme.textDim
                font.family: Theme.mono
                font.pixelSize: Theme.fontSizeSmall
            }
            NumberField {
                label: qsTr("Antenna height")
                suffix: "m AGL"
                value: siteSection.agl
                from: 0.5
                to: 500
                decimals: 1
                onEdited: (v) => siteSection.siteIndex === 0 ? controller.siteAAgl = v
                                                             : controller.siteBAgl = v
            }
        }

        SiteEditor {
            title: qsTr("Site A")
            helpTopic: "sites"
            siteIndex: 0
            lat: controller.siteALat
            lon: controller.siteALon
            agl: controller.siteAAgl
        }
        SiteEditor {
            title: qsTr("Site B")
            helpTopic: "sites"
            siteIndex: 1
            lat: controller.siteBLat
            lon: controller.siteBLon
            agl: controller.siteBAgl
        }

        PanelSection {
            title: qsTr("Radio")
            helpTopic: "radio"

            // Auto-design: fill every radio field from the path geometry.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                radius: 3
                color: autoArea.pressed ? Qt.darker(Theme.accent, 1.15)
                       : autoArea.containsMouse ? Theme.accent : Qt.alpha(Theme.accent, 0.85)
                Row {
                    anchors.centerIn: parent
                    spacing: 6
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "⚙"
                        color: "#101010"
                        font.pixelSize: Theme.fontSize + 2
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Auto-design radio from geometry")
                        color: "#101010"
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                    }
                }
                MouseArea {
                    id: autoArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        const r = controller.autoDesignRadio()
                        autoDesignDialog.showResult(r)
                    }
                }
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("Solves the whole radio (band, dish, gain, power, modulation) "
                           + "for the target availability at the current data rate.")
                color: Theme.textDim
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("Antenna preset")
                    color: Theme.textDim
                    font.pixelSize: Theme.fontSizeSmall
                    Layout.preferredWidth: 108
                }
                ComboBox {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    model: controller.antennaPresets
                    font.pixelSize: Theme.fontSizeSmall
                    onActivated: (i) => controller.applyAntennaPreset(i)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("Radio preset")
                    color: Theme.textDim
                    font.pixelSize: Theme.fontSizeSmall
                    Layout.preferredWidth: 108
                }
                ComboBox {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    model: controller.radioPresets
                    font.pixelSize: Theme.fontSizeSmall
                    onActivated: (i) => controller.applyRadioPreset(i)
                }
            }
            NumberField {
                label: qsTr("Frequency")
                suffix: "GHz"
                value: controller.frequencyGHz
                from: 0.1
                to: 20
                decimals: 4
                onEdited: (v) => controller.frequencyGHz = v
            }
            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("TX power")
                    color: Theme.textDim
                    font.pixelSize: Theme.fontSizeSmall
                    Layout.preferredWidth: 108
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22
                    color: Theme.inputBg
                    border.color: powerInput.activeFocus ? Theme.accent : Theme.border
                    TextInput {
                        id: powerInput
                        anchors.fill: parent
                        anchors.margins: 3
                        color: Theme.text
                        font.family: Theme.mono
                        font.pixelSize: Theme.fontSize
                        verticalAlignment: TextInput.AlignVCenter
                        selectByMouse: true
                        text: controller.txPowerText
                        onAccepted: controller.txPowerText = text
                    }
                }
                Text {
                    text: controller.txPowerDbm.toFixed(1) + " dBm"
                    color: Theme.textDim
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.mono
                }
            }
            NumberField {
                label: qsTr("Antenna gain A")
                suffix: "dBi"
                value: controller.gainA
                from: 0; to: 70; decimals: 1
                onEdited: (v) => controller.gainA = v
            }
            NumberField {
                label: qsTr("Antenna gain B")
                suffix: "dBi"
                value: controller.gainB
                from: 0; to: 70; decimals: 1
                onEdited: (v) => controller.gainB = v
            }
            NumberField {
                label: qsTr("Antenna diameter")
                suffix: "m"
                value: controller.antennaDiameter
                from: 0.3; to: 30; decimals: 1
                tooltipText: qsTr("Used by the diversity separation calculator")
                onEdited: (v) => controller.antennaDiameter = v
            }
            NumberField {
                label: qsTr("Line loss A")
                suffix: "dB"
                value: controller.lineLossA
                from: 0; to: 20; decimals: 1
                onEdited: (v) => controller.lineLossA = v
            }
            NumberField {
                label: qsTr("Line loss B")
                suffix: "dB"
                value: controller.lineLossB
                from: 0; to: 20; decimals: 1
                onEdited: (v) => controller.lineLossB = v
            }
            NumberField {
                label: qsTr("Noise figure")
                suffix: "dB"
                value: controller.noiseFigure
                from: 0; to: 20; decimals: 1
                onEdited: (v) => controller.noiseFigure = v
            }
            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("Modulation")
                    color: Theme.textDim
                    font.pixelSize: Theme.fontSizeSmall
                    Layout.preferredWidth: 108
                }
                ComboBox {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    model: controller.modulationNames
                    currentIndex: controller.modulationIndex
                    font.pixelSize: Theme.fontSizeSmall
                    onActivated: (i) => controller.modulationIndex = i
                }
            }
            NumberField {
                label: qsTr("Data rate")
                suffix: "Mb/s"
                value: controller.dataRateMbps
                from: 0.001; to: 1000; decimals: 3
                onEdited: (v) => controller.dataRateMbps = v
            }
        }

        PanelSection {
            title: qsTr("Atmosphere & climate")
            helpTopic: "atmosphere"
            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("k-factor")
                    color: Theme.textDim
                    font.pixelSize: Theme.fontSizeSmall
                    Layout.preferredWidth: 108
                }
                CheckBox {
                    id: kAuto
                    checked: controller.kFactorAuto
                    text: qsTr("auto")
                    font.pixelSize: Theme.fontSizeSmall
                    palette.windowText: Theme.textDim
                    onToggled: controller.kFactorAuto = checked
                }
            }
            NumberField {
                label: qsTr("k value")
                suffix: ""
                value: controller.kFactor
                from: 0.3; to: 10; decimals: 3
                enabled: !controller.kFactorAuto
                opacity: enabled ? 1.0 : 0.5
                tooltipText: qsTr("k = 157/(157 - dN); sub-refractive k < 1 supported")
                onEdited: (v) => controller.kFactor = v
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("N0 = %1   dN = %2 N/km  (ITU maps)")
                          .arg(controller.seaLevelN0.toFixed(1))
                          .arg(controller.lapseRateDn.toFixed(1))
                color: Theme.textDim
                font.pixelSize: Theme.fontSizeSmall
                font.family: Theme.mono
            }
            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("Climate")
                    color: Theme.textDim
                    font.pixelSize: Theme.fontSizeSmall
                    Layout.preferredWidth: 108
                }
                ComboBox {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    model: controller.climateNames
                    currentIndex: controller.climateIndex
                    font.pixelSize: Theme.fontSizeSmall
                    onActivated: (i) => controller.climateIndex = i
                }
            }
        }

        PanelSection {
            title: qsTr("Terrain store")
            helpTopic: "terrain"
            Repeater {
                model: controller.terrainEntries
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: modelData.file + "  (" + modelData.format + ", "
                              + modelData.resolution.toFixed(0) + " m"
                              + (modelData.downloaded ? ", " + qsTr("downloaded") : "") + ")"
                        color: modelData.downloaded ? Theme.warn : Theme.text
                        font.pixelSize: Theme.fontSizeSmall
                        elide: Text.ElideMiddle
                    }
                    Text {
                        text: "✕"
                        color: Theme.bad
                        font.pixelSize: Theme.fontSizeSmall
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -4
                            onClicked: controller.removeTerrainEntry(modelData.file)
                        }
                    }
                }
            }
            Text {
                visible: controller.terrainEntries.length === 0
                text: qsTr("No terrain data. Drag & drop DTED/HGT/GeoTIFF onto the window.")
                color: Theme.warn
                font.pixelSize: Theme.fontSizeSmall
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }
    }
}
