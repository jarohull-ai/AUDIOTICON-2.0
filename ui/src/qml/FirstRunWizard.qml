// FirstRunWizard.qml — Modal First-Run Wizard for Audioticon 2.0 (M13 / M16 P0)
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: wizardRoot
    objectName: "firstRunWizard"

    property color accent: "#22e58b"
    property real glass: 0.72

    signal openDoctorRequested()
    signal wizardCompleted()

    anchors.fill: parent
    z: 100

    // Scrim backdrop
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0.02, 0.04, 0.06, 0.78)
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Select Piper voice model (.onnx)")
        nameFilters: [qsTr("Piper models (*.onnx)"), qsTr("All files (*)")]
        onAccepted: {
            if (typeof activeDoctor !== "undefined" && activeDoctor) {
                activeDoctor.selectModelFile(selectedFile);
            }
        }
    }

    // Modal Glass card — responsive sizing, never overflows 780x560, no dead band on 1920x1080
    Rectangle {
        id: card
        objectName: "firstRunCard"
        anchors.centerIn: parent
        width: Math.min(parent.width - 32, 700)
        readonly property real idealHeight: headerRow.implicitHeight + scrollContent.implicitHeight + footerRow.implicitHeight + 92
        height: Math.min(parent.height - 28, Math.max(420, idealHeight))
        radius: 18
        color: Qt.rgba(0.035, 0.065, 0.09, Math.max(0.76, wizardRoot.glass))
        border.color: Qt.rgba(wizardRoot.accent.r, wizardRoot.accent.g, wizardRoot.accent.b, 0.38)
        border.width: 1
        clip: true

        // Subtle top accent hairline
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 2
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.5; color: wizardRoot.accent }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        ColumnLayout {
            id: cardCol
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            // ================================================================
            // 1. PINNED HEADER WITH WELCOME LANGUAGE SELECTOR
            // ================================================================
            RowLayout {
                id: headerRow
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    width: 40
                    height: 40
                    radius: 11
                    color: Qt.rgba(wizardRoot.accent.r, wizardRoot.accent.g, wizardRoot.accent.b, 0.12)
                    border.color: Qt.rgba(wizardRoot.accent.r, wizardRoot.accent.g, wizardRoot.accent.b, 0.4)
                    Text {
                        anchors.centerIn: parent
                        text: "🎙️"
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
                            text: qsTr("A U D I O T I C O N   2 . 0")
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            font.letterSpacing: 1.4
                            color: "#eff9ff"
                            elide: Text.ElideRight
                        }
                        Rectangle {
                            implicitWidth: badgeTxt.implicitWidth + 8
                            implicitHeight: 16
                            radius: 4
                            color: Qt.rgba(0.13, 0.90, 0.55, 0.14)
                            border.color: "#1b6e4b"
                            Text {
                                id: badgeTxt
                                anchors.centerIn: parent
                                text: qsTr("FIRST-RUN")
                                color: wizardRoot.accent
                                font.family: "monospace"
                                font.pixelSize: 8
                                font.weight: Font.Bold
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("ENVIRONMENT & VOICE MODEL SETUP")
                        font.family: "monospace"
                        font.pixelSize: 9
                        font.letterSpacing: 1.1
                        color: "#6c8c97"
                        elide: Text.ElideRight
                    }
                }

                // Welcome language selector (Auto / EN / PL)
                RowLayout {
                    id: langSelector
                    objectName: "firstRunLangSelector"
                    spacing: 4

                    Repeater {
                        model: [
                            { code: "auto", label: qsTr("Auto") },
                            { code: "en",   label: "EN" },
                            { code: "pl",   label: "PL" }
                        ]

                        Rectangle {
                            required property var modelData
                            readonly property bool active: (typeof i18n !== "undefined" && i18n) && i18n.choice === modelData.code
                            implicitWidth: langLbl.implicitWidth + 14
                            implicitHeight: 24
                            radius: 6
                            color: active
                                   ? Qt.rgba(wizardRoot.accent.r, wizardRoot.accent.g, wizardRoot.accent.b, 0.20)
                                   : Qt.rgba(0.02, 0.05, 0.07, 0.7)
                            border.color: active ? wizardRoot.accent : "#233a47"

                            Text {
                                id: langLbl
                                anchors.centerIn: parent
                                text: modelData.label
                                color: active ? "#eff9ff" : "#8ba8b5"
                                font.family: "monospace"
                                font.pixelSize: 10
                                font.weight: active ? Font.Bold : Font.Normal
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (typeof i18n !== "undefined" && i18n) {
                                        i18n.choice = modelData.code;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#1a2c38"
            }

            // ================================================================
            // 2. SCROLLABLE CHECKLIST BODY (NEVER OVERFLOWS AT 780x560)
            // ================================================================
            ScrollView {
                id: bodyScroll
                objectName: "firstRunScrollView"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                ColumnLayout {
                    id: scrollContent
                    width: bodyScroll.availableWidth
                    spacing: 10

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Audioticon 2.0 is a native, 100% local audio workstation. Before starting, we verify the speech engine, FFmpeg encoders, and audio output device.")
                        color: "#a0bcc6"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        lineHeight: 1.25
                    }

                    // Subsystem checklist
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        // 1. Qt & Rust FFI
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: Math.max(36, qtRow.implicitHeight + 14)
                            radius: 8
                            color: Qt.rgba(0.02, 0.05, 0.07, 0.65)
                            border.color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.qtStatus === "ok") ? "#1b4d3e" : "#ff3b56"

                            RowLayout {
                                id: qtRow
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                anchors.topMargin: 6
                                anchors.bottomMargin: 6
                                spacing: 10

                                Rectangle {
                                    width: 9; height: 9; radius: 5
                                    color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.qtStatus === "ok") ? wizardRoot.accent : "#ff4d6d"
                                }
                                Text {
                                    text: qsTr("Qt 6 Runtime & C++/Rust FFI (44.1 kHz)")
                                    color: "#dff2f8"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.qtStatus === "ok") ? qsTr("[ OK ]") : qsTr("[ FAIL ]")
                                    color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.qtStatus === "ok") ? wizardRoot.accent : "#ff4d6d"
                                    font.family: "monospace"
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                }
                            }
                        }

                        // 2. FFmpeg & Encoders
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: Math.max(36, ffRow.implicitHeight + 14)
                            radius: 8
                            color: Qt.rgba(0.02, 0.05, 0.07, 0.65)
                            border.color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.ffmpegStatus === "ok") ? "#1b4d3e" : "#ff3b56"

                            RowLayout {
                                id: ffRow
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                anchors.topMargin: 6
                                anchors.bottomMargin: 6
                                spacing: 10

                                Rectangle {
                                    width: 9; height: 9; radius: 5
                                    color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.ffmpegStatus === "ok") ? wizardRoot.accent : "#ff4d6d"
                                }
                                Text {
                                    text: qsTr("FFmpeg (loudnorm, FLAC, AAC, ffprobe)")
                                    color: "#dff2f8"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.ffmpegStatus === "ok") ? qsTr("[ OK ]") : qsTr("[ FAIL ]")
                                    color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.ffmpegStatus === "ok") ? wizardRoot.accent : "#ff4d6d"
                                    font.family: "monospace"
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                }
                            }
                        }

                        // 3. Audio Sink
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: Math.max(36, audioRow.implicitHeight + 14)
                            radius: 8
                            color: Qt.rgba(0.02, 0.05, 0.07, 0.65)
                            border.color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.audioStatus === "ok") ? "#1b4d3e" : "#5a4310"

                            RowLayout {
                                id: audioRow
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                anchors.topMargin: 6
                                anchors.bottomMargin: 6
                                spacing: 10

                                Rectangle {
                                    width: 9; height: 9; radius: 5
                                    color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.audioStatus === "ok") ? wizardRoot.accent : "#ffd43b"
                                }
                                Text {
                                    text: qsTr("Audio Device QAudioSink / PipeWire")
                                    color: "#dff2f8"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.audioStatus === "ok")
                                          ? (activeDoctor.audioDeviceName.length > 0 ? activeDoctor.audioDeviceName : qsTr("[ OK ]"))
                                          : qsTr("[ WARN ]")
                                    color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.audioStatus === "ok") ? wizardRoot.accent : "#ffd43b"
                                    font.family: "monospace"
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                    Layout.maximumWidth: 200
                                }
                            }
                        }

                        // 4. Piper-TTS Engine
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: Math.max(36, piperRow.implicitHeight + 14)
                            radius: 8
                            color: Qt.rgba(0.02, 0.05, 0.07, 0.65)
                            border.color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.piperStatus === "ok") ? "#1b4d3e" : "#5a4310"

                            RowLayout {
                                id: piperRow
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                anchors.topMargin: 6
                                anchors.bottomMargin: 6
                                spacing: 10

                                Rectangle {
                                    width: 9; height: 9; radius: 5
                                    color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.piperStatus === "ok") ? wizardRoot.accent : "#ffd43b"
                                }
                                Text {
                                    text: qsTr("Piper-TTS Synthesis Engine (Python / CLI)")
                                    color: "#dff2f8"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.piperStatus === "ok") ? qsTr("[ OK ]") : qsTr("[ NO ENGINE ]")
                                    color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.piperStatus === "ok") ? wizardRoot.accent : "#ffd43b"
                                    font.family: "monospace"
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                }
                            }
                        }

                        // 5. Piper Voice Model — dynamic height so subtitle NEVER overflows border
                        Rectangle {
                            id: modelTile
                            objectName: "firstRunModelTile"
                            Layout.fillWidth: true
                            implicitHeight: Math.max(58, modelRow.implicitHeight + 18)
                            radius: 8
                            color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.hasVoiceModel)
                                   ? Qt.rgba(0.02, 0.05, 0.07, 0.65)
                                   : Qt.rgba(0.20, 0.05, 0.08, 0.42)
                            border.color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.hasVoiceModel) ? "#1b4d3e" : "#ff4d6d"

                            RowLayout {
                                id: modelRow
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                anchors.topMargin: 8
                                anchors.bottomMargin: 8
                                spacing: 10

                                Rectangle {
                                    width: 9; height: 9; radius: 5
                                    color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.hasVoiceModel) ? wizardRoot.accent : "#ff4d6d"
                                }
                                ColumnLayout {
                                    id: modelTextCol
                                    spacing: 3
                                    Layout.fillWidth: true

                                    Text {
                                        Layout.fillWidth: true
                                        text: qsTr("Piper Voice Model (.onnx + .json)")
                                        color: "#eff9ff"
                                        font.pixelSize: 11
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        id: modelSubTxt
                                        objectName: "firstRunModelSubtitle"
                                        Layout.fillWidth: true
                                        text: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.hasVoiceModel)
                                              ? activeDoctor.modelDetails
                                              : qsTr("No voice model in ~/.local/share/audioticon/models")
                                        color: (typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.hasVoiceModel) ? "#9ec2ce" : "#ff8598"
                                        font.family: "monospace"
                                        font.pixelSize: 9
                                        elide: Text.ElideRight
                                    }
                                }

                                Button {
                                    id: selectModelBtn
                                    text: qsTr("Select file...")
                                    font.pixelSize: 10
                                    implicitHeight: 30
                                    onClicked: fileDialog.open()
                                }
                            }
                        }
                    }

                    // Offline & privacy notice box
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: infoCol.implicitHeight + 16
                        radius: 8
                        color: Qt.rgba(0.08, 0.12, 0.16, 0.7)
                        border.color: "#223947"

                        ColumnLayout {
                            id: infoCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 4

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("ℹ PRIVACY & LOCAL RELEASE PRINCIPLE:")
                                color: "#7fa5b5"
                                font.family: "monospace"
                                font.pixelSize: 9
                                font.weight: Font.Bold
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("This app uses no network and no WebView. If you do not have an ONNX model yet, you can work in Director Mode (directive editing, text analysis, and script export without TTS synthesis).")
                                color: "#9dbcc7"
                                font.pixelSize: 10
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#1a2c38"
            }

            // ================================================================
            // 3. PINNED ACTION FOOTER (ALWAYS VISIBLE AT 780x560)
            // ================================================================
            RowLayout {
                id: footerRow
                objectName: "firstRunFooter"
                Layout.fillWidth: true
                spacing: 8

                Button {
                    id: openDoctorBtn
                    objectName: "firstRunOpenDoctorBtn"
                    text: qsTr("🩺 Open Doctor")
                    font.pixelSize: 11
                    implicitHeight: 34
                    onClicked: {
                        wizardRoot.openDoctorRequested();
                    }
                }

                Button {
                    id: noTtsBtn
                    objectName: "firstRunNoTtsBtn"
                    text: qsTr("⚙️ Run without TTS")
                    font.pixelSize: 11
                    implicitHeight: 34
                    onClicked: {
                        if (typeof activeDoctor !== "undefined" && activeDoctor) {
                            activeDoctor.setInspectionMode(true);
                            activeDoctor.completeFirstRun();
                        }
                        wizardRoot.wizardCompleted();
                    }
                }

                Item { Layout.fillWidth: true }

                Button {
                    id: startBtn
                    objectName: "firstRunStartBtn"
                    text: qsTr("🚀 Start working")
                    font.pixelSize: 11
                    font.weight: Font.Bold
                    implicitHeight: 34
                    highlighted: true
                    enabled: (typeof activeDoctor !== "undefined" && activeDoctor)
                             && (activeDoctor.isEnvironmentReady || activeDoctor.inspectionMode)
                    onClicked: {
                        if (typeof activeDoctor !== "undefined" && activeDoctor) {
                            activeDoctor.completeFirstRun();
                        }
                        wizardRoot.wizardCompleted();
                    }
                }
            }
        }
    }
}
