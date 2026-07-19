pragma Singleton
import QtQuick

// Restrained, high-contrast operator theme. Dark default.
QtObject {
    id: theme
    property bool dark: true

    readonly property color bg: dark ? "#14181d" : "#f2f4f6"
    readonly property color panel: dark ? "#1c2229" : "#ffffff"
    readonly property color panelAlt: dark ? "#222a33" : "#e8ecf0"
    readonly property color border: dark ? "#39434e" : "#c3cbd3"
    readonly property color text: dark ? "#d8dee5" : "#1a2027"
    readonly property color textDim: dark ? "#8b96a2" : "#5a646e"
    readonly property color accent: dark ? "#ffaa3c" : "#c85a00"
    readonly property color good: dark ? "#78dc8c" : "#1e8c3c"
    readonly property color warn: dark ? "#d4a72c" : "#a07800"
    readonly property color bad: dark ? "#e05a5a" : "#be2828"
    readonly property color ray: dark ? "#5aaaff" : "#1e5abe"
    readonly property color inputBg: dark ? "#12161b" : "#f7f9fa"

    readonly property int fontSize: 12
    readonly property int fontSizeSmall: 10
    readonly property string mono: "Consolas"
}
