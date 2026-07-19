import QtCore
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import TropoLink

// Single main window, dockable panels, dark theme default, keyboard-reachable.
ApplicationWindow {
    id: window
    width: 1560
    height: 960
    visibility: Window.Maximized
    visible: true
    title: "TropoLink " + Qt.application.version
    color: Theme.bg

    Component.onCompleted: {
        Theme.dark = controller.darkTheme
        if (!settings.tourSeen) {
            tour.open()
            settings.tourSeen = true
        }
    }

    Settings {
        id: settings
        property bool tourSeen: false
    }

    // --- keyboard shortcuts ---------------------------------------------------
    Shortcut { sequence: StandardKey.Open;  onActivated: openDialog.open() }
    Shortcut { sequence: StandardKey.Save;  onActivated: saveDialog.open() }
    Shortcut { sequence: "Ctrl+R";          onActivated: reportDialog.open() }
    Shortcut { sequence: "Ctrl+E";          onActivated: kmlDialog.open() }
    Shortcut { sequence: "Ctrl+L";          onActivated: toolbar.toggleLanguage() }
    Shortcut { sequence: "Ctrl+T";          onActivated: toolbar.toggleTheme() }
    Shortcut { sequence: "F1";              onActivated: tour.open() }
    Shortcut { sequence: "Ctrl+N";          onActivated: controller.loadReferenceProject() }

    // --- drag & drop terrain import -------------------------------------------
    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            if (drop.hasUrls)
                controller.importTerrainFiles(drop.urls)
        }
    }

    // --- dialogs ---------------------------------------------------------------
    FileDialog {
        id: openDialog
        title: qsTr("Open project")
        nameFilters: [qsTr("TropoLink projects (*.tlk)")]
        onAccepted: controller.loadProjectFile(selectedFile)
    }
    FileDialog {
        id: saveDialog
        title: qsTr("Save project")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("TropoLink projects (*.tlk)")]
        defaultSuffix: "tlk"
        onAccepted: controller.saveProjectFile(selectedFile)
    }
    FileDialog {
        id: reportDialog
        title: qsTr("Write link-design report (PDF)")
        fileMode: FileDialog.SaveFile
        nameFilters: ["PDF (*.pdf)"]
        defaultSuffix: "pdf"
        onAccepted: {
            const url = selectedFile
            mapPane.grabSnapshot(function (image) {
                controller.generateReport(url, image)
            })
        }
    }
    FileDialog {
        id: kmlDialog
        title: qsTr("Export KML")
        fileMode: FileDialog.SaveFile
        nameFilters: ["KML (*.kml)"]
        defaultSuffix: "kml"
        onAccepted: controller.exportKml(selectedFile)
    }
    FileDialog {
        id: profileCsvDialog
        title: qsTr("Export profile CSV")
        fileMode: FileDialog.SaveFile
        nameFilters: ["CSV (*.csv)"]
        defaultSuffix: "csv"
        onAccepted: controller.exportProfileCsv(selectedFile)
    }
    FileDialog {
        id: budgetCsvDialog
        title: qsTr("Export budget CSV")
        fileMode: FileDialog.SaveFile
        nameFilters: ["CSV (*.csv)"]
        defaultSuffix: "csv"
        onAccepted: controller.exportBudgetCsv(selectedFile)
    }

    // --- layout ----------------------------------------------------------------
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Toolbar
        Rectangle {
            id: toolbar
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: Theme.panelAlt
            border.color: Theme.border

            function toggleLanguage() {
                controller.languageCode = controller.languageCode === "pl" ? "en" : "pl"
                restartHint.visible = true
            }
            function toggleTheme() {
                Theme.dark = !Theme.dark
                controller.darkTheme = Theme.dark
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 4

                component ToolButton2: Rectangle {
                    property string label: ""
                    property string shortcutHint: ""
                    signal clicked()
                    Layout.preferredWidth: btnLabel.implicitWidth + 18
                    Layout.preferredHeight: 24
                    color: btnArea.containsMouse ? Theme.panel : "transparent"
                    border.color: btnArea.containsMouse ? Theme.border : "transparent"
                    Text {
                        id: btnLabel
                        anchors.centerIn: parent
                        text: parent.label
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeSmall
                    }
                    MouseArea {
                        id: btnArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: parent.clicked()
                    }
                }

                ToolButton2 { label: qsTr("Open"); onClicked: openDialog.open() }
                ToolButton2 { label: qsTr("Save"); onClicked: saveDialog.open() }
                ToolButton2 { label: qsTr("Reference"); onClicked: controller.loadReferenceProject() }
                Rectangle { width: 1; Layout.preferredHeight: 20; color: Theme.border }
                ToolButton2 { label: qsTr("KML"); onClicked: kmlDialog.open() }
                ToolButton2 { label: qsTr("Profile CSV"); onClicked: profileCsvDialog.open() }
                ToolButton2 { label: qsTr("Budget CSV"); onClicked: budgetCsvDialog.open() }
                Rectangle { width: 1; Layout.preferredHeight: 20; color: Theme.border }
                Rectangle {
                    Layout.preferredWidth: reportLabel.implicitWidth + 22
                    Layout.preferredHeight: 26
                    color: Theme.accent
                    Text {
                        id: reportLabel
                        anchors.centerIn: parent
                        text: qsTr("Report PDF")
                        color: "#101010"
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                    }
                    MouseArea { anchors.fill: parent; onClicked: reportDialog.open() }
                }
                Item { Layout.fillWidth: true }
                Text {
                    id: restartHint
                    visible: false
                    text: qsTr("language change applies at restart")
                    color: Theme.warn
                    font.pixelSize: Theme.fontSizeSmall
                }
                ToolButton2 {
                    label: controller.languageCode === "pl" ? "PL" : "EN"
                    onClicked: toolbar.toggleLanguage()
                }
                ToolButton2 {
                    label: Theme.dark ? qsTr("Dark") : qsTr("Light")
                    onClicked: toolbar.toggleTheme()
                }
                ToolButton2 { label: "?"; onClicked: tour.open() }
            }
        }

        // Main area: map + right panels above the profile dock.
        SplitView {
            id: vsplit
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Vertical

            SplitView {
                id: hsplit
                SplitView.fillHeight: true
                orientation: Qt.Horizontal

                Rectangle {
                    id: mapRect
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 400
                    color: Theme.bg
                    clip: true // tile nodes extend past the viewport
                    MapView {
                        id: mapPane
                        anchors.fill: parent
                        hoverProfileDistance: profileDock.hoverDistance
                    }
                }

                Rectangle {
                    id: sitesRect
                    SplitView.preferredWidth: 330
                    SplitView.minimumWidth: 260
                    color: Theme.panel
                    SitesPanel {
                        anchors.fill: parent
                        anchors.margins: 4
                    }
                }

                Rectangle {
                    id: resultsRect
                    SplitView.preferredWidth: 360
                    SplitView.minimumWidth: 280
                    color: Theme.panel
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 6
                        ResultsPanel {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                        SolverPanel { Layout.fillWidth: true }
                    }
                }
            }

            ProfilePanel {
                id: profileDock
                SplitView.preferredHeight: 240
                SplitView.minimumHeight: 140
            }
        }

        StatusBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
        }
    }

    OnboardingTour { id: tour }
}
