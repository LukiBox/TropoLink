import QtQuick
import QtQuick.Layouts

// Link-budget cascade: EIRP -> losses -> RSL -> noise -> SNR -> margin.
Item {
    id: chart
    property var items: controller.waterfall
    implicitHeight: 190

    readonly property var labels: ({
        tx_power: qsTr("TX"),
        line_loss_tx: qsTr("Line A"),
        antenna_gain_tx: qsTr("Ant A"),
        eirp: qsTr("EIRP"),
        path_loss: qsTr("Path"),
        antenna_gain_rx: qsTr("Ant B"),
        line_loss_rx: qsTr("Line B"),
        rsl: qsTr("RSL"),
        noise_floor: qsTr("Noise"),
        median_snr: qsTr("SNR"),
        required_snr: qsTr("Req"),
        fade_margin: qsTr("Margin")
    })

    Row {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 2

        Repeater {
            model: chart.items
            delegate: Item {
                width: (chart.width - 8) / Math.max(1, chart.items.length) - 2
                height: chart.height - 8
                property var entry: modelData
                property bool isLevel: entry.isLevel
                property double v: entry.value

                // Level bars span from a fixed floor; step bars show magnitude.
                Rectangle {
                    anchors.bottom: labelText.top
                    anchors.bottomMargin: 2
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width - 2
                    // Map: levels -160..+100 dBm; steps by |value| (log-ish clamp).
                    height: {
                        const h = parent.height - 30
                        if (isLevel)
                            return Math.max(4, (v + 160) / 260 * h)
                        return Math.max(4, Math.min(1, Math.abs(v) / 220) * h)
                    }
                    color: {
                        if (entry.key === "fade_margin")
                            return v >= 0 ? Theme.good : Theme.bad
                        if (isLevel)
                            return Theme.ray
                        return v >= 0 ? Theme.good : Theme.bad
                    }
                    opacity: isLevel ? 1.0 : 0.75
                    border.color: Theme.border
                    border.width: isLevel ? 1 : 0

                    Text {
                        anchors.bottom: parent.top
                        anchors.bottomMargin: 1
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: (v >= 0 && !isLevel ? "+" : "") + v.toFixed(1)
                        color: Theme.text
                        font.pixelSize: 8
                        font.family: Theme.mono
                        rotation: -50
                        transformOrigin: Item.Bottom
                    }
                }
                Text {
                    id: labelText
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: chart.labels[entry.key] ?? entry.key
                    color: Theme.textDim
                    font.pixelSize: 8
                }
            }
        }
    }
}
