// WallpaperSettings.qml — Unified Glass & Wallpaper configuration panel (M10 / M16 P0)
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ColumnLayout {
    id: wpSettings
    objectName: "wallpaperSettingsPanel"
    property color accent: "#22e58b"

    readonly property bool hasWp: typeof wallpaper !== "undefined" && wallpaper !== null

    spacing: 10

    // Header and master switch
    RowLayout {
        Layout.fillWidth: true
        Text {
            text: qsTr("WALLPAPER & GLASS")
            color: "#52707d"
            font.family: "monospace"
            font.pixelSize: 9
            font.weight: Font.Bold
            font.letterSpacing: 1.4
            Layout.fillWidth: true
        }
        Switch {
            objectName: "wallpaperEnableSwitch"
            checked: wpSettings.hasWp ? wallpaper.enabled : true
            onToggled: if (wpSettings.hasWp) wallpaper.enabled = checked
        }
    }

    // Fallback notice when a previously selected file was removed
    Rectangle {
        visible: wpSettings.hasWp && wallpaper.fallbackNotice.length > 0
        Layout.fillWidth: true
        implicitHeight: noticeTxt.implicitHeight + 12
        radius: 7
        color: Qt.rgba(1.0, 0.72, 0.2, 0.14)
        border.color: "#8f7014"

        Text {
            id: noticeTxt
            anchors.fill: parent
            anchors.margins: 6
            text: wpSettings.hasWp ? wallpaper.fallbackNotice : ""
            color: "#ffd43b"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
        }
    }

    // Mode selector (None / Image / Video / Rotate)
    RowLayout {
        Layout.fillWidth: true
        spacing: 4
        enabled: wpSettings.hasWp && wallpaper.enabled

        Repeater {
            model: [
                { id: "none",   label: qsTr("None") },
                { id: "static", label: qsTr("Image") },
                { id: "video",  label: qsTr("Video") },
                { id: "rotate", label: qsTr("Rotate") }
            ]

            Rectangle {
                required property var modelData
                readonly property bool isActive: wpSettings.hasWp && wallpaper.mode === modelData.id
                Layout.fillWidth: true
                implicitHeight: 26
                radius: 6
                color: isActive ? Qt.rgba(wpSettings.accent.r, wpSettings.accent.g, wpSettings.accent.b, 0.16) : "#0c141c"
                border.color: isActive ? wpSettings.accent : "#1d2d3b"

                Text {
                    anchors.centerIn: parent
                    text: modelData.label
                    color: isActive ? "#eff9ff" : "#7895a3"
                    font.pixelSize: 10
                    font.weight: isActive ? Font.DemiBold : Font.Normal
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: if (wpSettings.hasWp) wallpaper.mode = modelData.id
                }
            }
        }
    }

    // Glass & readability sliders (unified glassOpacity controls all panels)
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: slidersCol.implicitHeight + 16
        radius: 10
        color: Qt.rgba(0.035, 0.06, 0.09, 0.75)
        border.color: "#162430"

        ColumnLayout {
            id: slidersCol
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6

            Repeater {
                model: [
                    { key: "glassOpacity",     label: qsTr("Tile glass opacity (all panels)"), min: 0.30, max: 0.98, suffix: "%", mult: 100, alwaysEnabled: true },
                    { key: "wallpaperOpacity", label: qsTr("Wallpaper visibility"),            min: 0.0,  max: 1.0,  suffix: "%", mult: 100, alwaysEnabled: false },
                    { key: "darknessOverlay",  label: qsTr("Background dimming"),              min: 0.0,  max: 0.95, suffix: "%", mult: 100, alwaysEnabled: false },
                    { key: "blurRadius",       label: qsTr("Background blur"),                 min: 0.0,  max: 64.0, suffix: "px", mult: 1,  alwaysEnabled: false },
                    { key: "saturation",       label: qsTr("Color saturation"),                min: 0.0,  max: 1.5,  suffix: "%", mult: 100, alwaysEnabled: false },
                    { key: "vignette",         label: qsTr("Studio vignette"),                 min: 0.0,  max: 1.0,  suffix: "%", mult: 100, alwaysEnabled: false }
                ]

                ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 1
                    enabled: modelData.alwaysEnabled || (wpSettings.hasWp && wallpaper.enabled && wallpaper.mode !== "none")
                    opacity: enabled ? 1.0 : 0.45

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: modelData.label
                            color: "#8ca8b5"
                            font.pixelSize: 10
                            Layout.fillWidth: true
                        }
                        Text {
                            readonly property real rawVal: wpSettings.hasWp ? wallpaper[modelData.key] : 0
                            text: Math.round(rawVal * modelData.mult) + modelData.suffix
                            color: wpSettings.accent
                            font.family: "monospace"
                            font.pixelSize: 9
                        }
                    }

                    Slider {
                        Layout.fillWidth: true
                        implicitHeight: 20
                        from: modelData.min
                        to: modelData.max
                        value: wpSettings.hasWp ? wallpaper[modelData.key] : modelData.min
                        onMoved: {
                            if (wpSettings.hasWp) {
                                wallpaper[modelData.key] = value;
                            }
                        }
                    }
                }
            }
        }
    }

    // Motion & accessibility options
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: motionCol.implicitHeight + 12
        radius: 10
        color: Qt.rgba(0.035, 0.06, 0.09, 0.75)
        border.color: "#162430"

        ColumnLayout {
            id: motionCol
            anchors.fill: parent
            anchors.margins: 8
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                Text { text: qsTr("Pause motion"); color: "#9bb8c2"; font.pixelSize: 10; Layout.fillWidth: true }
                Switch {
                    checked: wpSettings.hasWp ? wallpaper.pauseMotion : false
                    onToggled: if (wpSettings.hasWp) wallpaper.pauseMotion = checked
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Text { text: qsTr("Reduced motion (Accessibility)"); color: "#9bb8c2"; font.pixelSize: 10; Layout.fillWidth: true }
                Switch {
                    objectName: "reducedMotionSwitch"
                    checked: wpSettings.hasWp ? wallpaper.reducedMotion : false
                    onToggled: if (wpSettings.hasWp) wallpaper.reducedMotion = checked
                }
            }
        }
    }

    // Local wallpaper thumbnails catalog
    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        Text {
            text: qsTr("LOCAL WALLPAPERS")
            color: "#52707d"
            font.family: "monospace"
            font.pixelSize: 9
            font.letterSpacing: 1.2
            Layout.fillWidth: true
        }
        ToolButton {
            text: "↻"
            font.pixelSize: 12
            implicitWidth: 26
            implicitHeight: 24
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Refresh local wallpapers")
            onClicked: if (wpSettings.hasWp) { wallpaper.scanAssets(); wallpaper.validateSource(); }
        }
        Button {
            text: qsTr("Choose file...")
            font.pixelSize: 10
            implicitHeight: 26
            onClicked: filePicker.open()
        }
    }

    GridView {
        id: wpThumbGrid
        objectName: "wallpaperThumbnailGrid"
        Layout.fillWidth: true
        Layout.preferredHeight: 196
        clip: true
        cellWidth: Math.max(120, Math.floor(width / 2))
        cellHeight: 84
        model: wpSettings.hasWp ? wallpaper.availableWallpapers : []
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        delegate: Item {
            required property var modelData
            width: wpThumbGrid.cellWidth
            height: wpThumbGrid.cellHeight

            readonly property bool isSelected: wpSettings.hasWp && wallpaper.source === modelData.path

            Rectangle {
                anchors.fill: parent
                anchors.margins: 3
                radius: 8
                color: isSelected
                       ? Qt.rgba(wpSettings.accent.r, wpSettings.accent.g, wpSettings.accent.b, 0.16)
                       : (wpItemMouse.containsMouse ? "#121d26" : "#0b1219")
                border.width: isSelected ? 2 : 1
                border.color: isSelected ? wpSettings.accent : "#182734"
                clip: true

                // Thumbnail preview for static images
                Image {
                    anchors.fill: parent
                    anchors.margins: 1
                    visible: modelData.kind === "static"
                    source: modelData.kind === "static" ? modelData.path : ""
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    sourceSize.width: 160
                    sourceSize.height: 90
                    opacity: 0.72
                }

                // Stylized preview for video assets
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    visible: modelData.kind === "video"
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#151a2e" }
                        GradientStop { position: 1.0; color: "#0a0e18" }
                    }
                    Text {
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: -6
                        text: "🎬"
                        font.pixelSize: 20
                    }
                }

                // Bottom scrim with filename & kind badge
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 24
                    color: Qt.rgba(0.02, 0.04, 0.06, 0.84)

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        spacing: 4

                        Text {
                            text: modelData.name || ""
                            color: isSelected ? "#ffffff" : "#c6dce6"
                            font.pixelSize: 9
                            font.weight: isSelected ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Rectangle {
                            implicitWidth: badgeTxt.implicitWidth + 6
                            implicitHeight: 14
                            radius: 3
                            color: modelData.kind === "video" ? "#261c42" : "#102821"
                            Text {
                                id: badgeTxt
                                anchors.centerIn: parent
                                text: modelData.kind === "video" ? "MP4" : "IMG"
                                color: modelData.kind === "video" ? "#c29eff" : wpSettings.accent
                                font.family: "monospace"
                                font.pixelSize: 8
                            }
                        }
                    }
                }

                MouseArea {
                    id: wpItemMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (wpSettings.hasWp) {
                            wallpaper.selectWallpaper(modelData.path, modelData.kind);
                        }
                    }
                }
            }
        }
    }

    Button {
        Layout.fillWidth: true
        text: qsTr("Reset background defaults")
        font.pixelSize: 10
        implicitHeight: 28
        onClicked: if (wpSettings.hasWp) wallpaper.resetDefaults()
    }

    FileDialog {
        id: filePicker
        title: qsTr("Select wallpaper (image or MP4 video)")
        nameFilters: [qsTr("Media (*.jpg *.jpeg *.png *.webp *.mp4 *.webm *.mkv)")]
        onAccepted: {
            if (wpSettings.hasWp) {
                wallpaper.selectWallpaper(selectedFile.toString());
            }
        }
    }
}
