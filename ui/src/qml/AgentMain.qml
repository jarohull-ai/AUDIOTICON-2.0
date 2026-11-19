// ASTRA UI template — Audioticon 2.0 App Shell.
//
// Same glass language as the dashboard shell, arranged for a conversation and
// audiobook studio: module navigation rail, directive studio, agent console,
// context inspector and the shared state strip along the bottom.
//
// Context properties: `feed` (AgentModel), `studio` (StudioBridge), `i18n` (Translator).
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Audioticon.Doctor 1.0

ApplicationWindow {
    id: win
    objectName: "mainWindow"
    width: 1180; height: 850
    minimumWidth: 780; minimumHeight: 560
    visible: true
    title: qsTr("Audioticon 2.0 · ASTRA Shell")
    color: "transparent"
    Material.theme: Material.Dark
    Material.accent: feed.accent

    DoctorModel {
        id: defaultDoctor
    }
    readonly property var activeDoctor: (typeof doctor !== "undefined" && doctor) ? doctor : defaultDoctor

    property color accent: feed.accent
    property string clockText: ""
    property int revision: 0
    readonly property real effectiveGlass: (typeof wallpaper !== "undefined" && wallpaper) ? wallpaper.glassOpacity : feed.glass

    // Moduły i stan nawigacji M6
    property string activeModule: "studio"   // "studio" | "agent" | "voicelab" | "projects" | "sfx" | "ledger" | "doctor"
    property bool inspectorOpen: true
    property bool inspectorPinned: true
    property string currentInspectorTab: "queue"
    property var selectedSegment: null

    Connections { target: feed; function onMessagesChanged() { win.revision++ } }
    Timer {
        interval: 1000; running: true; repeat: true; triggeredOnStart: true
        onTriggered: win.clockText = new Date().toLocaleTimeString(Qt.locale(), "HH:mm:ss")
    }

    // Skróty klawiszowe M6 / M13
    Shortcut { sequence: "Q"; onActivated: Qt.quit() }
    Shortcut { sequence: "N"; onActivated: feed.newSession() }
    Shortcut { sequence: "?"; onActivated: help.open() }
    Shortcut {
        sequence: "Ctrl+L"
        onActivated: { input.forceActiveFocus(); input.selectAll() }
    }
    Shortcut { sequence: "Ctrl+1"; onActivated: win.activeModule = "studio" }
    Shortcut { sequence: "Ctrl+2"; onActivated: win.activeModule = "agent" }
    Shortcut { sequence: "Ctrl+3"; onActivated: win.activeModule = "voicelab" }
    Shortcut { sequence: "Ctrl+4"; onActivated: win.activeModule = "projects" }
    Shortcut { sequence: "Ctrl+5"; onActivated: win.activeModule = "sfx" }
    Shortcut { sequence: "Ctrl+6"; onActivated: win.activeModule = "ledger" }
    Shortcut { sequence: "Ctrl+7"; onActivated: win.activeModule = "doctor" }
    Shortcut { sequence: "Ctrl+I"; onActivated: win.inspectorOpen = !win.inspectorOpen }
    Shortcut {
        sequence: "Ctrl+J"
        onActivated: {
            win.currentInspectorTab = "queue";
            win.inspectorOpen = true;
        }
    }
    Shortcut {
        sequence: "Space"
        enabled: win.activeModule === "studio" && (!win.activeFocusItem || win.activeFocusItem.objectName !== "composerTextArea")
        onActivated: {
            if (typeof studio !== "undefined" && studio) {
                if (studio.isBusy)
                    studio.cancelRender();
                else
                    studio.togglePlayback();
            }
        }
    }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (win.inspectorOpen && !win.inspectorPinned) {
                win.inspectorOpen = false;
            } else {
                feed.stop();
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 22
        color: "transparent"
        border.color: "#1a2632"
        clip: true

        // Warstwa tapet Glass (statyczne / filmowe Qt Multimedia / rotacja)
        WallpaperBackground {
            id: wallpaperLayer
            anchors.fill: parent
        }

        Rectangle {
            x: parent.width * .5; y: -420; width: 900; height: 900; radius: 450; opacity: .09
            gradient: Gradient {
                GradientStop { position: 0; color: win.accent }
                GradientStop { position: 1; color: "transparent" }
            }
        }
        Rectangle {
            x: -300; y: parent.height - 300; width: 750; height: 750; radius: 375; opacity: .07
            gradient: Gradient {
                GradientStop { position: 0; color: "#00d2ff" }
                GradientStop { position: 1; color: "transparent" }
            }
        }
        Canvas {
            anchors.fill: parent; opacity: .12
            onWidthChanged: requestPaint(); onHeightChanged: requestPaint()
            onPaint: {
                var c = getContext("2d"); c.reset(); c.fillStyle = "#52707d";
                for (var x = 18; x < width; x += 28)
                    for (var y = 18; y < height; y += 28) c.fillRect(x, y, 1, 1);
            }
        }

        ColumnLayout {
            anchors.fill: parent; anchors.margins: 18; spacing: 12

            // ---------- header ----------
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Rectangle {
                    width: 38; height: 38; radius: 11; color: "#0d1a22"; border.color: "#1f473d"
                    Canvas {
                        anchors.fill: parent; anchors.margins: 6
                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.reset();
                            ctx.strokeStyle = win.accent;
                            ctx.lineWidth = 2;
                            ctx.lineCap = "round";
                            var bars = [0.35, 0.7, 1.0, 0.55, 0.85, 0.4];
                            var w = width / bars.length;
                            for (var i = 0; i < bars.length; i++) {
                                var h = height * bars[i];
                                var x = i * w + w * 0.5;
                                ctx.beginPath();
                                ctx.moveTo(x, (height - h) * 0.5);
                                ctx.lineTo(x, (height + h) * 0.5);
                                ctx.stroke();
                            }
                        }
                    }
                }
                ColumnLayout {
                    spacing: 2
                    RowLayout {
                        spacing: 8
                        Text { text: qsTr("A U D I O T I C O N   2 . 0"); font.pixelSize: 16; font.weight: Font.DemiBold; font.letterSpacing: 1.5; color: "#eff9ff" }
                        Rectangle {
                            implicitWidth: proTxt.implicitWidth + 10; implicitHeight: 16; radius: 4
                            color: Qt.rgba(0.13, 0.90, 0.55, 0.14); border.color: "#1b6e4b"
                            Text {
                                id: proTxt; anchors.centerIn: parent; text: "PRO STUDIO"
                                color: win.accent; font.family: "monospace"; font.pixelSize: 8; font.weight: Font.Bold; font.letterSpacing: 0.8
                            }
                        }
                        Rectangle {
                            visible: win.activeDoctor && win.activeDoctor.inspectionMode
                            implicitWidth: inspModeBadge.implicitWidth + 10; implicitHeight: 16; radius: 4
                            color: Qt.rgba(1.0, 0.83, 0.23, 0.14); border.color: "#8f7014"
                            Text {
                                id: inspModeBadge; anchors.centerIn: parent; text: qsTr("DIRECTOR MODE")
                                color: "#ffd43b"; font.family: "monospace"; font.pixelSize: 8; font.weight: Font.Bold; font.letterSpacing: 0.8
                            }
                        }
                    }
                    Text {
                        text: qsTr("DIRECTOR WORKSTATION  //  NATIVE C++17 + RUST")
                        font.family: "monospace"; font.pixelSize: 9; font.letterSpacing: 1.4; color: "#5f7d8c"
                    }
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: win.clockText; color: "#8ca8b5"; font.family: "monospace"
                    font.pixelSize: 13; visible: win.width > 940
                }
                // Active voice / engine pill
                Rectangle {
                    implicitWidth: modelText.implicitWidth + 24; implicitHeight: 26; radius: 13
                    color: (win.activeDoctor && win.activeDoctor.inspectionMode)
                           ? "#241c09"
                           : (win.activeDoctor && !win.activeDoctor.hasVoiceModel ? "#260e14" : "#0e2220")
                    border.color: (win.activeDoctor && win.activeDoctor.inspectionMode)
                                  ? "#8f7014"
                                  : (win.activeDoctor && !win.activeDoctor.hasVoiceModel ? "#ff3b56" : "#1b6e4b")
                    Text {
                        id: modelText
                        anchors.centerIn: parent
                        text: {
                            if (win.activeDoctor && win.activeDoctor.inspectionMode) {
                                return qsTr("DIRECTOR · NO TTS");
                            }
                            if (typeof studio !== "undefined" && studio && studio.isBusy) {
                                return qsTr("SYNTHESIS %1%").arg(Math.round(studio.taskPercent));
                            }
                            if (win.activeDoctor && win.activeDoctor.hasVoiceModel) {
                                return win.activeDoctor.activeModelName + qsTr(" · 44.1 kHz");
                            }
                            return qsTr("No voice model");
                        }
                        color: (win.activeDoctor && win.activeDoctor.inspectionMode)
                               ? "#ffd43b"
                               : (win.activeDoctor && !win.activeDoctor.hasVoiceModel ? "#ff8093" : win.accent)
                        font.family: "monospace"; font.pixelSize: 10
                    }
                }
                Button {
                    visible: win.activeModule === "agent"
                    text: qsTr("New session")
                    onClicked: { feed.newSession(); input.forceActiveFocus() }
                }
                // Side drawer toggle
                ToolButton {
                    text: "☰"
                    font.pixelSize: 18
                    onClicked: win.inspectorOpen = !win.inspectorOpen
                    ToolTip.text: qsTr("Side panel (Ctrl+I)"); ToolTip.visible: hovered
                }
                ToolButton {
                    id: settingsGearBtn
                    objectName: "settingsGearButton"
                    text: "\u2699"; font.pixelSize: 20; onClicked: settings.open()
                    ToolTip.text: qsTr("Settings (Language, Wallpaper & Glass)"); ToolTip.visible: hovered
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#1a2632" }

            // ---------- body ----------
            RowLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 12

                // 1. LEFT NAVIGATION RAIL
                NavRail {
                    id: navRail
                    activeModule: win.activeModule
                    accent: win.accent
                    glass: win.effectiveGlass
                    windowWidth: win.width
                    Layout.fillHeight: true
                    onModuleSelected: function(modId) {
                        win.activeModule = modId;
                    }
                }

                // 2. CENTRALNY OBSZAR ROBOCZY
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    // WIDOK 1: STUDIO
                    StudioView {
                        id: studioView
                        anchors.fill: parent
                        visible: win.activeModule === "studio"
                        accent: win.accent
                        glass: win.effectiveGlass
                        onSegmentSelected: function(seg, idx) {
                            win.selectedSegment = seg;
                            win.currentInspectorTab = "inspector";
                            win.inspectorOpen = true;
                        }
                        onInspectRequested: {
                            win.currentInspectorTab = "inspector";
                            win.inspectorOpen = true;
                        }
                    }

                    // WIDOK 2: CZAT ASYSTENTA (Zawiera transcriptView dla 100% zgodności z testami!)
                    Item {
                        id: agentViewContainer
                        anchors.fill: parent
                        visible: win.activeModule === "agent"

                        RowLayout {
                            anchors.fill: parent
                            spacing: 12

                            // sessions
                            ColumnLayout {
                                Layout.preferredWidth: 190; Layout.maximumWidth: 210
                                Layout.fillHeight: true; spacing: 8
                                visible: win.width > 1060
                                Text {
                                    text: qsTr("SESSIONS"); color: "#52737e"
                                    font.family: "monospace"; font.pixelSize: 9; font.letterSpacing: 1.4
                                }
                                Repeater {
                                    model: feed.sessions
                                    Rectangle {
                                        required property var modelData
                                        required property int index
                                        Layout.fillWidth: true
                                        implicitHeight: 56
                                        radius: 10
                                        color: Qt.rgba(.035, .065, .085, win.effectiveGlass)
                                        border.width: 1
                                        border.color: index === feed.current
                                                      ? Qt.rgba(win.accent.r, win.accent.g, win.accent.b, .45)
                                                      : "#1e3a43"
                                        ColumnLayout {
                                            anchors.fill: parent
                                            anchors.margins: 8
                                            spacing: 2
                                            RowLayout {
                                                spacing: 6
                                                Rectangle {
                                                    width: 5; height: 5; radius: 3
                                                    color: modelData.dot === "active" ? win.accent : "#4d6b76"
                                                }
                                                Text {
                                                    text: modelData.title
                                                    color: index === feed.current ? "#eaf6fb" : "#a9c2cc"
                                                    font.pixelSize: 11; elide: Text.ElideRight
                                                    Layout.fillWidth: true
                                                }
                                            }
                                            Text {
                                                text: modelData.subtitle
                                                color: "#6f8f99"; font.pixelSize: 9
                                                elide: Text.ElideRight; Layout.fillWidth: true
                                            }
                                        }
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: feed.pickSession(index) }
                                    }
                                }
                                Item { Layout.fillHeight: true }
                            }

                            // transcript + composer
                            ColumnLayout {
                                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 10

                                Rectangle {
                                    Layout.fillWidth: true; Layout.fillHeight: true
                                    radius: 16
                                    color: Qt.rgba(.035, .065, .085, win.effectiveGlass * .92)
                                    border.color: Qt.rgba(win.accent.r, win.accent.g, win.accent.b, .16)
                                    clip: true

                                    ListView {
                                        id: transcript
                                        objectName: "transcriptView"
                                        anchors.fill: parent
                                        anchors.margins: 14
                                        spacing: 8
                                        bottomMargin: 10
                                        model: feed.messages
                                        boundsBehavior: Flickable.StopAtBounds
                                        delegate: ChatBubble {
                                            required property int index
                                            message: { win.revision; return feed.messageAt(index) }
                                            accent: win.accent
                                            glass: win.effectiveGlass
                                            onReadInStudioRequested: function(text) {
                                                win.activeModule = "studio";
                                            }
                                        }
                                        Timer {
                                            interval: 120; running: feed.streaming; repeat: true
                                            onTriggered: transcript.positionViewAtEnd()
                                        }
                                        onContentHeightChanged: positionViewAtEnd()
                                        onCountChanged: Qt.callLater(positionViewAtEnd)
                                        Connections {
                                            target: feed
                                            function onMessagesChanged() { Qt.callLater(transcript.positionViewAtEnd) }
                                            function onStreamingChanged() { pinTimer.restart() }
                                        }
                                        Timer {
                                            id: pinTimer; interval: 90
                                            onTriggered: transcript.positionViewAtEnd()
                                        }
                                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                                    }
                                }

                                // composer
                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: 50
                                    radius: 14
                                    color: Qt.rgba(.035, .065, .085, win.effectiveGlass)
                                    border.color: input.activeFocus
                                                  ? Qt.rgba(win.accent.r, win.accent.g, win.accent.b, .55)
                                                  : "#25414a"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 14
                                        anchors.rightMargin: 10
                                        spacing: 10
                                        Text {
                                            text: "\u203A"; color: win.accent
                                            font.family: "monospace"; font.pixelSize: 18
                                        }
                                        TextField {
                                            id: input
                                            Layout.fillWidth: true
                                            placeholderText: feed.streaming ? qsTr("streaming… press Esc to stop")
                                                                            : qsTr("Ask the agent, or type status / test / patch")
                                            font.pixelSize: 13
                                            background: Item {}
                                            color: "#e7f3f8"
                                            onAccepted: {
                                                if (feed.streaming) return;
                                                if (text.trim().length === 0) return;
                                                feed.send(text);
                                                text = "";
                                            }
                                            Component.onCompleted: forceActiveFocus()
                                        }
                                        Button {
                                            text: feed.streaming ? qsTr("Stop") : qsTr("Send")
                                            highlighted: !feed.streaming
                                            onClicked: {
                                                if (feed.streaming) { feed.stop(); return; }
                                                if (input.text.trim().length === 0) return;
                                                feed.send(input.text);
                                                input.text = "";
                                                input.forceActiveFocus();
                                            }
                                        }
                                    }
                                }

                                Text {
                                    text: feed.hint
                                    color: "#52737e"
                                    font.family: "monospace"; font.pixelSize: 9
                                }
                            }
                        }
                    }

                    // WIDOK 3: DOCTOR DIAGNOSTYKA
                    DoctorView {
                        id: doctorView
                        anchors.fill: parent
                        visible: win.activeModule === "doctor"
                        accent: win.accent
                        glass: win.effectiveGlass
                        onReturnToStudioRequested: win.activeModule = "studio"
                    }

                    // PLANNED MODULES PLACEHOLDER (voicelab, projects, sfx, ledger — P1)
                    Rectangle {
                        anchors.fill: parent
                        visible: win.activeModule !== "studio" && win.activeModule !== "agent" && win.activeModule !== "doctor"
                        radius: 16
                        color: Qt.rgba(.02, .04, .06, win.effectiveGlass)
                        border.color: "#1e3a43"
                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 12
                            Text {
                                text: win.activeModule === "voicelab" ? qsTr("👥 VoiceLab")
                                    : win.activeModule === "projects" ? qsTr("📁 Projects & Materials")
                                    : win.activeModule === "sfx" ? qsTr("🔊 Foley & SFX") : qsTr("📊 ACX Ledger")
                                color: "#eff9ff"; font.pixelSize: 20; font.weight: Font.DemiBold
                            }
                            Text {
                                text: qsTr("Planned module — not yet implemented in this build.")
                                color: "#6b8b98"; font.pixelSize: 12
                            }
                            Button {
                                text: qsTr("Back to Studio")
                                onClicked: win.activeModule = "studio"
                            }
                        }
                    }
                }

                // 3. PRAWY PANEL KONTEKSTOWY (DOKOWANY)
                ContextInspector {
                    id: contextInspector
                    objectName: "contextInspector"
                    visible: win.inspectorOpen && win.inspectorPinned && win.width > 920
                    currentTab: win.currentInspectorTab
                    pinned: win.inspectorPinned
                    segmentData: win.selectedSegment
                    accent: win.accent
                    glass: win.effectiveGlass
                    Layout.fillHeight: true
                    onCloseRequested: win.inspectorOpen = false
                    onPinToggled: win.inspectorPinned = !win.inspectorPinned
                }
            }

            // ---------- state strip (wyłącznie dla Czat AI) ----------
            StateStrip {
                id: stripPanel
                readonly property bool hasData: feed.strip && Object.keys(feed.strip).length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: (win.activeModule === "agent" && hasData) ? 136 : 0
                visible: win.activeModule === "agent" && hasData
                glass: win.effectiveGlass; accent: win.accent
                s: { win.revision; return feed.strip }
            }

            // ---------- mastering strip (Studio i moduły audio) ----------
            MasteringStrip {
                id: masteringStrip
                Layout.fillWidth: true
                Layout.preferredHeight: win.activeModule !== "agent" ? 38 : 0
                visible: win.activeModule !== "agent"
                glass: win.effectiveGlass
                accent: "#22e58b"
            }

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("AUDIOTICON 2.0  //  C++17 E1 + RUST TOKIO  //  LOCAL AUDIO WORKSTATION")
                    color: "#52707d"; font.family: "monospace"; font.pixelSize: 10
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: qsTr("Ctrl+1..7 modules   ·   Ctrl+I inspector   ·   Space play/pause   ·   ESC stop")
                    color: "#52707d"; font.pixelSize: 10
                }
            }
        }
    }

    // FLOATING DRAWER (when unpinned or compact window)
    Rectangle {
        id: drawerScrim
        anchors.fill: parent
        color: Qt.rgba(0.01, 0.02, 0.03, 0.72)
        visible: win.inspectorOpen && (!win.inspectorPinned || win.width <= 920)
        z: 90
        MouseArea {
            anchors.fill: parent
            onClicked: win.inspectorOpen = false
        }
    }

    ContextInspector {
        id: floatingDrawer
        objectName: "floatingContextInspector"
        visible: win.inspectorOpen && (!win.inspectorPinned || win.width <= 920)
        z: 95
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 14
        currentTab: win.currentInspectorTab
        pinned: win.inspectorPinned
        segmentData: win.selectedSegment
        accent: win.accent
        glass: win.effectiveGlass
        onCloseRequested: win.inspectorOpen = false
        onPinToggled: win.inspectorPinned = !win.inspectorPinned
    }

    // Unified Settings Drawer under ⚙ (Language + Wallpaper & Glass + Local Cache)
    Drawer {
        id: settings
        objectName: "unifiedSettingsDrawer"
        edge: Qt.RightEdge; width: Math.min(360, win.width - 24); height: win.height
        background: Rectangle {
            color: Qt.rgba(0.04, 0.07, 0.10, Math.max(0.82, win.effectiveGlass))
            border.color: "#294452"
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("Settings")
                    color: "#e6f7ff"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                }
                ToolButton {
                    text: "✕"
                    font.pixelSize: 14
                    onClicked: settings.close()
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#1c3340" }

            ScrollView {
                id: settingsScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: settingsScroll.availableWidth
                    spacing: 14

                    // 1. Language section
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: qsTr("LANGUAGE")
                            color: "#52707d"
                            font.family: "monospace"
                            font.pixelSize: 9
                            font.weight: Font.Bold
                            font.letterSpacing: 1.4
                        }
                        ComboBox {
                            id: settingsLangCombo
                            objectName: "settingsLanguageCombo"
                            Layout.fillWidth: true
                            readonly property var codes: ["auto", "en", "pl"]
                            model: [qsTr("Automatic (Omarchy)"), "English", "Polski"]
                            currentIndex: Math.max(0, codes.indexOf(i18n.choice))
                            onActivated: i18n.choice = codes[currentIndex]
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: "#162430" }

                    // 2. Unified Wallpaper & Glass section (with thumbnails & live controls)
                    WallpaperSettings {
                        Layout.fillWidth: true
                        accent: win.accent
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#1c3340" }

            Button {
                Layout.fillWidth: true
                text: qsTr("Close")
                onClicked: settings.close()
            }
        }
    }

    Popup {
        id: help
        anchors.centerIn: parent; width: 400; modal: true; padding: 24
        ColumnLayout {
            spacing: 12
            Text { text: qsTr("Audioticon 2.0 controls"); color: "white"; font.pixelSize: 20 }
            Text {
                text: qsTr("Ctrl+1  Studio\nCtrl+2  AI Chat\nCtrl+7  Doctor diagnostics\nCtrl+I  Toggle side panel\nCtrl+J  Jump to render queue\nCtrl+L  Focus text input\nSpace   Start/Pause playback\nESC     Stop streaming / close panel")
                color: "#b9d0d9"; font.pixelSize: 13
            }
            Button { text: qsTr("Got it"); onClicked: help.close() }
        }
    }

    // Modalny przewodnik pierwszego uruchomienia
    FirstRunWizard {
        id: firstRunWizard
        visible: (typeof activeDoctor !== "undefined" && activeDoctor)
                 && !activeDoctor.firstRunCompleted
                 && !activeDoctor.inspectionMode
                 && !activeDoctor.hasVoiceModel
        accent: win.accent
        glass: win.effectiveGlass
        onOpenDoctorRequested: {
            win.activeModule = "doctor";
            firstRunWizard.visible = false;
        }
        onWizardCompleted: {
            firstRunWizard.visible = false;
        }
    }
}
