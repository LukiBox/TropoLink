import QtQuick
import QtQuick.Layouts
import TropoLink

// Bottom dock: the signature profile & geometry view, synced with the map.
Rectangle {
    id: panel
    color: Theme.panel
    border.color: Theme.border
    clip: true
    property alias hoverDistance: hoverArea.hoverDistance

    ProfileItem {
        id: profile
        anchors.fill: parent
        anchors.margins: 2
        terrain: controller.profileTerrain
        rayA: controller.profileRayA
        rayB: controller.profileRayB
        lens: controller.profileLens
        directRay: controller.profileDirectRay
        fresnelLower: controller.profileFresnelLower
        fresnelUpper: controller.profileFresnelUpper
        voidSpans: controller.profileVoidSpans
        meta: controller.profileMeta
        darkTheme: Theme.dark

        // Axis labels as light QML overlays.
        Repeater {
            model: profile.elevationTicks()
            Text {
                x: 4
                y: modelData.pixel - height / 2
                text: modelData.value.toFixed(0) + " m"
                color: Theme.textDim
                font.pixelSize: 9
                font.family: Theme.mono
            }
        }
        Repeater {
            model: profile.distanceTicks()
            Text {
                x: modelData.pixel - width / 2
                y: profile.height - 18
                text: modelData.value.toFixed(0) + " km"
                color: Theme.textDim
                font.pixelSize: 9
                font.family: Theme.mono
            }
        }
        // Scatter-angle annotation at the lens.
        Text {
            x: profile.xToPixel(controller.profileMeta.lensX ?? 0) + 8
            y: profile.yToPixel(controller.profileMeta.lensBaseY ?? 0) - 18
            text: "θ = " + (controller.profileMeta.thetaMrad ?? 0).toFixed(2) + " mrad"
            color: Theme.good
            font.pixelSize: 10
            font.family: Theme.mono
            visible: (controller.profileMeta.distanceM ?? 0) > 0
        }

        MouseArea {
            id: hoverArea
            anchors.fill: parent
            hoverEnabled: true
            property double hoverDistance: -1
            onPositionChanged: (e) => hoverDistance = profile.pixelToDistance(e.x)
            onExited: hoverDistance = -1

            Rectangle {
                visible: hoverArea.hoverDistance >= 0
                x: profile.xToPixel(hoverArea.hoverDistance)
                y: 0
                width: 1
                height: parent.height
                color: Theme.accent
                opacity: 0.7
            }
            Text {
                visible: hoverArea.hoverDistance >= 0
                x: Math.min(profile.xToPixel(hoverArea.hoverDistance) + 6, profile.width - width - 4)
                y: 6
                text: (hoverArea.hoverDistance / 1000).toFixed(2) + " km"
                color: Theme.accent
                font.pixelSize: 10
                font.family: Theme.mono
            }
        }
    }
    Text {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 6
        text: controller.profileHasVoids ? qsTr("⚠ DEM voids interpolated (orange)") : ""
        color: Theme.warn
        font.pixelSize: Theme.fontSizeSmall
    }
}
