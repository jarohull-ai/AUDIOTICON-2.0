// StudioView.qml — Główny warsztat reżyserski Audioticon 2.0 (M9 Studio Dark Glass)
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: studioRoot
    objectName: "studioView"
    property color accent: "#22e58b"
    property real glass: .88
    property var selectedSegment: null
    property int selectedIndex: -1

    function formatTimeMs(ms) {
        var clamped = Math.max(0, ms);
        var totalSec = Math.floor(clamped / 1000);
        var m = Math.floor(totalSec / 60);
        var s = totalSec % 60;
        var tenths = Math.floor((clamped % 1000) / 10);
        return (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s + "." + (tenths < 10 ? "0" : "") + tenths;
    }

    signal segmentSelected(var segment, int index)
    signal inspectRequested()

    FileDialog {
        id: studioFileDialog
        title: qsTr("Select Piper voice model (.onnx)")
        nameFilters: [qsTr("Piper models (*.onnx)"), qsTr("All files (*)")]
        onAccepted: {
            if (typeof activeDoctor !== "undefined" && activeDoctor) {
                activeDoctor.selectModelFile(selectedFile);
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        // ====================================================================
        // 1. DIRECTIVE COMPOSER (Edytor reżyserii emoji — bez kolizji!)
        // ====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: studioRoot.height * 0.38
            Layout.minimumHeight: 205
            radius: 14
            color: Qt.rgba(.04, .065, .09, studioRoot.glass)
            border.color: composerInput.activeFocus
                          ? Qt.rgba(studioRoot.accent.r, studioRoot.accent.g, studioRoot.accent.b, .48)
                          : "#1a2632"
            clip: true

            Behavior on border.color { ColorAnimation { duration: 140 } }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // A. Wydzielony pasek narzędziowy dyrektyw emoji (Toolbar)
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: directiveRow.implicitHeight + 12
                    radius: 9
                    color: "#091017"
                    border.color: "#162430"

                    RowLayout {
                        id: directiveRow
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 8

                        Text {
                            text: qsTr("DIRECTIVES")
                            color: "#52707d"
                            font.family: "monospace"
                            font.pixelSize: 9
                            font.weight: Font.Bold
                            font.letterSpacing: 1.4
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 5

                            Repeater {
                                model: [
                                    { token: "📖 ", hint: "Rozdz.", label: "Rozdział" },
                                    { token: "🛑 ", hint: "900ms",  label: "Stop 900ms" },
                                    { token: "⏸️ ", hint: "400ms",  label: "Pauza 400ms" },
                                    { token: "😮‍💨 ", hint: "Oddech", label: "Oddech lektora" },
                                    { token: "🐢 ", hint: "0.85x",  label: "Zwolnij tempo" },
                                    { token: "🐇 ", hint: "1.15x",  label: "Przyspiesz tempo" },
                                    { token: "📻 ", hint: "Radio",  label: "Filtr Radio DSP" },
                                    { token: "📱 ", hint: "Tel.",   label: "Filtr Telefon DSP" },
                                    { token: "🏛️ ", hint: "Katedra",label: "Pogłos Katedra" },
                                    { token: "🤫 ", hint: "Szept",  label: "Tryb szeptu" }
                                ]

                                Rectangle {
                                    id: chipRect
                                    required property var modelData
                                    implicitWidth: chipContent.implicitWidth + 12
                                    implicitHeight: 26
                                    radius: 6
                                    color: chipMouse.containsMouse ? "#15232e" : "#0e1720"
                                    border.color: chipMouse.containsMouse ? Qt.rgba(studioRoot.accent.r, studioRoot.accent.g, studioRoot.accent.b, 0.5) : "#1d2e3b"
                                    scale: chipMouse.pressed ? 0.96 : 1.0

                                    Behavior on scale { NumberAnimation { duration: 80 } }
                                    Behavior on color { ColorAnimation { duration: 100 } }

                                    RowLayout {
                                        id: chipContent
                                        anchors.centerIn: parent
                                        spacing: 4

                                        Text {
                                            text: chipRect.modelData.token.trim()
                                            font.pixelSize: 12
                                        }
                                        Text {
                                            visible: studioRoot.width > 760
                                            text: chipRect.modelData.hint
                                            color: chipMouse.containsMouse ? "#d6ecf5" : "#6c8a99"
                                            font.family: "monospace"
                                            font.pixelSize: 9
                                        }
                                    }

                                    MouseArea {
                                        id: chipMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            var pos = composerInput.cursorPosition;
                                            composerInput.insert(pos, chipRect.modelData.token);
                                            composerInput.forceActiveFocus();
                                        }
                                    }

                                    ToolTip.visible: chipMouse.containsMouse
                                    ToolTip.text: chipRect.modelData.label + " (" + chipRect.modelData.token.trim() + ")"
                                    ToolTip.delay: 350
                                }
                            }
                        }

                        Text {
                            id: wordCounter
                            text: qsTr("%1 chars").arg(composerInput.text.length)
                            color: "#52707d"
                            font.family: "monospace"
                            font.pixelSize: 10
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }
                }

                // B. Odizolowane pole edycyjne z numeracją linii
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "#080e14"
                    border.color: "#14202b"
                    clip: true

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 0

                        // Numeracja linii (gutter)
                        Rectangle {
                            Layout.preferredWidth: 28
                            Layout.fillHeight: true
                            color: "#0a1118"
                            border.color: "#131e28"

                            Column {
                                anchors.top: parent.top
                                anchors.topMargin: 8
                                anchors.horizontalCenter: parent.horizontalCenter
                                spacing: 4
                                Repeater {
                                    model: Math.max(1, composerInput.lineCount)
                                    Text {
                                        required property int index
                                        text: index + 1
                                        color: "#385260"
                                        font.family: "monospace"
                                        font.pixelSize: 11
                                        height: 18
                                    }
                                }
                            }
                        }

                        ScrollView {
                            id: composerScroll
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            TextArea {
                                id: composerInput
                                objectName: "composerTextArea"
                                width: composerScroll.availableWidth
                                placeholderText: composerInput.length === 0 ? qsTr("Type or paste text with directive markers (e.g. '📖 Chapter 1: Beginning. 🛑 Darkness fell over the valley... 🐢')") : ""
                                placeholderTextColor: "#3e5966"
                                color: "#e8f4fa"
                                selectionColor: Qt.rgba(studioRoot.accent.r, studioRoot.accent.g, studioRoot.accent.b, 0.28)
                                selectedTextColor: "#ffffff"
                                font.pixelSize: 14
                                font.family: "sans-serif"
                                wrapMode: TextArea.Wrap
                                selectByMouse: true
                                leftPadding: 10
                                rightPadding: 10
                                topPadding: 8
                                bottomPadding: 8
                                background: Item {}
                                text: "📖 Rozdział 1: Początek. 🛑 Wtedy lektor rzekł spokojnym głosem: 🐢 'Czas wyruszyć w drogę'. ⏸️ 📻"
                                onTextChanged: {
                                    parseTimer.restart();
                                }
                            }
                        }
                    }
                }

                Timer {
                    id: parseTimer
                    interval: 200
                    repeat: false
                    onTriggered: {
                        if (typeof studio !== "undefined" && studio) {
                            studio.parseDirectedText(composerInput.text);
                        }
                    }
                }

                // C. Pasek akcji kompozytora i banner błędu
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Rectangle {
                        implicitWidth: parseBtnRow.implicitWidth + 20
                        implicitHeight: 30
                        radius: 7
                        color: parseMouse.containsMouse ? "#162633" : "#0f1a24"
                        border.color: "#243b4a"
                        scale: parseMouse.pressed ? 0.97 : 1.0
                        Behavior on scale { NumberAnimation { duration: 70 } }

                        RowLayout {
                            id: parseBtnRow
                            anchors.centerIn: parent
                            spacing: 6
                            Text { text: "🎙️"; font.pixelSize: 12 }
                            Text { text: qsTr("Direct (C++ E1)"); color: "#c9e2ed"; font.pixelSize: 12; font.weight: Font.Medium }
                        }
                        MouseArea {
                            id: parseMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof studio !== "undefined" && studio) {
                                    studio.parseDirectedText(composerInput.text);
                                }
                            }
                        }
                    }

                    Rectangle {
                        id: renderBtn
                        readonly property bool busy: typeof studio !== "undefined" && studio && studio.isBusy
                        implicitWidth: synthBtnRow.implicitWidth + 22
                        implicitHeight: 30
                        radius: 7
                        color: busy
                               ? Qt.rgba(0.0, 0.82, 1.0, 0.14)
                               : (synthMouse.containsMouse ? Qt.rgba(0.13, 0.90, 0.55, 0.24) : Qt.rgba(0.13, 0.90, 0.55, 0.16))
                        border.color: busy ? "#00d2ff" : studioRoot.accent
                        scale: synthMouse.pressed && !busy ? 0.97 : 1.0
                        Behavior on scale { NumberAnimation { duration: 70 } }

                        RowLayout {
                            id: synthBtnRow
                            anchors.centerIn: parent
                            spacing: 6
                            Text { text: renderBtn.busy ? "⏳" : "▶"; color: renderBtn.busy ? "#00d2ff" : studioRoot.accent; font.pixelSize: 11 }
                            Text {
                                text: renderBtn.busy
                                      ? qsTr("Synthesizing (%1%)").arg(Math.round(studio.taskPercent))
                                      : qsTr("Synthesize chapter")
                                color: "#eff9ff"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }
                        }
                        MouseArea {
                            id: synthMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: !renderBtn.busy
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: {
                                if (typeof studio !== "undefined" && studio) {
                                    studio.startRender(composerInput.text);
                                }
                            }
                        }
                    }

                    Rectangle {
                        visible: typeof studio !== "undefined" && studio && studio.isBusy
                        implicitWidth: cancelTxt.implicitWidth + 18
                        implicitHeight: 30
                        radius: 7
                        color: cancelMouse.containsMouse ? "#38141c" : "#260f15"
                        border.color: "#ff3b56"
                        Text { id: cancelTxt; anchors.centerIn: parent; text: qsTr("⏹ Cancel"); color: "#ff758a"; font.pixelSize: 12 }
                        MouseArea {
                            id: cancelMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof studio !== "undefined" && studio) {
                                    studio.cancelRender();
                                }
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        visible: (typeof studio !== "undefined" && studio)
                                 && studio.errorMessage.length === 0
                                 && studio.playbackError.length === 0
                                 && studio.statusMessage.length > 0
                        text: (typeof studio !== "undefined" && studio) ? studio.statusMessage : ""
                        color: (typeof studio !== "undefined" && studio && studio.isBusy) ? "#00d2ff" : "#7898a6"
                        font.family: "monospace"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.maximumWidth: 280
                    }
                }
            }
        }

        // Dedicated non-clipping Error / Missing-Model Banner (never overflows at 780x560 or 1180x850)
        Rectangle {
            id: studioErrorBanner
            objectName: "studioErrorBanner"
            readonly property string rawErr: (typeof studio !== "undefined" && studio)
                                             ? (studio.errorMessage.length > 0 ? studio.errorMessage : studio.playbackError)
                                             : ""
            readonly property bool isMissingModel: rawErr.toLowerCase().indexOf("model") !== -1
                                                   || rawErr.toLowerCase().indexOf("głosu") !== -1
            readonly property string displayErr: isMissingModel
                                                 ? qsTr("Missing voice model (set AUDIOTICON_PIPER_MODEL or select a file)")
                                                 : rawErr
            visible: rawErr.length > 0
            Layout.fillWidth: true
            implicitHeight: errBannerFlow.implicitHeight + 14
            radius: 8
            color: Qt.rgba(0.20, 0.05, 0.08, 0.78)
            border.color: "#ff3b56"

            Flow {
                id: errBannerFlow
                anchors.fill: parent
                anchors.margins: 7
                spacing: 8

                Text {
                    text: "⚠ " + studioErrorBanner.displayErr
                    color: "#ff8598"
                    font.family: "monospace"
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    height: 24
                    verticalAlignment: Text.AlignVCenter
                }

                Button {
                    visible: studioErrorBanner.isMissingModel
                    text: qsTr("📁 Select model...")
                    font.pixelSize: 10
                    implicitHeight: 24
                    onClicked: studioFileDialog.open()
                }

                Button {
                    visible: studioErrorBanner.isMissingModel
                    text: qsTr("🩺 Doctor")
                    font.pixelSize: 10
                    implicitHeight: 24
                    onClicked: {
                        if (typeof win !== "undefined") {
                            win.activeModule = "doctor";
                        }
                    }
                }

                Button {
                    visible: studioErrorBanner.isMissingModel
                    text: qsTr("⚙️ No-TTS mode")
                    font.pixelSize: 10
                    implicitHeight: 24
                    onClicked: {
                        if (typeof activeDoctor !== "undefined" && activeDoctor) {
                            activeDoctor.setInspectionMode(true);
                        }
                    }
                }
            }
        }

        // Director Mode banner (when running without TTS)
        Rectangle {
            visible: typeof activeDoctor !== "undefined" && activeDoctor && activeDoctor.inspectionMode
            Layout.fillWidth: true
            implicitHeight: 28
            radius: 6
            color: Qt.rgba(1.0, 0.83, 0.23, 0.12)
            border.color: "#8f7014"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8
                Text { text: "🎭"; font.pixelSize: 12 }
                Text {
                    text: qsTr("DIRECTOR MODE ACTIVE — Text analysis, emoji direction, and mastering without TTS synthesis.")
                    color: "#ffd43b"
                    font.family: "monospace"
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Button {
                    text: qsTr("🩺 Open Doctor")
                    font.pixelSize: 9
                    implicitHeight: 20
                    onClicked: {
                        if (typeof win !== "undefined") {
                            win.activeModule = "doctor";
                        }
                    }
                }
            }
        }

        // ====================================================================
        // 2. SEGMENT TIMELINE MATRIX (Lista zdań, LED Cache Tiles i Waveforms)
        // ====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 14
            color: Qt.rgba(.035, .055, .078, studioRoot.glass)
            border.color: "#1a2632"
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 6

                // Nagłówek tabeli segmentów
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    spacing: 8

                    Text { text: "#"; color: "#4e6b78"; font.family: "monospace"; font.pixelSize: 10; font.weight: Font.Bold; width: 24 }
                    Text { text: qsTr("SEGMENT (VERBALIZED)"); color: "#4e6b78"; font.family: "monospace"; font.pixelSize: 10; font.weight: Font.Bold; font.letterSpacing: 1.0; Layout.fillWidth: true }
                    Text { text: qsTr("TEMPO"); color: "#4e6b78"; font.family: "monospace"; font.pixelSize: 10; font.weight: Font.Bold; width: 48; horizontalAlignment: Text.AlignHCenter }
                    Text { text: qsTr("DSP"); color: "#4e6b78"; font.family: "monospace"; font.pixelSize: 10; font.weight: Font.Bold; width: 58; horizontalAlignment: Text.AlignHCenter }
                    Text { text: qsTr("PAUSE"); color: "#4e6b78"; font.family: "monospace"; font.pixelSize: 10; font.weight: Font.Bold; width: 48; horizontalAlignment: Text.AlignHCenter }
                    Text { text: qsTr("CACHE"); color: "#4e6b78"; font.family: "monospace"; font.pixelSize: 10; font.weight: Font.Bold; width: 54; horizontalAlignment: Text.AlignHCenter }
                    Text { visible: studioRoot.width > 680; text: qsTr("WAVE"); color: "#4e6b78"; font.family: "monospace"; font.pixelSize: 10; font.weight: Font.Bold; width: 56; horizontalAlignment: Text.AlignHCenter }
                    Text { text: qsTr("PLAY"); color: "#4e6b78"; font.family: "monospace"; font.pixelSize: 10; font.weight: Font.Bold; width: 34; horizontalAlignment: Text.AlignHCenter }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: "#15232e" }

                // Lista segmentów
                ListView {
                    id: segmentList
                    objectName: "segmentListView"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 3
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: (typeof studio !== "undefined" && studio) ? studio.segments : []

                    delegate: Rectangle {
                        id: segRow
                        required property var modelData
                        required property int index
                        readonly property bool isSelected: studioRoot.selectedIndex === index
                        readonly property bool isRowPlaying: (typeof studio !== "undefined" && studio && studio.isPlaying && isSelected)
                        readonly property bool isCacheHit: (index % 2 === 0) // Placeholder statusu z E1/M7

                        width: segmentList.width
                        implicitHeight: 40
                        radius: 8
                        color: isRowPlaying
                               ? Qt.rgba(studioRoot.accent.r, studioRoot.accent.g, studioRoot.accent.b, 0.14)
                               : (isSelected
                                  ? Qt.rgba(0.0, 0.82, 1.0, 0.10)
                                  : (rowMouse.containsMouse
                                     ? "#131e28"
                                     : (index % 2 === 0 ? "#0a1017" : "#0d151d")))
                        border.width: isSelected || isRowPlaying ? 1 : 0
                        border.color: isRowPlaying
                                      ? studioRoot.accent
                                      : Qt.rgba(0.0, 0.82, 1.0, 0.40)

                        Behavior on color { ColorAnimation { duration: 110 } }

                        // Lewy pasek akcentu aktywnego segmentu
                        Rectangle {
                            visible: segRow.isSelected || segRow.isRowPlaying
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: 3
                            height: 22
                            radius: 2
                            color: segRow.isRowPlaying ? studioRoot.accent : "#00d2ff"
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8

                            Text {
                                text: (index + 1) < 10 ? ("0" + (index + 1)) : (index + 1)
                                color: segRow.isRowPlaying ? studioRoot.accent : "#52707d"
                                font.family: "monospace"
                                font.pixelSize: 11
                                width: 24
                            }

                            Text {
                                text: modelData.text || ""
                                color: segRow.isSelected ? "#ffffff" : "#dcecf5"
                                font.pixelSize: 13
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Text {
                                text: modelData.speed ? (modelData.speed + "x") : "1.0x"
                                color: (modelData.speed && modelData.speed !== 1.0) ? "#00d2ff" : "#7895a3"
                                font.family: "monospace"
                                font.pixelSize: 10
                                width: 48
                                horizontalAlignment: Text.AlignHCenter
                            }

                            Text {
                                text: modelData.dsp && modelData.dsp.length > 0 ? modelData.dsp : "—"
                                color: modelData.dsp && modelData.dsp.length > 0 ? "#b388ff" : "#385260"
                                font.family: "monospace"
                                font.pixelSize: 10
                                width: 58
                                horizontalAlignment: Text.AlignHCenter
                            }

                            Text {
                                text: modelData.pauseMs ? (modelData.pauseMs + "ms") : "—"
                                color: modelData.pauseMs ? "#ffaa33" : "#385260"
                                font.family: "monospace"
                                font.pixelSize: 10
                                width: 48
                                horizontalAlignment: Text.AlignHCenter
                            }

                            // Sprzętowa płytka LED Cache HIT / MISS
                            Rectangle {
                                width: 54
                                height: 20
                                radius: 5
                                color: segRow.isCacheHit
                                       ? Qt.rgba(0.13, 0.90, 0.55, 0.10)
                                       : Qt.rgba(1.0, 0.67, 0.20, 0.08)
                                border.color: segRow.isCacheHit ? "#1b6e4b" : Qt.rgba(1.0, 0.67, 0.20, 0.32)
                                border.width: 1

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 4
                                    Rectangle {
                                        width: 5; height: 5; radius: 3
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: segRow.isCacheHit ? studioRoot.accent : "#ffaa33"
                                    }
                                    Text {
                                        text: segRow.isCacheHit ? "HIT" : "MISS"
                                        color: segRow.isCacheHit ? studioRoot.accent : "#ffaa33"
                                        font.family: "monospace"
                                        font.pixelSize: 9
                                        font.weight: Font.Bold
                                        font.letterSpacing: 0.6
                                    }
                                }
                            }

                            // Mini-Waveform fali mowy segmentu
                            Canvas {
                                visible: studioRoot.width > 680
                                width: 56
                                height: 20
                                property bool activeWave: segRow.isSelected || segRow.isRowPlaying
                                onActiveWaveChanged: requestPaint()
                                onPaint: {
                                    var ctx = getContext("2d");
                                    ctx.reset();
                                    var txt = segRow.modelData.text || "";
                                    var bars = 14;
                                    var step = width / bars;
                                    ctx.fillStyle = activeWave ? studioRoot.accent : "#385766";
                                    for (var i = 0; i < bars; i++) {
                                        var seed = (txt.charCodeAt(i % Math.max(1, txt.length)) || 42) + (i * 17) + (segRow.index * 7);
                                        var amp = 0.25 + ((seed % 75) / 100.0);
                                        var bh = Math.max(3, height * amp);
                                        ctx.fillRect(i * step + 1, (height - bh) * 0.5, step - 2, bh);
                                    }
                                }
                            }

                            // Przycisk natychmiastowego odsłuchu (▶ / ⏸)
                            Rectangle {
                                width: 30; height: 26; radius: 6
                                color: segRow.isRowPlaying
                                       ? Qt.rgba(0.13, 0.90, 0.55, 0.22)
                                       : (playBtnMouse.containsMouse ? "#1a2c38" : "#101a24")
                                border.color: segRow.isRowPlaying ? studioRoot.accent : "#223846"

                                Text {
                                    anchors.centerIn: parent
                                    text: segRow.isRowPlaying ? "⏸" : "▶"
                                    color: segRow.isRowPlaying ? studioRoot.accent : "#dcecf5"
                                    font.pixelSize: 10
                                }

                                MouseArea {
                                    id: playBtnMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        studioRoot.selectedSegment = modelData;
                                        studioRoot.selectedIndex = index;
                                        if (typeof studio !== "undefined" && studio) {
                                            if (studio.isPlaying && studioRoot.selectedIndex === index) {
                                                studio.pausePlayback();
                                            } else {
                                                studio.playSegment(index);
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: rowMouse
                            anchors.fill: parent
                            anchors.rightMargin: 38
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                studioRoot.selectedSegment = modelData;
                                studioRoot.selectedIndex = index;
                                studioRoot.segmentSelected(modelData, index);
                                studioRoot.inspectRequested();
                            }
                        }
                    }

                    // Estetyczny Empty State
                    Column {
                        visible: segmentList.count === 0
                        anchors.centerIn: parent
                        spacing: 6
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "🎙️"
                            font.pixelSize: 22
                            opacity: 0.45
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("READY TO DIRECT")
                            color: "#678896"
                            font.family: "monospace"
                            font.pixelSize: 11
                            font.letterSpacing: 1.5
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("Enter text with directives above or click 'Direct'.")
                            color: "#435e6b"
                            font.pixelSize: 12
                        }
                    }
                }
            }
        }

        // ====================================================================
        // 3. MASTER TRANSPORT DOCK (Profesjonalna konsola taśmy i scrubber)
        // ====================================================================
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 54
            radius: 14
            color: Qt.rgba(.035, .055, .08, studioRoot.glass)
            border.color: (typeof studio !== "undefined" && studio && studio.isPlaying)
                          ? Qt.rgba(studioRoot.accent.r, studioRoot.accent.g, studioRoot.accent.b, 0.45)
                          : "#1a2632"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 12

                // Nawigacja wstecz
                Rectangle {
                    width: 30; height: 30; radius: 7
                    color: prevMouse.containsMouse ? "#162430" : "#0e1720"
                    border.color: "#1e303d"
                    Text { anchors.centerIn: parent; text: "⏮"; color: "#b8d0dc"; font.pixelSize: 11 }
                    MouseArea {
                        id: prevMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof studio !== "undefined" && studio && studio.playbackPosition > 500) {
                                studio.seekPlayback(0);
                            } else if (studioRoot.selectedIndex > 0) {
                                studioRoot.selectedIndex--;
                                if (typeof studio !== "undefined" && studio && studio.isPlaying) {
                                    studio.playSegment(studioRoot.selectedIndex);
                                }
                            }
                        }
                    }
                }

                // Główny przycisk Odtwórz / Pauza
                Rectangle {
                    id: transportPlayBtn
                    objectName: "transportPlayBtn"
                    readonly property bool playing: typeof studio !== "undefined" && studio && studio.isPlaying
                    readonly property bool paused: typeof studio !== "undefined" && studio && studio.isPaused
                    implicitWidth: playLblRow.implicitWidth + 24
                    implicitHeight: 34
                    radius: 8
                    color: playing
                           ? Qt.rgba(0.13, 0.90, 0.55, 0.24)
                           : (playMainMouse.containsMouse ? Qt.rgba(0.13, 0.90, 0.55, 0.20) : Qt.rgba(0.13, 0.90, 0.55, 0.12))
                    border.color: playing ? studioRoot.accent : "#256e52"
                    scale: playMainMouse.pressed ? 0.96 : 1.0
                    Behavior on scale { NumberAnimation { duration: 70 } }

                    RowLayout {
                        id: playLblRow
                        anchors.centerIn: parent
                        spacing: 6
                        Text {
                            text: transportPlayBtn.playing ? "⏸" : "▶"
                            color: studioRoot.accent
                            font.pixelSize: 12
                        }
                        Text {
                            visible: studioRoot.width > 760
                            text: transportPlayBtn.playing
                                  ? qsTr("PAUSE")
                                  : (transportPlayBtn.paused ? qsTr("RESUME") : qsTr("PLAY"))
                            color: "#eff9ff"
                            font.family: "monospace"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                            font.letterSpacing: 0.8
                        }
                    }

                    MouseArea {
                        id: playMainMouse
                        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof studio !== "undefined" && studio) {
                                studio.togglePlayback();
                            }
                        }
                    }
                }

                // Przycisk Stop
                Rectangle {
                    id: stopBtn
                    width: 30; height: 30; radius: 7
                    readonly property bool canStop: (typeof studio !== "undefined" && studio) && (studio.isPlaying || studio.isPaused || studio.isBusy)
                    color: stopMouse.containsMouse && canStop ? "#2b1319" : "#0e1720"
                    border.color: canStop ? "#4d232d" : "#1a2833"
                    opacity: canStop ? 1.0 : 0.45
                    Text { anchors.centerIn: parent; text: "⏹"; color: stopBtn.canStop ? "#ff6b81" : "#6b8896"; font.pixelSize: 11 }
                    MouseArea {
                        id: stopMouse; anchors.fill: parent; hoverEnabled: true; enabled: parent.canStop
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: {
                            if (typeof studio !== "undefined" && studio) {
                                if (studio.isBusy)
                                    studio.cancelRender();
                                studio.stopPlayback();
                            }
                        }
                    }
                }

                // Nawigacja naprzód
                Rectangle {
                    width: 30; height: 30; radius: 7
                    color: nextMouse.containsMouse ? "#162430" : "#0e1720"
                    border.color: "#1e303d"
                    Text { anchors.centerIn: parent; text: "⏭"; color: "#b8d0dc"; font.pixelSize: 11 }
                    MouseArea {
                        id: nextMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            var maxIdx = (typeof studio !== "undefined" && studio) ? studio.segments.length - 1 : 0;
                            if (studioRoot.selectedIndex < maxIdx) {
                                studioRoot.selectedIndex++;
                                if (typeof studio !== "undefined" && studio && studio.isPlaying) {
                                    studio.playSegment(studioRoot.selectedIndex);
                                }
                            }
                        }
                    }
                }

                // Interaktywny Scrubber fali / osi czasu (Waveform Tape Scrubber)
                Rectangle {
                    id: playbackSlider
                    objectName: "playbackSlider"
                    Layout.fillWidth: true
                    height: 30
                    radius: 7
                    color: "#070c11"
                    border.color: "#162430"

                    readonly property real durationMs: (typeof studio !== "undefined" && studio) ? studio.playbackDuration : 0
                    readonly property real positionMs: (typeof studio !== "undefined" && studio) ? studio.playbackPosition : 0
                    readonly property real progressRatio: durationMs > 0 ? Math.max(0, Math.min(1, positionMs / durationMs))
                                                        : ((typeof studio !== "undefined" && studio && studio.isBusy) ? Math.max(0, Math.min(1, studio.taskPercent / 100.0)) : 0)

                    onProgressRatioChanged: scrubberCanvas.requestPaint()
                    onDurationMsChanged: scrubberCanvas.requestPaint()

                    Canvas {
                        id: scrubberCanvas
                        anchors.fill: parent
                        anchors.margins: 4
                        onWidthChanged: requestPaint()
                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.reset();
                            var bars = Math.max(24, Math.floor(width / 4));
                            var step = width / bars;
                            var splitX = width * playbackSlider.progressRatio;
                            var isBusy = (typeof studio !== "undefined" && studio && studio.isBusy);

                            for (var i = 0; i < bars; i++) {
                                var x = i * step;
                                var env = 0.25 + 0.65 * Math.abs(Math.sin(i * 0.42) * Math.cos(i * 0.17));
                                var bh = Math.max(2, height * env);
                                var y = (height - bh) * 0.5;
                                if (x <= splitX) {
                                    ctx.fillStyle = isBusy ? "#00d2ff" : studioRoot.accent;
                                } else {
                                    ctx.fillStyle = "#1d313d";
                                }
                                ctx.fillRect(x, y, Math.max(1.5, step - 1.5), bh);
                            }

                            // Igła playhead
                            if (playbackSlider.progressRatio > 0) {
                                ctx.fillStyle = "#ffffff";
                                ctx.fillRect(Math.min(width - 2, splitX), 0, 2, height);
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: playbackSlider.durationMs > 0
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onPressed: function(mouse) {
                            var ratio = Math.max(0, Math.min(1, mouse.x / width));
                            studio.seekPlayback(Math.round(ratio * playbackSlider.durationMs));
                        }
                        onPositionChanged: function(mouse) {
                            if (pressed) {
                                var ratio = Math.max(0, Math.min(1, mouse.x / width));
                                studio.seekPlayback(Math.round(ratio * playbackSlider.durationMs));
                            }
                        }
                    }
                }

                // Zegar SMPTE z milisekundami
                Text {
                    id: playbackTimeText
                    objectName: "playbackTimeText"
                    text: {
                        if (typeof studio !== "undefined" && studio && studio.isBusy) {
                            return qsTr("SYNTHESIS %1%").arg(Math.round(studio.taskPercent));
                        }
                        if (typeof studio !== "undefined" && studio && studio.playbackDuration > 0) {
                            return studioRoot.formatTimeMs(studio.playbackPosition) + " / " + studioRoot.formatTimeMs(studio.playbackDuration);
                        }
                        return qsTr("00:00.00 / 00:00.00");
                    }
                    color: (typeof studio !== "undefined" && studio && studio.isPlaying)
                           ? studioRoot.accent
                           : ((typeof studio !== "undefined" && studio && studio.isBusy) ? "#00d2ff" : "#8ca8b3")
                    font.family: "monospace"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
            }
        }
    }
}
