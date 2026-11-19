// WallpaperBackground.qml — Natywna warstwa tapet statycznych i wideo Qt Multimedia pod panelami Glass (M10)
import QtQuick
import QtQuick.Effects
import QtMultimedia

Item {
    id: bgRoot
    objectName: "wallpaperBackground"

    // Bezpieczne bindowanie do globalnego obiektu `wallpaper` (WallpaperModel)
    readonly property bool hasModel: typeof wallpaper !== "undefined" && wallpaper !== null
    readonly property bool wpEnabled: hasModel ? wallpaper.enabled : true
    readonly property string wpMode: hasModel ? wallpaper.mode : "static"
    readonly property string wpSource: hasModel ? wallpaper.source : "qrc:/assets/wallpapers/pexels-artyusufpatel-8998289.jpg"
    readonly property real wpOpacity: hasModel ? wallpaper.wallpaperOpacity : 0.35
    readonly property real wpGlass: hasModel ? wallpaper.glassOpacity : 0.72
    readonly property real wpBlur: hasModel ? wallpaper.blurRadius : 14.0
    readonly property real wpDarkness: hasModel ? wallpaper.darknessOverlay : 0.50
    readonly property real wpSaturation: hasModel ? wallpaper.saturation : 0.85
    readonly property real wpVignette: hasModel ? wallpaper.vignette : 0.48
    readonly property bool wpMotionPaused: hasModel ? wallpaper.effectiveMotionPaused : false

    readonly property bool isVideo: wpEnabled && wpMode === "video" && wpSource.length > 0
    readonly property bool isImage: wpEnabled && (wpMode === "static" || wpMode === "rotate") && wpSource.length > 0

    // Kontrolowana baza tła — bez nieprzezroczystego #080c11 blokującego szkło
    Rectangle {
        id: baseBackdrop
        objectName: "wallpaperBaseBackdrop"
        anchors.fill: parent
        color: (bgRoot.wpEnabled && bgRoot.wpMode !== "none")
               ? Qt.rgba(0.025, 0.042, 0.060, 0.92)
               : Qt.rgba(0.025, 0.042, 0.060, bgRoot.wpGlass)
    }

    // Kontener źródła multimedialnego (obraz lub wideo)
    Item {
        id: mediaContainer
        anchors.fill: parent
        visible: bgRoot.wpEnabled && bgRoot.wpMode !== "none"
        opacity: bgRoot.wpOpacity
        layer.enabled: bgRoot.wpBlur > 0 || Math.abs(bgRoot.wpSaturation - 1.0) > 0.01

        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

        // 1. Tapeta statyczna / rotacja
        Image {
            id: staticImage
            objectName: "wallpaperStaticImage"
            anchors.fill: parent
            visible: bgRoot.isImage
            source: bgRoot.isImage ? bgRoot.wpSource : ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            smooth: true
            cache: true
            onStatusChanged: {
                if (status === Image.Error && bgRoot.hasModel) {
                    wallpaper.validateSource();
                }
            }
        }

        // 2. Tapeta filmowa (Natywne Qt Multimedia — bez WebView i bez HTTP)
        MediaPlayer {
            id: videoPlayer
            objectName: "wallpaperMediaPlayer"
            source: bgRoot.isVideo ? bgRoot.wpSource : ""
            loops: MediaPlayer.Infinite
            videoOutput: videoOutput

            onErrorOccurred: function(error, errorString) {
                console.warn("WallpaperBackground MediaPlayer error:", errorString);
                if (bgRoot.hasModel) {
                    wallpaper.validateSource();
                }
            }
        }

        VideoOutput {
            id: videoOutput
            objectName: "wallpaperVideoOutput"
            anchors.fill: parent
            visible: bgRoot.isVideo
            fillMode: VideoOutput.PreserveAspectCrop
        }

        // Zarządzanie odtwarzaniem wideo z poszanowaniem pauseMotion / reducedMotion
        function syncVideoPlayback() {
            if (bgRoot.isVideo && !bgRoot.wpMotionPaused) {
                if (videoPlayer.playbackState !== MediaPlayer.PlayingState) {
                    videoPlayer.play();
                }
            } else {
                if (videoPlayer.playbackState === MediaPlayer.PlayingState) {
                    videoPlayer.pause();
                }
            }
        }

        onVisibleChanged: syncVideoPlayback()
        Connections {
            target: bgRoot
            function onIsVideoChanged() { mediaContainer.syncVideoPlayback(); }
            function onWpMotionPausedChanged() { mediaContainer.syncVideoPlayback(); }
            function onWpSourceChanged() { mediaContainer.syncVideoPlayback(); }
        }
        Component.onCompleted: syncVideoPlayback()
    }

    // Efekt rozmycia i nasycenia (MultiEffect)
    MultiEffect {
        anchors.fill: parent
        source: mediaContainer
        visible: mediaContainer.visible && mediaContainer.layer.enabled
        blurEnabled: bgRoot.wpBlur > 0
        blur: Math.min(1.0, bgRoot.wpBlur / 64.0)
        blurMax: 32
        saturation: Math.max(-1.0, Math.min(1.0, bgRoot.wpSaturation - 1.0))
    }

    // Warstwa przyciemnienia (Overlay) — gwarantuje wysoki kontrast tekstu
    Rectangle {
        id: darknessRect
        objectName: "wallpaperDarknessOverlay"
        anchors.fill: parent
        color: "#05090e"
        opacity: bgRoot.wpEnabled && bgRoot.wpMode !== "none" ? bgRoot.wpDarkness : 0.0
        Behavior on opacity { NumberAnimation { duration: 160 } }
    }

    // Warstwa winiety studyjnej (Vignette)
    Canvas {
        id: vignetteCanvas
        anchors.fill: parent
        visible: bgRoot.wpVignette > 0.01 && bgRoot.wpEnabled && bgRoot.wpMode !== "none"
        opacity: bgRoot.wpVignette
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            var cx = width * 0.5;
            var cy = height * 0.5;
            var rOuter = Math.sqrt(cx * cx + cy * cy);
            var grad = ctx.createRadialGradient(cx, cy, rOuter * 0.28, cx, cy, rOuter);
            grad.addColorStop(0.0, "rgba(4, 7, 11, 0.0)");
            grad.addColorStop(1.0, "rgba(4, 7, 11, 0.88)");
            ctx.fillStyle = grad;
            ctx.fillRect(0, 0, width, height);
        }
    }
}
