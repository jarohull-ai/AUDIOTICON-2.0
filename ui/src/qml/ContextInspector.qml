// ContextInspector.qml — Right context drawer/inspector for Audioticon 2.0 (ASTRA Glass, M16 P0)
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: inspectorRoot
    objectName: "contextInspector"
    property color accent: "#22e58b"
    property real glass: 0.72
    property string currentTab: "inspector"
    property bool pinned: true
    property bool isOpen: true
    property var segmentData: null

    signal closeRequested()
    signal pinToggled()

    width: 320
    color: Qt.rgba(.035, .055, .078, glass)
    border.color: "#1a2632"
    border.width: 1
    radius: 14
    clip: true

    readonly property var tabs: [
        { id: "inspector",   icon: "🔍", label: qsTr("Inspector") },
        { id: "queue",       icon: "⏳", label: qsTr("Queue") },
        { id: "models",      icon: "🎙️", label: qsTr("Voices") },
        { id: "projects",    icon: "📁", label: qsTr("Projects") },
        { id: "export",      icon: "💾", label: qsTr("Export") },
        { id: "settings",    icon: "⚙️", label: qsTr("Settings") },
        { id: "diagnostics", icon: "🩺", label: qsTr("Doctor") }
    ]

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        // ====================================================================
        // PANEL HEADER
        // ====================================================================
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: qsTr("SIDE PANEL")
                color: "#52737e"
                font.family: "monospace"
                font.pixelSize: 9
                font.letterSpacing: 1.4
            }

            Item { Layout.fillWidth: true }

            ToolButton {
                text: inspectorRoot.pinned ? "📌" : "📍"
                font.pixelSize: 12
                ToolTip.visible: hovered
                ToolTip.text: inspectorRoot.pinned ? qsTr("Pinned in dock") : qsTr("Floating drawer")
                onClicked: {
                    inspectorRoot.pinned = !inspectorRoot.pinned;
                    inspectorRoot.pinToggled();
                }
            }

            ToolButton {
                text: "✕"
                font.pixelSize: 12
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Close panel (Esc)")
                onClicked: {
                    inspectorRoot.closeRequested();
                }
            }
        }

        // Wrapping tab strip — ALL 7 tabs always visible and clickable
        Flow {
            Layout.fillWidth: true
            spacing: 4

            Repeater {
                model: inspectorRoot.tabs
                Rectangle {
                    required property var modelData
                    width: tabBtnText.implicitWidth + 14
                    height: 26
                    radius: 7
                    color: inspectorRoot.currentTab === modelData.id
                           ? Qt.rgba(inspectorRoot.accent.r, inspectorRoot.accent.g, inspectorRoot.accent.b, .20)
                           : tabMouse.containsMouse ? Qt.rgba(1, 1, 1, .06) : Qt.rgba(0.02, 0.04, 0.06, 0.45)
                    border.width: 1
                    border.color: inspectorRoot.currentTab === modelData.id
                                  ? Qt.rgba(inspectorRoot.accent.r, inspectorRoot.accent.g, inspectorRoot.accent.b, .55)
                                  : "#182a35"

                    Row {
                        id: tabBtnText
                        anchors.centerIn: parent
                        spacing: 4
                        Text { text: modelData.icon; font.pixelSize: 10 }
                        Text {
                            text: modelData.label
                            color: inspectorRoot.currentTab === modelData.id ? "#eff9ff" : "#8aa2ad"
                            font.pixelSize: 10
                            font.weight: inspectorRoot.currentTab === modelData.id ? Font.DemiBold : Font.Normal
                        }
                    }

                    MouseArea {
                        id: tabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: inspectorRoot.currentTab = modelData.id
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#16313a" }

        // ====================================================================
        // TAB CONTENT
        // ====================================================================
        ScrollView {
            id: tabScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: tabScroll.availableWidth
                spacing: 12

                // ------------------------------------------------------------
                // 1. SEGMENT INSPECTOR
                // ------------------------------------------------------------
                ColumnLayout {
                    visible: inspectorRoot.currentTab === "inspector"
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: qsTr("SEGMENT DETAILS")
                        color: "#7e9da9"
                        font.family: "monospace"
                        font.pixelSize: 10
                        font.letterSpacing: 1.1
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: segDetailsCol.implicitHeight + 20
                        radius: 10
                        color: Qt.rgba(.01, .025, .035, Math.max(0.45, inspectorRoot.glass * 0.85))
                        border.color: "#1e3a43"

                        ColumnLayout {
                            id: segDetailsCol
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6

                            Text {
                                text: inspectorRoot.segmentData ? qsTr("Segment #%1").arg(inspectorRoot.segmentData.index + 1) : qsTr("No segment selected")
                                color: "#eff9ff"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }

                            Text {
                                text: inspectorRoot.segmentData ? inspectorRoot.segmentData.text : qsTr("Click a segment in the timeline to inspect its directives.")
                                color: "#9bb8c2"
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            Rectangle { Layout.fillWidth: true; height: 1; color: "#16313a" }

                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: qsTr("Tempo:"); color: "#52737e"; font.pixelSize: 10 }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: inspectorRoot.segmentData && inspectorRoot.segmentData.speed ? (inspectorRoot.segmentData.speed + "x") : "1.0x"
                                    color: inspectorRoot.accent; font.family: "monospace"; font.pixelSize: 10
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: qsTr("DSP Filter:"); color: "#52737e"; font.pixelSize: 10 }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: inspectorRoot.segmentData && inspectorRoot.segmentData.dsp ? inspectorRoot.segmentData.dsp : qsTr("None (dry voice)")
                                    color: "#eaf6fb"; font.family: "monospace"; font.pixelSize: 10
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: qsTr("Pause:"); color: "#52737e"; font.pixelSize: 10 }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: inspectorRoot.segmentData && inspectorRoot.segmentData.pauseMs ? (inspectorRoot.segmentData.pauseMs + " ms") : "0 ms"
                                    color: "#eaf6fb"; font.family: "monospace"; font.pixelSize: 10
                                }
                            }
                        }
                    }

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("▶ Preview single segment")
                        enabled: inspectorRoot.segmentData !== null
                        onClicked: {
                            if (typeof studio !== "undefined" && studio && inspectorRoot.segmentData) {
                                studio.startRender(inspectorRoot.segmentData.text);
                            }
                        }
                    }
                }

                // ------------------------------------------------------------
                // 2. RENDER QUEUE
                // ------------------------------------------------------------
                ColumnLayout {
                    visible: inspectorRoot.currentTab === "queue"
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: qsTr("RENDER JOB QUEUE")
                        color: "#7e9da9"
                        font.family: "monospace"
                        font.pixelSize: 10
                        font.letterSpacing: 1.1
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: queueCol.implicitHeight + 20
                        radius: 10
                        color: Qt.rgba(.01, .025, .035, Math.max(0.45, inspectorRoot.glass * 0.85))
                        border.color: "#1e3a43"

                        ColumnLayout {
                            id: queueCol
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: qsTr("Status:"); color: "#52737e"; font.pixelSize: 11 }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: (typeof studio !== "undefined" && studio) ? studio.taskStatus : "Idle"
                                    color: (typeof studio !== "undefined" && studio && studio.isBusy) ? "#ffd43b" : inspectorRoot.accent
                                    font.family: "monospace"
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                }
                            }

                            ProgressBar {
                                Layout.fillWidth: true
                                value: (typeof studio !== "undefined" && studio) ? (studio.taskPercent / 100.0) : 0
                            }

                            Text {
                                text: (typeof studio !== "undefined" && studio && studio.statusMessage.length > 0)
                                      ? studio.statusMessage
                                      : qsTr("No active jobs running.")
                                color: "#8ca7b2"
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            Text {
                                visible: (typeof studio !== "undefined" && studio && studio.outputPath.length > 0)
                                text: (typeof studio !== "undefined" && studio) ? qsTr("Output file: %1").arg(studio.outputPath) : ""
                                color: inspectorRoot.accent
                                font.family: "monospace"
                                font.pixelSize: 9
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }

                            Button {
                                Layout.fillWidth: true
                                text: qsTr("🛑 Cancel current job")
                                visible: typeof studio !== "undefined" && studio && studio.isBusy
                                onClicked: {
                                    if (typeof studio !== "undefined" && studio)
                                        studio.cancelRender();
                                }
                            }
                        }
                    }
                }

                // ------------------------------------------------------------
                // 3. MODELS & VOICES (REAL DOCTOR STATE + PLANNED P1 CATALOG)
                // ------------------------------------------------------------
                ColumnLayout {
                    visible: inspectorRoot.currentTab === "models"
                    Layout.fillWidth: true
                    spacing: 8

                    Text { text: qsTr("ACTIVE VOICE MODEL (PIPER ONNX)"); color: "#7e9da9"; font.family: "monospace"; font.pixelSize: 10 }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: voiceCardCol.implicitHeight + 16
                        radius: 8
                        color: Qt.rgba(.01, .025, .035, Math.max(0.45, inspectorRoot.glass * 0.85))
                        border.color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.hasVoiceModel) ? inspectorRoot.accent : "#ff4d6d"

                        ColumnLayout {
                            id: voiceCardCol
                            anchors.fill: parent; anchors.margins: 8; spacing: 4
                            Text {
                                text: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.hasVoiceModel)
                                      ? activeDoctor.activeModelName
                                      : qsTr("No voice model detected")
                                color: "#eff9ff"; font.pixelSize: 12; font.weight: Font.DemiBold
                                elide: Text.ElideRight; Layout.fillWidth: true
                            }
                            Text {
                                text: (typeof activeDoctor !== "undefined" && activeDoctor) ? activeDoctor.modelDetails : ""
                                color: "#8ba8b5"; font.pixelSize: 10; wrapMode: Text.WordWrap; Layout.fillWidth: true
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Full VoiceLab catalog, voice import, and custom voices are planned for P1.")
                        color: "#6e8e9b"
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("🩺 Manage model in Doctor")
                        onClicked: if (typeof win !== "undefined") win.activeModule = "doctor"
                    }
                }

                // ------------------------------------------------------------
                // 4. PROJECTS (HONEST PLANNED MODULE)
                // ------------------------------------------------------------
                ColumnLayout {
                    visible: inspectorRoot.currentTab === "projects"
                    Layout.fillWidth: true
                    spacing: 8
                    Text { text: qsTr("PROJECTS & MATERIALS"); color: "#7e9da9"; font.family: "monospace"; font.pixelSize: 10 }
                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: projCol.implicitHeight + 16; radius: 8
                        color: Qt.rgba(.01, .025, .035, Math.max(0.45, inspectorRoot.glass * 0.85)); border.color: "#1e3a43"
                        ColumnLayout {
                            id: projCol
                            anchors.fill: parent; anchors.margins: 8; spacing: 4
                            Text { text: qsTr("Scratch Studio Session"); color: "#eff9ff"; font.pixelSize: 12; font.weight: Font.DemiBold }
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Multi-chapter project management and EPUB/DOCX import are not yet implemented in this build.")
                                color: "#7f9ca8"; font.pixelSize: 10; wrapMode: Text.WordWrap
                            }
                        }
                    }
                }

                // ------------------------------------------------------------
                // 5. EXPORT (HONEST STATUS)
                // ------------------------------------------------------------
                ColumnLayout {
                    visible: inspectorRoot.currentTab === "export"
                    Layout.fillWidth: true
                    spacing: 8
                    Text { text: qsTr("EXPORT LIBRARY"); color: "#7e9da9"; font.family: "monospace"; font.pixelSize: 10 }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Studio renders WAV output locally via FFmpeg/Piper. Batch M4B/MP3/FLAC export controls will connect to the Rust CLI pipeline in a future milestone.")
                        color: "#7f9ca8"; font.pixelSize: 10; wrapMode: Text.WordWrap
                    }
                }

                // ------------------------------------------------------------
                // 6. SETTINGS (GLASS WALLPAPER + XDG CACHE)
                // ------------------------------------------------------------
                ColumnLayout {
                    visible: inspectorRoot.currentTab === "settings"
                    Layout.fillWidth: true
                    spacing: 12

                    WallpaperSettings {
                        Layout.fillWidth: true
                        accent: inspectorRoot.accent
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: "#162430" }

                    Text { text: qsTr("LOCAL CACHE (XDG)"); color: "#52707d"; font.family: "monospace"; font.pixelSize: 9; font.letterSpacing: 1.2 }
                    Text {
                        text: (typeof activeDoctor !== "undefined" && activeDoctor) ? activeDoctor.xdgCacheDir : "~/.cache/audioticon"
                        color: "#9bb8c2"; font.family: "monospace"; font.pixelSize: 9; elide: Text.ElideMiddle; Layout.fillWidth: true
                    }
                }

                // ------------------------------------------------------------
                // 7. DIAGNOSTICS (DOCTOR MINI-PANEL)
                // ------------------------------------------------------------
                ColumnLayout {
                    visible: inspectorRoot.currentTab === "diagnostics"
                    Layout.fillWidth: true
                    spacing: 10

                    Text { text: qsTr("SYSTEM DIAGNOSTICS (DOCTOR)"); color: "#7e9da9"; font.family: "monospace"; font.pixelSize: 10; font.letterSpacing: 1.2 }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: diagCol.implicitHeight + 16
                        radius: 8
                        color: Qt.rgba(.01, .025, .035, Math.max(0.45, inspectorRoot.glass * 0.85))
                        border.color: (typeof activeDoctor !== "undefined" && activeDoctor && !activeDoctor.isEnvironmentReady && !activeDoctor.inspectionMode) ? "#ff3b56" : "#1e3a43"

                        ColumnLayout {
                            id: diagCol
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Rectangle {
                                    width: 8; height: 8; radius: 4
                                    color: (typeof activeDoctor !== "undefined" && activeDoctor)
                                           ? (activeDoctor.inspectionMode ? "#ffd43b" : (activeDoctor.isEnvironmentReady ? inspectorRoot.accent : "#ff4d6d"))
                                           : inspectorRoot.accent
                                }
                                Text {
                                    text: (typeof activeDoctor !== "undefined" && activeDoctor)
                                          ? (activeDoctor.inspectionMode ? qsTr("Director Mode (no TTS)") : (activeDoctor.isEnvironmentReady ? qsTr("All subsystems OK") : qsTr("Requires repair (%1 errors)").arg(activeDoctor.errorCount)))
                                          : qsTr("Environment OK")
                                    color: "#eff9ff"; font.family: "monospace"; font.pixelSize: 10; font.weight: Font.Bold
                                    Layout.fillWidth: true
                                }
                            }

                            RowLayout {
                                spacing: 6
                                Rectangle { width: 6; height: 6; radius: 3; color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.qtStatus === "ok") ? inspectorRoot.accent : "#ff4d6d" }
                                Text { text: "Qt/FFI"; color: "#9bb8c2"; font.family: "monospace"; font.pixelSize: 9 }
                                Rectangle { width: 6; height: 6; radius: 3; color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.audioStatus === "ok") ? inspectorRoot.accent : "#ffd43b" }
                                Text { text: "Audio"; color: "#9bb8c2"; font.family: "monospace"; font.pixelSize: 9 }
                                Rectangle { width: 6; height: 6; radius: 3; color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.ffmpegStatus === "ok") ? inspectorRoot.accent : "#ff4d6d" }
                                Text { text: "FFmpeg"; color: "#9bb8c2"; font.family: "monospace"; font.pixelSize: 9 }
                            }
                            RowLayout {
                                spacing: 6
                                Rectangle { width: 6; height: 6; radius: 3; color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.piperStatus === "ok") ? inspectorRoot.accent : "#ffd43b" }
                                Text { text: "Piper"; color: "#9bb8c2"; font.family: "monospace"; font.pixelSize: 9 }
                                Rectangle { width: 6; height: 6; radius: 3; color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.hasVoiceModel) ? inspectorRoot.accent : "#ff4d6d" }
                                Text { text: "Model"; color: "#9bb8c2"; font.family: "monospace"; font.pixelSize: 9 }
                                Rectangle { width: 6; height: 6; radius: 3; color: "#4fc3ff" }
                                Text { text: "ORT"; color: "#9bb8c2"; font.family: "monospace"; font.pixelSize: 9 }
                            }

                            Rectangle { Layout.fillWidth: true; height: 1; color: "#162430" }

                            Text {
                                text: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.hasVoiceModel)
                                      ? qsTr("Model: %1").arg(activeDoctor.activeModelName)
                                      : qsTr("No active ONNX model")
                                color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.hasVoiceModel) ? inspectorRoot.accent : "#ff8598"
                                font.family: "monospace"; font.pixelSize: 9; elide: Text.ElideRight; Layout.fillWidth: true
                            }
                        }
                    }

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("🩺 Open full Doctor (Ctrl+7)")
                        font.pixelSize: 11
                        highlighted: true
                        onClicked: {
                            if (typeof win !== "undefined") {
                                win.activeModule = "doctor";
                            }
                        }
                    }

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("📋 Copy diagnostics report")
                        font.pixelSize: 10
                        onClicked: {
                            if (typeof activeDoctor !== "undefined" && activeDoctor) {
                                activeDoctor.copyReportToClipboard();
                            }
                        }
                    }

                    Button {
                        Layout.fillWidth: true
                        text: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.inspectionMode)
                              ? qsTr("⚙️ Disable no-TTS mode")
                              : qsTr("⚙️ Enable no-TTS mode")
                        font.pixelSize: 10
                        onClicked: {
                            if (typeof activeDoctor !== "undefined" && activeDoctor) {
                                activeDoctor.setInspectionMode(!activeDoctor.inspectionMode);
                            }
                        }
                    }
                }
            }
        }
    }
}
