import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Design solver: lock a target availability, solve for the missing variable.
PanelSection {
    id: solver
    title: qsTr("Design solver")
    property var result: null

    NumberField {
        label: qsTr("Target availability")
        suffix: "%"
        value: controller.targetAvailability
        from: 50
        to: 99.9999
        decimals: 4
        onEdited: (v) => controller.targetAvailability = v
    }
    RowLayout {
        Layout.fillWidth: true
        CheckBox {
            id: wmCheck
            checked: controller.targetWorstMonth
            text: qsTr("worst month")
            font.pixelSize: Theme.fontSizeSmall
            palette.windowText: Theme.textDim
            onToggled: controller.targetWorstMonth = checked
        }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 4
        component SolveButton: Rectangle {
            property string label: ""
            property int mode: 0
            Layout.fillWidth: true
            Layout.preferredHeight: 26
            color: solveArea.pressed ? Theme.panelAlt : Theme.inputBg
            border.color: Theme.border
            Text {
                anchors.centerIn: parent
                text: parent.label
                color: Theme.text
                font.pixelSize: Theme.fontSizeSmall
            }
            MouseArea {
                id: solveArea
                anchors.fill: parent
                onClicked: solver.result = controller.solveFor(parent.mode)
            }
        }
        SolveButton { label: qsTr("Solve power"); mode: 0 }
        SolveButton { label: qsTr("Solve gain"); mode: 1 }
        SolveButton { label: qsTr("Solve rate"); mode: 2 }
    }
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: resultText.implicitHeight + 12
        visible: solver.result !== null
        color: Theme.inputBg
        border.color: (solver.result?.ok ?? false) ? Theme.good : Theme.bad
        Text {
            id: resultText
            anchors.fill: parent
            anchors.margins: 6
            color: Theme.text
            font.pixelSize: Theme.fontSizeSmall
            font.family: Theme.mono
            wrapMode: Text.WordWrap
            text: {
                const r = solver.result
                if (!r) return ""
                let lines = []
                lines.push(qsTr("Required margin: %1 dB (now %2 dB)")
                           .arg(r.requiredMargin.toFixed(1)).arg(r.currentMargin.toFixed(1)))
                if (r.txDbm !== undefined)
                    lines.push(qsTr("Required TX: %1 dBm (%2 W)")
                               .arg(r.txDbm.toFixed(1)).arg(r.txWatts.toFixed(0)))
                if (r.gainDbi !== undefined)
                    lines.push(qsTr("Required gain: %1 dBi per antenna").arg(r.gainDbi.toFixed(1)))
                if (r.rateMbps !== undefined)
                    lines.push(qsTr("Max data rate: %1 Mb/s").arg(r.rateMbps.toFixed(3)))
                if (r.note.length > 0)
                    lines.push(r.note)
                return lines.join("\n")
            }
        }
    }
}
