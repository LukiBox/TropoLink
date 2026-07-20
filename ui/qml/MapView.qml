import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import TropoLink

// The operator's primary surface: offline tile map with pins, link line, common
// volume, MGRS grid, horizon fans, coordinate readout, search and measurement.
Item {
    id: mapView
    property alias map: tiles
    property double hoverProfileDistance: -1
    property bool measureMode: false
    property var measureStart: null
    property string measureResult: ""
    // The context property by its own name would self-resolve inside items that have
    // a 'controller' property; capture it once at this scope.
    readonly property var appCtl: controller

    function grabSnapshot(callback) {
        mapView.grabToImage(function (result) { callback(result.image) })
    }

    // Paper sheet under the tiles: areas without DEM coverage read as blank map
    // paper instead of holes.
    Rectangle {
        anchors.fill: parent
        color: Theme.dark ? "#23262b" : "#f4f1e7"
    }

    TileMapItem {
        id: tiles
        anchors.fill: parent
        controller: mapView.appCtl
        darkTheme: Theme.dark
        centerLat: 51.97
        centerLon: 15.27
        zoom: 8.0
        basemapPath: mapView.appCtl.mapBasemapPath
        onlineSource: mapView.appCtl.mapOnlineSource
    }

    MapOverlayItem {
        id: overlay
        anchors.fill: parent
        map: tiles
        controller: mapView.appCtl
        darkTheme: Theme.dark
        showMgrsGrid: gridToggle.checked
        hoverDistanceM: mapView.hoverProfileDistance
    }

    // --- interaction ---------------------------------------------------------
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        property point pressPos
        property bool dragging: false
        cursorShape: mapView.measureMode ? Qt.CrossCursor : Qt.OpenHandCursor

        onPressed: (e) => {
            pressPos = Qt.point(e.x, e.y)
            dragging = false
        }
        onPositionChanged: (e) => {
            const c = tiles.toCoordinate(Qt.point(e.x, e.y))
            readout.update(c.lat, c.lon)
            if (e.buttons & Qt.LeftButton) {
                dragging = true
                tiles.pan(e.x - pressPos.x, e.y - pressPos.y)
                pressPos = Qt.point(e.x, e.y)
            }
        }
        onClicked: (e) => {
            if (e.button === Qt.RightButton) {
                const c = tiles.toCoordinate(Qt.point(e.x, e.y))
                contextMenu.lat = c.lat
                contextMenu.lon = c.lon
                contextMenu.popup()
            } else if (mapView.measureMode && !dragging) {
                const c = tiles.toCoordinate(Qt.point(e.x, e.y))
                if (mapView.measureStart === null) {
                    mapView.measureStart = c
                    overlay.measurePoints = [c]
                } else {
                    overlay.measurePoints = [mapView.measureStart, c]
                    mapView.measureResult = controller.measure(
                        mapView.measureStart.lat, mapView.measureStart.lon, c.lat, c.lon)
                    mapView.measureStart = null
                }
            }
        }
        onWheel: (e) => tiles.zoomAround(Qt.point(e.x, e.y), e.angleDelta.y / 240.0)
        onDoubleClicked: (e) => tiles.zoomAround(Qt.point(e.x, e.y), 1.0)
    }

    // --- draggable site pins -------------------------------------------------
    component SitePin: Item {
        id: pin
        property int siteIndex: 0
        property double lat: 0
        property double lon: 0
        property color pinColor: Theme.accent
        property string label: "A"
        width: 26
        height: 34
        z: 10

        function reposition() {
            const p = tiles.fromCoordinate(lat, lon)
            x = p.x - width / 2
            y = p.y - height
        }
        Connections {
            target: tiles
            function onViewChanged() { pin.reposition() }
        }
        onLatChanged: reposition()
        onLonChanged: reposition()
        Component.onCompleted: reposition()

        Canvas {
            anchors.fill: parent
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.fillStyle = pin.pinColor
                ctx.strokeStyle = Theme.dark ? "#ffffff" : "#000000"
                ctx.lineWidth = 1.4
                ctx.beginPath()
                ctx.arc(width / 2, width / 2, width / 2 - 2, 0, 2 * Math.PI)
                ctx.fill()
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(width / 2 - 4, width - 3)
                ctx.lineTo(width / 2, height - 1)
                ctx.lineTo(width / 2 + 4, width - 3)
                ctx.closePath()
                ctx.fill()
            }
            Component.onCompleted: requestPaint()
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 4
            text: pin.label
            color: "#101010"
            font.bold: true
            font.pixelSize: 13
        }
        MouseArea {
            anchors.fill: parent
            drag.target: pin
            cursorShape: Qt.SizeAllCursor
            onPositionChanged: {
                if (drag.active) {
                    const c = tiles.toCoordinate(Qt.point(pin.x + pin.width / 2, pin.y + pin.height))
                    controller.setSiteFromMap(pin.siteIndex, c.lat, c.lon, true)
                }
            }
            onReleased: {
                const c = tiles.toCoordinate(Qt.point(pin.x + pin.width / 2, pin.y + pin.height))
                controller.setSiteFromMap(pin.siteIndex, c.lat, c.lon, false)
            }
        }
    }

    SitePin {
        siteIndex: 0
        label: "A"
        pinColor: "#ffb43c"
        lat: controller.siteALat
        lon: controller.siteALon
    }
    SitePin {
        siteIndex: 1
        label: "B"
        pinColor: "#64c8ff"
        lat: controller.siteBLat
        lon: controller.siteBLon
    }

    Menu {
        id: contextMenu
        property double lat: 0
        property double lon: 0
        MenuItem {
            text: qsTr("Set Site A here")
            onTriggered: controller.setSiteFromMap(0, contextMenu.lat, contextMenu.lon, false)
        }
        MenuItem {
            text: qsTr("Set Site B here")
            onTriggered: controller.setSiteFromMap(1, contextMenu.lat, contextMenu.lon, false)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Measure from here")
            onTriggered: {
                mapView.measureMode = true
                mapView.measureStart = { lat: contextMenu.lat, lon: contextMenu.lon }
                overlay.measurePoints = [mapView.measureStart]
            }
        }
        MenuItem {
            visible: !controller.airgap
            height: visible ? implicitHeight : 0
            text: qsTr("Download SRTM for this view")
            enabled: !controller.downloading
            onTriggered: {
                const tl = tiles.toCoordinate(Qt.point(0, 0))
                const br = tiles.toCoordinate(Qt.point(mapView.width, mapView.height))
                controller.downloadSrtmForRegion(br.lat, tl.lat, tl.lon, br.lon)
            }
        }
        MenuItem {
            text: qsTr("Import terrain files...")
            onTriggered: terrainDialog.open()
        }
        MenuItem {
            text: qsTr("Load basemap (MBTiles)...")
            onTriggered: basemapDialog.open()
        }
        MenuItem {
            visible: !controller.airgap
            height: visible ? implicitHeight : 0
            text: qsTr("Download offline maps...")
            onTriggered: mapView.openDownloadDialog()
        }
    }

    FileDialog {
        id: terrainDialog
        title: qsTr("Import terrain (DTED / SRTM HGT / GeoTIFF)")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Elevation data (*.dt0 *.dt1 *.dt2 *.hgt *.tif *.tiff)"),
                      qsTr("All files (*)")]
        onAccepted: controller.importTerrainFiles(selectedFiles)
    }
    FileDialog {
        id: basemapDialog
        title: qsTr("Open MBTiles basemap")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("MBTiles (*.mbtiles)")]
        onAccepted: mapView.appCtl.mapBasemapPath =
                        selectedFile.toString().replace("file:///", "")
    }

    MapDownloadDialog {
        id: downloadDialog
        parent: mapView
    }

    function openDownloadDialog() {
        const tl = tiles.toCoordinate(Qt.point(0, 0))
        const br = tiles.toCoordinate(Qt.point(mapView.width, mapView.height))
        downloadDialog.viewBbox = { minLat: Math.min(tl.lat, br.lat),
                                    maxLat: Math.max(tl.lat, br.lat),
                                    minLon: Math.min(tl.lon, br.lon),
                                    maxLon: Math.max(tl.lon, br.lon) }
        const c = mapView.appCtl
        // ~25 km corridor around the path bbox.
        const dLat = 0.25
        const dLon = 0.25 / Math.max(0.2, Math.cos(
            (c.siteALat + c.siteBLat) / 2 * Math.PI / 180))
        downloadDialog.pathBbox = {
            minLat: Math.min(c.siteALat, c.siteBLat) - dLat,
            maxLat: Math.max(c.siteALat, c.siteBLat) + dLat,
            minLon: Math.min(c.siteALon, c.siteBLon) - dLon,
            maxLon: Math.max(c.siteALon, c.siteBLon) + dLon }
        downloadDialog.open()
    }

    // --- search box (accepts any coordinate format) --------------------------
    Rectangle {
        x: 10
        y: 10
        width: 300
        height: 28
        color: Theme.panel
        opacity: 0.95
        border.color: searchInput.activeFocus ? Theme.accent : Theme.border

        TextInput {
            id: searchInput
            anchors.fill: parent
            anchors.margins: 5
            color: Theme.text
            font.pixelSize: Theme.fontSize
            font.family: Theme.mono
            verticalAlignment: TextInput.AlignVCenter
            selectByMouse: true
            property string placeholder: qsTr("Search: 51.5N 15.3E / 33UXT... / 51°30'N...")
            Text {
                anchors.fill: parent
                verticalAlignment: Text.AlignVCenter
                visible: !searchInput.text && !searchInput.activeFocus
                text: searchInput.placeholder
                color: Theme.textDim
                font.pixelSize: Theme.fontSizeSmall
            }
            onAccepted: {
                const r = controller.parseCoordinate(text)
                if (r.ok) {
                    tiles.centerOn(r.lat, r.lon, Math.max(tiles.zoom, 10))
                    searchFail.visible = false
                } else {
                    searchFail.visible = true
                }
            }
        }
        Text {
            id: searchFail
            visible: false
            anchors.top: parent.bottom
            anchors.topMargin: 2
            text: qsTr("Unrecognized coordinate")
            color: Theme.bad
            font.pixelSize: Theme.fontSizeSmall
        }
    }

    // --- toggles, measurement result -----------------------------------------
    Row {
        x: 320
        y: 10
        spacing: 6
        Rectangle {
            width: gridLabel.width + 16
            height: 28
            color: gridToggle.checked ? Theme.panelAlt : Theme.panel
            opacity: 0.95
            border.color: Theme.border
            Text {
                id: gridLabel
                anchors.centerIn: parent
                text: qsTr("MGRS grid")
                color: gridToggle.checked ? Theme.accent : Theme.textDim
                font.pixelSize: Theme.fontSizeSmall
            }
            MouseArea {
                id: gridToggle
                property bool checked: true
                anchors.fill: parent
                onClicked: checked = !checked
            }
        }
        Rectangle {
            width: measureLabel.width + 16
            height: 28
            color: mapView.measureMode ? Theme.panelAlt : Theme.panel
            opacity: 0.95
            border.color: Theme.border
            Text {
                id: measureLabel
                anchors.centerIn: parent
                text: mapView.measureResult.length > 0 && mapView.measureMode
                      ? mapView.measureResult
                      : qsTr("Measure")
                color: mapView.measureMode ? Theme.accent : Theme.textDim
                font.pixelSize: Theme.fontSizeSmall
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    mapView.measureMode = !mapView.measureMode
                    if (!mapView.measureMode) {
                        overlay.measurePoints = []
                        mapView.measureResult = ""
                        mapView.measureStart = null
                    }
                }
            }
        }

        // Basemap selector: offline terrain rendering / online sources.
        Rectangle {
            width: sourceLabel.width + 26
            height: 28
            color: Theme.panel
            opacity: 0.95
            border.color: Theme.border
            Text {
                id: sourceLabel
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 8
                text: {
                    if (tiles.hasBasemap) return qsTr("Basemap: pack")
                    if (tiles.onlineSource === "opentopomap") return "OpenTopoMap"
                    if (tiles.onlineSource === "osm") return "OSM"
                    return qsTr("Terrain (offline)")
                }
                color: Theme.text
                font.pixelSize: Theme.fontSizeSmall
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                text: "▾"
                color: Theme.textDim
                font.pixelSize: 9
            }
            MouseArea {
                anchors.fill: parent
                onClicked: sourceMenu.popup()
            }
            Menu {
                id: sourceMenu
                MenuItem {
                    text: qsTr("Terrain rendering (offline)")
                    onTriggered: {
                        mapView.appCtl.mapBasemapPath = ""
                        mapView.appCtl.mapOnlineSource = ""
                    }
                }
                MenuItem {
                    visible: !mapView.appCtl.airgap
                    height: visible ? implicitHeight : 0
                    text: qsTr("OpenTopoMap (online, cached)")
                    onTriggered: {
                        mapView.appCtl.mapBasemapPath = ""
                        mapView.appCtl.mapOnlineSource = "opentopomap"
                    }
                }
                MenuItem {
                    visible: !mapView.appCtl.airgap
                    height: visible ? implicitHeight : 0
                    text: qsTr("OpenStreetMap (online, cached)")
                    onTriggered: {
                        mapView.appCtl.mapBasemapPath = ""
                        mapView.appCtl.mapOnlineSource = "osm"
                    }
                }
                MenuSeparator {}
                MenuItem {
                    text: qsTr("Load basemap (MBTiles)...")
                    onTriggered: basemapDialog.open()
                }
                MenuItem {
                    visible: tiles.hasBasemap
                    height: visible ? implicitHeight : 0
                    text: qsTr("Unload basemap pack")
                    onTriggered: mapView.appCtl.mapBasemapPath = ""
                }
                MenuItem {
                    visible: !mapView.appCtl.airgap
                    height: visible ? implicitHeight : 0
                    text: qsTr("Download offline maps...")
                    onTriggered: mapView.openDownloadDialog()
                }
            }
        }

        Rectangle {
            visible: !mapView.appCtl.airgap
            width: dlLabel.width + 16
            height: 28
            color: Theme.panel
            opacity: 0.95
            border.color: Theme.border
            Text {
                id: dlLabel
                anchors.centerIn: parent
                text: qsTr("Download maps")
                color: Theme.textDim
                font.pixelSize: Theme.fontSizeSmall
            }
            MouseArea {
                anchors.fill: parent
                onClicked: mapView.openDownloadDialog()
            }
        }

        Rectangle {
            width: 28
            height: 28
            color: Theme.panel
            opacity: 0.95
            border.color: Theme.border
            HelpButton {
                anchors.centerIn: parent
                topic: "map"
            }
        }
    }

    // Source attribution (license requirement for OSM/OpenTopoMap layers).
    Text {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 2
        text: tiles.attribution
        color: Theme.textDim
        font.pixelSize: 9
        style: Text.Outline
        styleColor: Theme.dark ? "#000000" : "#ffffff"
    }

    // --- scale bar + north arrow ---------------------------------------------
    Item {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 12
        width: 160
        height: 40

        property double scaleMeters: {
            const target = tiles.metersPerPixel() * 100
            const steps = [100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000]
            for (let s of steps)
                if (s >= target) return s
            return 500000
        }
        Rectangle {
            id: scaleBar
            anchors.bottom: scaleText.top
            anchors.bottomMargin: 3
            width: parent.scaleMeters / tiles.metersPerPixel()
            height: 4
            color: Theme.text
            border.color: Theme.dark ? "#000000" : "#ffffff"
        }
        Text {
            id: scaleText
            anchors.bottom: parent.bottom
            text: parent.scaleMeters >= 1000 ? (parent.scaleMeters / 1000) + " km"
                                             : parent.scaleMeters + " m"
            color: Theme.text
            font.pixelSize: Theme.fontSizeSmall
            style: Text.Outline
            styleColor: Theme.dark ? "#000000" : "#ffffff"
        }
    }
    Canvas {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        width: 30
        height: 40
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = Theme.dark ? "#d8dee5" : "#1a2027"
            ctx.fillStyle = Theme.accent
            ctx.lineWidth = 1.5
            ctx.beginPath()
            ctx.moveTo(width / 2, 4)
            ctx.lineTo(width / 2 - 7, 26)
            ctx.lineTo(width / 2, 20)
            ctx.lineTo(width / 2 + 7, 26)
            ctx.closePath()
            ctx.fill()
            ctx.stroke()
            ctx.fillStyle = Theme.dark ? "#d8dee5" : "#1a2027"
            ctx.font = "10px sans-serif"
            ctx.textAlign = "center"
            ctx.fillText("N", width / 2, 38)
        }
        Component.onCompleted: requestPaint()
        Connections {
            target: Theme
            function onDarkChanged() { parent.requestPaint() }
        }
    }

    // --- cursor coordinate readout in all four formats ------------------------
    Rectangle {
        id: readout
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12
        width: readoutColumn.width + 16
        height: readoutColumn.height + 10
        color: Theme.panel
        opacity: 0.93
        border.color: Theme.border

        property var formats: ({ dd: "-", dms: "-", mgrs: "-", utm: "-" })
        function update(lat, lon) {
            formats = controller.coordinateFormats(lat, lon)
        }
        Column {
            id: readoutColumn
            anchors.centerIn: parent
            spacing: 1
            Text { text: readout.formats.dd;   color: Theme.text; font.family: Theme.mono; font.pixelSize: Theme.fontSizeSmall }
            Text { text: readout.formats.dms;  color: Theme.text; font.family: Theme.mono; font.pixelSize: Theme.fontSizeSmall }
            Text { text: "MGRS " + readout.formats.mgrs; color: Theme.accent; font.family: Theme.mono; font.pixelSize: Theme.fontSizeSmall }
            Text { text: "UTM " + readout.formats.utm;  color: Theme.text; font.family: Theme.mono; font.pixelSize: Theme.fontSizeSmall }
        }
    }
}
