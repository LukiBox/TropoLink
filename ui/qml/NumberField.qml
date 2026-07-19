import QtQuick
import QtQuick.Layouts

// Labeled numeric input with unit suffix and instant validation.
RowLayout {
    id: field
    property string label: ""
    property string suffix: ""
    property double value: 0
    property double from: -1e12
    property double to: 1e12
    property int decimals: 2
    property string tooltipText: ""
    signal edited(double newValue)
    spacing: 6
    Layout.fillWidth: true

    Text {
        text: field.label
        color: Theme.textDim
        font.pixelSize: Theme.fontSizeSmall
        Layout.preferredWidth: 108
        elide: Text.ElideRight

        MouseArea {
            id: hoverArea
            anchors.fill: parent
            hoverEnabled: true
        }
        Rectangle {
            visible: hoverArea.containsMouse && field.tooltipText.length > 0
            x: 0; y: parent.height + 2
            z: 99
            width: tipText.implicitWidth + 12
            height: tipText.implicitHeight + 8
            color: Theme.panelAlt
            border.color: Theme.border
            Text {
                id: tipText
                anchors.centerIn: parent
                text: field.tooltipText
                color: Theme.text
                font.pixelSize: Theme.fontSizeSmall
            }
        }
    }
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 22
        color: Theme.inputBg
        border.color: input.activeFocus ? Theme.accent : (input.acceptableInput ? Theme.border : Theme.bad)
        border.width: 1

        TextInput {
            id: input
            anchors.fill: parent
            anchors.margins: 3
            color: Theme.text
            font.pixelSize: Theme.fontSize
            font.family: Theme.mono
            verticalAlignment: TextInput.AlignVCenter
            selectByMouse: true
            validator: DoubleValidator {
                bottom: field.from
                top: field.to
                decimals: field.decimals
                notation: DoubleValidator.StandardNotation
                locale: "C"
            }
            text: field.value.toFixed(field.decimals)
            onEditingFinished: {
                const v = parseFloat(text.replace(",", "."))
                if (!isNaN(v) && v >= field.from && v <= field.to)
                    field.edited(v)
                else
                    text = field.value.toFixed(field.decimals)
            }
            Connections {
                target: field
                function onValueChanged() {
                    if (!input.activeFocus)
                        input.text = field.value.toFixed(field.decimals)
                }
            }
        }
    }
    Text {
        text: field.suffix
        color: Theme.textDim
        font.pixelSize: Theme.fontSizeSmall
        Layout.preferredWidth: 34
    }
}
