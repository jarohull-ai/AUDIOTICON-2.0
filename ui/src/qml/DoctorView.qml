// DoctorView.qml — Main Doctor Diagnostics view for Audioticon 2.0 (M13 / M16 P0)
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Rectangle {
    id: doctorRoot
    objectName: "doctorView"

    property color accent: "#22e58b"
    property real glass: 0.72
    property string copiedMessage: ""

    readonly property real tileAlpha: Math.max(0.35, Math.min(0.95, glass * 0.92))

    color: Qt.rgba(0.02, 0.04, 0.06, glass)
    radius: 16
    border.color: "#1e3a43"
    clip: true

    signal returnToStudioRequested()

    FileDialog {
        id: doctorFileDialog
        title: qsTr("Select Piper voice model (.onnx)")
        nameFilters: [qsTr("Piper models (*.onnx)"), qsTr("All files (*)")]
        onAccepted: {
            if (typeof activeDoctor !== "undefined" && activeDoctor) {
                activeDoctor.selectModelFile(selectedFile);
            }
        }
    }

    Timer {
        id: copiedTimer
        interval: 3000
        onTriggered: doctorRoot.copiedMessage = ""
    }

    ScrollView {
        id: doctorScroll
        anchors.fill: parent
        anchors.margins: 18
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: doctorScroll.availableWidth
            spacing: 14

            // ================================================================
            // 1. VIEW HEADER
            // ================================================================
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    width: 42
                    height: 42
                    radius: 12
                    color: Qt.rgba(doctorRoot.accent.r, doctorRoot.accent.g, doctorRoot.accent.b, 0.12)
                    border.color: Qt.rgba(doctorRoot.accent.r, doctorRoot.accent.g, doctorRoot.accent.b, 0.4)
                    Text {
                        anchors.centerIn: parent
                        text: "🩺"
                        font.pixelSize: 20
                    }
                }

                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true

                    RowLayout {
                        spacing: 8
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("DOCTOR · ENVIRONMENT DIAGNOSTICS")
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            font.letterSpacing: 1.1
                            color: "#eff9ff"
                            elide: Text.ElideRight
                        }
                        Rectangle {
                            implicitWidth: badgeModeTxt.implicitWidth + 10
                            implicitHeight: 18
                            radius: 4
                            color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.inspectionMode)
                                   ? Qt.rgba(1.0, 0.83, 0.23, 0.14)
                                   : Qt.rgba(0.13, 0.90, 0.55, 0.14)
                            border.color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.inspectionMode)
                                          ? "#8f7014"
                                          : "#1b6e4b"
                            Text {
                                id: badgeModeTxt
                                anchors.centerIn: parent
                                text: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.inspectionMode)
                                      ? qsTr("DIRECTOR MODE")
                                      : qsTr("FULL MODE (TTS)")
                                color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.inspectionMode)
                                       ? "#ffd43b"
                                       : doctorRoot.accent
                                font.family: "monospace"
                                font.pixelSize: 8
                                font.weight: Font.Bold
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("LOCAL DEPENDENCIES, ENCODERS, ENGINES & XDG PATHS")
                        font.family: "monospace"
                        font.pixelSize: 9
                        font.letterSpacing: 1.1
                        color: "#6c8c97"
                        elide: Text.ElideRight
                    }
                }

                Button {
                    text: qsTr("🎙️ Back to Studio")
                    font.pixelSize: 11
                    onClicked: doctorRoot.returnToStudioRequested()
                }
            }

            // ================================================================
            // 2. OVERALL STATUS CARD & WRAPPING ACTION FLOW
            // ================================================================
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: statusCol.implicitHeight + 22
                radius: 12
                color: {
                    if (typeof activeDoctor === "undefined" || !activeDoctor) return Qt.rgba(0.05, 0.09, 0.12, doctorRoot.tileAlpha);
                    if (activeDoctor.inspectionMode) return Qt.rgba(0.20, 0.15, 0.05, Math.min(0.65, doctorRoot.tileAlpha));
                    if (activeDoctor.isEnvironmentReady) return Qt.rgba(0.04, 0.18, 0.12, Math.min(0.65, doctorRoot.tileAlpha));
                    return Qt.rgba(0.22, 0.06, 0.08, Math.min(0.65, doctorRoot.tileAlpha));
                }
                border.color: {
                    if (typeof activeDoctor === "undefined" || !activeDoctor) return "#1e3a43";
                    if (activeDoctor.inspectionMode) return "#b38612";
                    if (activeDoctor.isEnvironmentReady) return "#1b6e4b";
                    return "#ff3b56";
                }

                ColumnLayout {
                    id: statusCol
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Rectangle {
                            width: 11
                            height: 11
                            radius: 6
                            color: {
                                if (typeof activeDoctor === "undefined" || !activeDoctor) return "#6c8c97";
                                if (activeDoctor.inspectionMode) return "#ffd43b";
                                if (activeDoctor.isEnvironmentReady) return doctorRoot.accent;
                                return "#ff4d6d";
                            }
                        }

                        Text {
                            text: {
                                if (typeof activeDoctor === "undefined" || !activeDoctor) return qsTr("Status: Unknown");
                                if (activeDoctor.inspectionMode) {
                                    return qsTr("ENVIRONMENT READY IN DIRECTOR MODE (WITHOUT TTS SYNTHESIS)");
                                }
                                if (activeDoctor.isEnvironmentReady) {
                                    return qsTr("ENVIRONMENT READY FOR FULL WORK & VOICE SYNTHESIS (100% OFFLINE)");
                                }
                                return qsTr("ENVIRONMENT ISSUES DETECTED (%1 errors, %2 warnings)")
                                       .arg(activeDoctor.errorCount).arg(activeDoctor.warningCount);
                            }
                            color: "#eff9ff"
                            font.family: "monospace"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Text {
                            visible: doctorRoot.copiedMessage.length > 0
                            text: doctorRoot.copiedMessage
                            color: doctorRoot.accent
                            font.family: "monospace"
                            font.pixelSize: 10
                            font.weight: Font.Bold
                        }
                    }

                    // Wrapping action flow — never clips at 780x560 or 1180x850
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8

                        Button {
                            text: qsTr("📋 Copy full report")
                            font.pixelSize: 10
                            implicitHeight: 30
                            onClicked: {
                                if (typeof activeDoctor !== "undefined" && activeDoctor) {
                                    activeDoctor.copyReportToClipboard();
                                    doctorRoot.copiedMessage = qsTr("✓ Report copied to clipboard!");
                                    copiedTimer.restart();
                                }
                            }
                        }

                        Button {
                            text: qsTr("🔄 Refresh status")
                            font.pixelSize: 10
                            implicitHeight: 30
                            onClicked: {
                                if (typeof activeDoctor !== "undefined" && activeDoctor) {
                                    activeDoctor.refresh();
                                    doctorRoot.copiedMessage = qsTr("✓ Refreshed!");
                                    copiedTimer.restart();
                                }
                            }
                        }

                        Button {
                            text: qsTr("📁 Select model...")
                            font.pixelSize: 10
                            implicitHeight: 30
                            onClicked: doctorFileDialog.open()
                        }

                        Button {
                            text: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.inspectionMode)
                                  ? qsTr("⚙️ Disable no-TTS mode")
                                  : qsTr("⚙️ Enable no-TTS mode")
                            font.pixelSize: 10
                            implicitHeight: 30
                            onClicked: {
                                if (typeof activeDoctor !== "undefined" && activeDoctor) {
                                    activeDoctor.setInspectionMode(!activeDoctor.inspectionMode);
                                }
                            }
                        }

                        Button {
                            text: qsTr("↺ Reset First-Run")
                            font.pixelSize: 10
                            implicitHeight: 30
                            onClicked: {
                                if (typeof activeDoctor !== "undefined" && activeDoctor) {
                                    activeDoctor.resetFirstRun();
                                    doctorRoot.copiedMessage = qsTr("✓ First-Run reset");
                                    copiedTimer.restart();
                                }
                            }
                        }
                    }
                }
            }

            // ================================================================
            // 3. SUBSYSTEM LED TILES (6 TILES, UNIFIED GLASS OPACITY)
            // ================================================================
            Text {
                text: qsTr("SUBSYSTEMS & RUNTIME DEPENDENCIES")
                color: "#52737e"
                font.family: "monospace"
                font.pixelSize: 10
                font.letterSpacing: 1.4
            }

            GridLayout {
                Layout.fillWidth: true
                columns: (doctorRoot.width > 820) ? 2 : 1
                rowSpacing: 10
                columnSpacing: 10

                // TILE 1: Qt 6 & Rust FFI
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 112
                    radius: 10
                    color: Qt.rgba(0.03, 0.06, 0.08, doctorRoot.tileAlpha)
                    border.color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.qtStatus === "ok") ? "#1b4d3e" : "#ff3b56"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Rectangle { width: 8; height: 8; radius: 4; color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.qtStatus === "ok") ? doctorRoot.accent : "#ff4d6d" }
                            Text { text: qsTr("1. Qt 6 & Core C++/Rust FFI"); color: "#eff9ff"; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                            Rectangle {
                                implicitWidth: 36; implicitHeight: 16; radius: 3; color: Qt.rgba(0.13, 0.90, 0.55, 0.15)
                                Text { anchors.centerIn: parent; text: "OK"; color: doctorRoot.accent; font.family: "monospace"; font.pixelSize: 9; font.weight: Font.Bold }
                            }
                        }
                        Text {
                            text: (typeof activeDoctor !== "undefined" && activeDoctor)
                                  ? qsTr("Version: Qt %1 · Platform: %2").arg(activeDoctor.qtVersion).arg(activeDoctor.qtPlatform)
                                  : ""
                            color: "#9dbcc7"; font.family: "monospace"; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true
                        }
                        Text {
                            text: (typeof activeDoctor !== "undefined" && activeDoctor)
                                  ? qsTr("Rust Tokio FFI: %1 Hz (statically linked)").arg(activeDoctor.coreSampleRate)
                                  : ""
                            color: doctorRoot.accent; font.family: "monospace"; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true
                        }
                    }
                }

                // TILE 2: Audio Device (QAudioSink)
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 112
                    radius: 10
                    color: Qt.rgba(0.03, 0.06, 0.08, doctorRoot.tileAlpha)
                    border.color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.audioStatus === "ok") ? "#1b4d3e" : "#8f7014"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.audioStatus === "ok") ? doctorRoot.accent : "#ffd43b"
                            }
                            Text { text: qsTr("2. Audio Device (QAudioSink)"); color: "#eff9ff"; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                            Rectangle {
                                implicitWidth: 42; implicitHeight: 16; radius: 3
                                color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.audioStatus === "ok") ? Qt.rgba(0.13, 0.90, 0.55, 0.15) : Qt.rgba(1.0, 0.83, 0.23, 0.15)
                                Text {
                                    anchors.centerIn: parent
                                    text: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.audioStatus === "ok") ? "OK" : "WARN"
                                    color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.audioStatus === "ok") ? doctorRoot.accent : "#ffd43b"
                                    font.family: "monospace"; font.pixelSize: 9; font.weight: Font.Bold
                                }
                            }
                        }
                        Text {
                            text: (typeof activeDoctor !== "undefined" && activeDoctor)
                                  ? qsTr("Active output: %1").arg(activeDoctor.audioDeviceName)
                                  : ""
                            color: "#9dbcc7"; font.family: "monospace"; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true
                        }
                        Text {
                            text: (typeof activeDoctor !== "undefined" && activeDoctor) ? activeDoctor.audioDetails : ""
                            color: "#6c8c97"; font.family: "monospace"; font.pixelSize: 9; elide: Text.ElideRight; Layout.fillWidth: true
                        }
                    }
                }

                // TILE 3: FFmpeg & Encoders
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 112
                    radius: 10
                    color: Qt.rgba(0.03, 0.06, 0.08, doctorRoot.tileAlpha)
                    border.color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.ffmpegStatus === "ok") ? "#1b4d3e" : "#ff3b56"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Rectangle { width: 8; height: 8; radius: 4; color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.ffmpegStatus === "ok") ? doctorRoot.accent : "#ff4d6d" }
                            Text { text: qsTr("3. FFmpeg & Export Tools"); color: "#eff9ff"; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                            Rectangle {
                                implicitWidth: 36; implicitHeight: 16; radius: 3
                                color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.ffmpegStatus === "ok") ? Qt.rgba(0.13, 0.90, 0.55, 0.15) : Qt.rgba(1.0, 0.23, 0.35, 0.15)
                                Text {
                                    anchors.centerIn: parent
                                    text: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.ffmpegStatus === "ok") ? "OK" : "FAIL"
                                    color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.ffmpegStatus === "ok") ? doctorRoot.accent : "#ff4d6d"
                                    font.family: "monospace"; font.pixelSize: 9; font.weight: Font.Bold
                                }
                            }
                        }
                        Text {
                            text: (typeof activeDoctor !== "undefined" && activeDoctor)
                                  ? qsTr("Version: %1 (%2)").arg(activeDoctor.ffmpegVersion).arg(activeDoctor.ffmpegPath)
                                  : ""
                            color: "#9dbcc7"; font.family: "monospace"; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true
                        }
                        Text {
                            text: (typeof activeDoctor !== "undefined" && activeDoctor)
                                  ? qsTr("Filters/Encoders: loudnorm=%1 · FLAC=%2 · AAC=%3")
                                    .arg(activeDoctor.hasLoudnorm ? qsTr("YES") : qsTr("MISSING"))
                                    .arg(activeDoctor.hasFlac ? qsTr("YES") : qsTr("MISSING"))
                                    .arg(activeDoctor.hasAac ? qsTr("YES") : qsTr("MISSING"))
                                  : ""
                            color: doctorRoot.accent; font.family: "monospace"; font.pixelSize: 9; elide: Text.ElideRight; Layout.fillWidth: true
                        }
                    }
                }

                // TILE 4: Piper-TTS Engine
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 112
                    radius: 10
                    color: Qt.rgba(0.03, 0.06, 0.08, doctorRoot.tileAlpha)
                    border.color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.piperStatus === "ok") ? "#1b4d3e" : "#8f7014"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.piperStatus === "ok") ? doctorRoot.accent : "#ffd43b"
                            }
                            Text { text: qsTr("4. Piper-TTS Synthesis Engine"); color: "#eff9ff"; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                            Rectangle {
                                implicitWidth: 42; implicitHeight: 16; radius: 3
                                color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.piperStatus === "ok") ? Qt.rgba(0.13, 0.90, 0.55, 0.15) : Qt.rgba(1.0, 0.83, 0.23, 0.15)
                                Text {
                                    anchors.centerIn: parent
                                    text: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.piperStatus === "ok") ? "OK" : "WARN"
                                    color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.piperStatus === "ok") ? doctorRoot.accent : "#ffd43b"
                                    font.family: "monospace"; font.pixelSize: 9; font.weight: Font.Bold
                                }
                            }
                        }
                        Text {
                            text: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.piperPath.length > 0)
                                  ? qsTr("Engine path: %1").arg(activeDoctor.piperPath)
                                  : qsTr("Piper engine missing (use Director Mode or install)")
                            color: "#9dbcc7"; font.family: "monospace"; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true
                        }
                        Text {
                            text: (typeof activeDoctor !== "undefined" && activeDoctor) ? activeDoctor.piperDetails : ""
                            color: "#6c8c97"; font.family: "monospace"; font.pixelSize: 9; elide: Text.ElideRight; Layout.fillWidth: true
                        }
                    }
                }

                // TILE 5: Piper Voice Model
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 112
                    radius: 10
                    color: Qt.rgba(0.03, 0.06, 0.08, doctorRoot.tileAlpha)
                    border.color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.hasVoiceModel) ? "#1b4d3e" : "#ff4d6d"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.hasVoiceModel) ? doctorRoot.accent : "#ff4d6d"
                            }
                            Text { text: qsTr("5. Active Voice Model (.onnx)"); color: "#eff9ff"; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                            Rectangle {
                                implicitWidth: 42; implicitHeight: 16; radius: 3
                                color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.hasVoiceModel) ? Qt.rgba(0.13, 0.90, 0.55, 0.15) : Qt.rgba(1.0, 0.23, 0.35, 0.15)
                                Text {
                                    anchors.centerIn: parent
                                    text: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.hasVoiceModel) ? "OK" : "FAIL"
                                    color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.hasVoiceModel) ? doctorRoot.accent : "#ff4d6d"
                                    font.family: "monospace"; font.pixelSize: 9; font.weight: Font.Bold
                                }
                            }
                        }
                        Text {
                            text: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.hasVoiceModel)
                                  ? qsTr("Model: %1").arg(activeDoctor.activeModelName)
                                  : qsTr("No voice model in ~/.local/share/audioticon/models")
                            color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.hasVoiceModel) ? "#eff9ff" : "#ff8598"
                            font.family: "monospace"; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true
                        }
                        Text {
                            text: (typeof activeDoctor !== "undefined" && activeDoctor)
                                  ? qsTr("Installed models: %1 · %2")
                                    .arg(activeDoctor.installedModelsCount)
                                    .arg(activeDoctor.modelDetails)
                                  : ""
                            color: "#6c8c97"; font.family: "monospace"; font.pixelSize: 9; elide: Text.ElideRight; Layout.fillWidth: true
                        }
                    }
                }

                // TILE 6: ONNX Runtime (Optional)
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 112
                    radius: 10
                    color: Qt.rgba(0.03, 0.06, 0.08, doctorRoot.tileAlpha)
                    border.color: "#1e3a43"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Rectangle { width: 8; height: 8; radius: 4; color: "#4fc3ff" }
                            Text { text: qsTr("6. ONNX Runtime (libonnxruntime)"); color: "#eff9ff"; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                            Rectangle {
                                implicitWidth: optBadgeTxt.implicitWidth + 10; implicitHeight: 16; radius: 3; color: Qt.rgba(0.31, 0.76, 1.0, 0.15)
                                Text { id: optBadgeTxt; anchors.centerIn: parent; text: qsTr("OPTIONAL"); color: "#4fc3ff"; font.family: "monospace"; font.pixelSize: 8; font.weight: Font.Bold }
                            }
                        }
                        Text {
                            text: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.ortPath.length > 0)
                                  ? qsTr("Library: %1").arg(activeDoctor.ortPath)
                                  : qsTr("No standalone ORT (Piper uses bundled ORT)")
                            color: "#9dbcc7"; font.family: "monospace"; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true
                        }
                        Text {
                            text: qsTr("Piper voices work without system ORT. Optional for experimental Kokoro voices.")
                            color: "#6c8c97"; font.pixelSize: 9; wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }
                    }
                }
            }

            // ================================================================
            // 4. XDG DIRECTORY PERMISSIONS CARD
            // ================================================================
            Text {
                text: qsTr("DIRECTORY STRUCTURE & PERMISSIONS (XDG)")
                color: "#52737e"
                font.family: "monospace"
                font.pixelSize: 10
                font.letterSpacing: 1.4
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: xdgCol.implicitHeight + 20
                radius: 10
                color: Qt.rgba(0.03, 0.06, 0.08, doctorRoot.tileAlpha)
                border.color: "#1e3a43"

                ColumnLayout {
                    id: xdgCol
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: qsTr("📁 App data (XDG_DATA):"); color: "#7fa5b5"; font.family: "monospace"; font.pixelSize: 10; Layout.preferredWidth: 190 }
                        Text { text: (typeof activeDoctor !== "undefined" && activeDoctor) ? activeDoctor.xdgDataDir : ""; color: "#eff9ff"; font.family: "monospace"; font.pixelSize: 10; elide: Text.ElideMiddle; Layout.fillWidth: true }
                        Text { text: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.xdgDataWritable) ? qsTr("[ Write OK ]") : qsTr("[ ERROR ]"); color: doctorRoot.accent; font.family: "monospace"; font.pixelSize: 10 }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: qsTr("🎙️ Voice models (MODELS):"); color: "#7fa5b5"; font.family: "monospace"; font.pixelSize: 10; Layout.preferredWidth: 190 }
                        Text { text: (typeof activeDoctor !== "undefined" && activeDoctor) ? activeDoctor.xdgModelsDir : ""; color: "#eff9ff"; font.family: "monospace"; font.pixelSize: 10; elide: Text.ElideMiddle; Layout.fillWidth: true }
                        Text { text: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.xdgModelsWritable) ? qsTr("[ Write OK ]") : qsTr("[ ERROR ]"); color: doctorRoot.accent; font.family: "monospace"; font.pixelSize: 10 }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: qsTr("⚡ Segment cache (CACHE):"); color: "#7fa5b5"; font.family: "monospace"; font.pixelSize: 10; Layout.preferredWidth: 190 }
                        Text { text: (typeof activeDoctor !== "undefined" && activeDoctor) ? activeDoctor.xdgCacheDir : ""; color: "#eff9ff"; font.family: "monospace"; font.pixelSize: 10; elide: Text.ElideMiddle; Layout.fillWidth: true }
                        Text { text: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.xdgCacheWritable) ? qsTr("[ Write OK ]") : qsTr("[ ERROR ]"); color: doctorRoot.accent; font.family: "monospace"; font.pixelSize: 10 }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: qsTr("💾 Render outputs (OUTPUTS):"); color: "#7fa5b5"; font.family: "monospace"; font.pixelSize: 10; Layout.preferredWidth: 190 }
                        Text { text: (typeof activeDoctor !== "undefined" && activeDoctor) ? activeDoctor.xdgOutputsDir : ""; color: "#eff9ff"; font.family: "monospace"; font.pixelSize: 10; elide: Text.ElideMiddle; Layout.fillWidth: true }
                        Text { text: qsTr("[ Active ]"); color: doctorRoot.accent; font.family: "monospace"; font.pixelSize: 10 }
                    }
                }
            }
        }
    }
}
