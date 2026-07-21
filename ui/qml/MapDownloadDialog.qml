import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TropoLink

// Offline basemap download: pick area, source and zoom range; tiles are packed
// into a .mbtiles file that becomes the active basemap when done.
Dialog {
    id: dialog
    // Same reason as HelpDialog: a Popup has no parent until opened.
    parent: Overlay.overlay
    modal: true
    width: 460
    anchors.centerIn: parent
    padding: 12

    readonly property var appCtl: controller
    // Set by the caller before open(): current view and path corridor, degrees.
    property var viewBbox: ({ minLat: 0, maxLat: 0, minLon: 0, maxLon: 0 })
    property var pathBbox: ({ minLat: 0, maxLat: 0, minLon: 0, maxLon: 0 })

    readonly property var bbox: areaCombo.currentIndex === 0 ? viewBbox : pathBbox
    readonly property double tileEstimate: downloader.estimateTiles(
        bbox.minLat, bbox.maxLat, bbox.minLon, bbox.maxLon,
        zoomMin.value, zoomMax.value)
    readonly property string sourceId: sourceCombo.currentIndex === 1 ? "osm" : "opentopomap"

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
            text: qsTr("Download offline maps")
            color: Theme.text
            font.pixelSize: Theme.fontSize
            font.bold: true
        }
        HelpButton {
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            topic: "offline-maps"
        }
    }

    MapDownloader {
        id: downloader
        onFinished: (path, ok) => {
            if (ok && path.length > 0) {
                dialog.appCtl.mapBasemapPath = path
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 8

        component FieldLabel: Text {
            color: Theme.textDim
            font.pixelSize: Theme.fontSizeSmall
        }

        FieldLabel { text: qsTr("Area") }
        ComboBox {
            id: areaCombo
            Layout.fillWidth: true
            model: [qsTr("Current map view"), qsTr("Corridor along the A–B path")]
            font.pixelSize: Theme.fontSize
        }

        FieldLabel { text: qsTr("Source") }
        ComboBox {
            id: sourceCombo
            Layout.fillWidth: true
            model: [qsTr("OpenTopoMap (topographic, contours)"), qsTr("OpenStreetMap (general)")]
            font.pixelSize: Theme.fontSize
        }

        FieldLabel { text: qsTr("Zoom levels (coarse → detailed)") }
        RowLayout {
            spacing: 8
            SpinBox {
                id: zoomMin
                from: 5
                to: zoomMax.value
                value: 8
                font.pixelSize: Theme.fontSize
            }
            Text { text: "–"; color: Theme.text }
            SpinBox {
                id: zoomMax
                from: zoomMin.value
                to: 16
                value: 13
                font.pixelSize: Theme.fontSize
            }
            Item { Layout.fillWidth: true }
        }

        Text {
            Layout.fillWidth: true
            text: dialog.tileEstimate > 30000
                  ? qsTr("Estimate: %1 tiles — over the 30,000 limit, reduce area or zoom")
                        .arg(Math.round(dialog.tileEstimate))
                  : qsTr("Estimate: %1 tiles, ~%2 MB")
                        .arg(Math.round(dialog.tileEstimate))
                        .arg((dialog.tileEstimate * 0.02).toFixed(0))
            color: dialog.tileEstimate > 30000 ? Theme.bad : Theme.textDim
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.Wrap
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        ProgressBar {
            Layout.fillWidth: true
            visible: downloader.running || downloader.done > 0
            from: 0
            to: Math.max(1, downloader.total)
            value: downloader.done + downloader.failed
        }
        Text {
            Layout.fillWidth: true
            text: downloader.statusText
            color: Theme.textDim
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Item { Layout.fillWidth: true }
            Button {
                text: downloader.running ? qsTr("Cancel") : qsTr("Start download")
                enabled: downloader.running || (dialog.tileEstimate > 0 && dialog.tileEstimate <= 30000)
                font.pixelSize: Theme.fontSize
                onClicked: {
                    if (downloader.running) {
                        downloader.cancel()
                    } else {
                        const stamp = new Date().toISOString().slice(0, 10)
                        const path = downloader.defaultOutputDir() + "/" + dialog.sourceId
                                     + "_" + stamp + ".mbtiles"
                        downloader.start(dialog.sourceId,
                                         dialog.bbox.minLat, dialog.bbox.maxLat,
                                         dialog.bbox.minLon, dialog.bbox.maxLon,
                                         zoomMin.value, zoomMax.value, path)
                    }
                }
            }
            Button {
                text: qsTr("Close")
                font.pixelSize: Theme.fontSize
                onClicked: dialog.close()
            }
        }
    }
}
