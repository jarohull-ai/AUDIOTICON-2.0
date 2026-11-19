// MasteringStrip.qml — Kompaktowy pasek telemetrii audio i masteringu EBU R128 / ACX (M9)
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: stripRoot
    objectName: "masteringStrip"
    property color accent: "#22e58b"
    property real glass: .88
    property bool isPlaying: (typeof studio !== "undefined" && studio) ? studio.isPlaying : false
    property bool isBusy: (typeof studio !== "undefined" && studio) ? studio.isBusy : false
    property int segmentCount: (typeof studio !== "undefined" && studio && studio.segments) ? studio.segments.length : 0
    property real meterPhase: 0.0

    implicitHeight: 38
    radius: 12
    color: Qt.rgba(.04, .065, .09, glass)
    border.color: "#1a2632"
    border.width: 1

    Timer {
        interval: 60
        running: stripRoot.isPlaying || stripRoot.isBusy
        repeat: true
        onTriggered: {
            stripRoot.meterPhase = (stripRoot.meterPhase + 0.35) % (Math.PI * 2);
        }
        onRunningChanged: {
            if (!running) stripRoot.meterPhase = 0.0;
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 14

        // 1. Status silnika Piper / Rust
        RowLayout {
            spacing: 7
            Rectangle {
                width: 7; height: 7; radius: 4
                color: stripRoot.isBusy ? "#00d2ff" : stripRoot.accent
                SequentialAnimation on opacity {
                    running: stripRoot.isBusy || stripRoot.isPlaying
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.35; duration: 420; easing.type: Easing.InOutQuad }
                    NumberAnimation { to: 1.0; duration: 420; easing.type: Easing.InOutQuad }
                }
            }
            Text {
                text: stripRoot.isBusy
                      ? qsTr("PIPER ONNX · SYNTHESIZING")
                      : (stripRoot.isPlaying ? qsTr("QAudioSink · PLAYING") : qsTr("PIPER ONNX · 44.1 kHz / 16-bit"))
                color: stripRoot.isBusy ? "#00d2ff" : "#b8d0dc"
                font.family: "monospace"
                font.pixelSize: 10
                font.weight: Font.DemiBold
            }
        }

        Rectangle { width: 1; height: 16; color: "#1a2632" }

        // 2. Cache Telemetry
        Text {
            visible: stripRoot.width > 740
            text: qsTr("SEGMENTS: %1  ·  CACHE RAM").arg(stripRoot.segmentCount)
            color: "#6b8896"
            font.family: "monospace"
            font.pixelSize: 10
        }

        Item { Layout.fillWidth: true }

        // 3. EBU R128 Loudness & True Peak
        RowLayout {
            spacing: 10
            visible: stripRoot.width > 820

            RowLayout {
                spacing: 4
                Text { text: "LUFS"; color: "#52707d"; font.family: "monospace"; font.pixelSize: 9; font.letterSpacing: 1 }
                Text {
                    text: stripRoot.isPlaying ? "-19.2" : "-20.0"
                    color: stripRoot.accent
                    font.family: "monospace"
                    font.pixelSize: 10
                    font.weight: Font.Bold
                }
            }

            RowLayout {
                spacing: 4
                Text { text: "PEAK"; color: "#52707d"; font.family: "monospace"; font.pixelSize: 9; font.letterSpacing: 1 }
                Text {
                    text: stripRoot.isPlaying ? "-1.4 dBFS" : "-62.0 dBFS"
                    color: stripRoot.isPlaying ? "#00d2ff" : "#7895a3"
                    font.family: "monospace"
                    font.pixelSize: 10
                }
            }
        }

        // 4. Segmentowy miernik LED VU (16 diod)
        Row {
            spacing: 2
            Layout.alignment: Qt.AlignVCenter

            Repeater {
                model: 16
                Rectangle {
                    required property int index
                    width: 4
                    height: 12
                    radius: 1.5

                    readonly property real level: {
                        if (stripRoot.isPlaying) {
                            var wave = 0.55 + 0.35 * Math.sin(stripRoot.meterPhase + index * 0.45);
                            return wave;
                        }
                        if (stripRoot.isBusy) {
                            return 0.25 + 0.25 * Math.sin(stripRoot.meterPhase * 1.4);
                        }
                        return 0.08;
                    }
                    readonly property bool lit: (index / 16.0) < level
                    readonly property color ledColor: index >= 14 ? "#ff3b56"
                                                    : (index >= 11 ? "#ffaa33" : stripRoot.accent)

                    color: lit ? ledColor : "#121c24"
                    opacity: lit ? 0.95 : 0.7
                    Behavior on color { ColorAnimation { duration: 70 } }
                }
            }
        }

        // 5. Plakietka ACX / EBU R128
        Rectangle {
            implicitWidth: acxLabel.implicitWidth + 14
            implicitHeight: 20
            radius: 5
            color: Qt.rgba(0.13, 0.90, 0.55, 0.10)
            border.color: "#1b6e4b"
            border.width: 1
            visible: stripRoot.width > 640

            Text {
                id: acxLabel
                anchors.centerIn: parent
                text: "ACX READY"
                color: stripRoot.accent
                font.family: "monospace"
                font.pixelSize: 9
                font.weight: Font.Bold
                font.letterSpacing: 0.8
            }
        }
    }
}
