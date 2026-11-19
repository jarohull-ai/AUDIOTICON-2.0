// The bottom strip: one wide glass card that summarises a subsystem.
//
// It is data-driven, so any project can fill it. Assign a map to `s`:
//
//   {
//     ready: true,                        // false shows "measuring…" placeholders
//     glyph: "Π", title: "JFP PARADOX",   // left column identity
//     subtitle: "Network governance",     // small line under the title
//     score: 96, scoreMax: 100, scoreSuffix: "/100",
//     state: "healthy",                   // healthy | degraded | critical
//     stateLabel: "HEALTHY",
//     statusDetail: "All thresholds within limits",
//     metrics: [{k: "LATENCY", v: "16.2", u: "ms"}, ...],   // up to 4 fit cleanly
//     note: "Supervisor: no recommendation",
//     noteTone: "idle",                   // idle | warning | danger
//     series: [100, 98, 100],             // primary line, 0..seriesMax
//     seriesMax: 100, seriesColor: "#6be38b", seriesLabel: "HEALTH",
//     overlay: [16.2, 15.4],              // optional second line, own scale
//     overlayCeiling: 50, overlayColor: "#4fc3ff", overlayLabel: "LATENCY",
//     window: "7.5 min",
//     footer: [{k: "PROOF #12", tip: "SHA-256 chain: ..."}]
//   }
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: panel

    property var s: ({})
    property color accent: "#31f7a5"
    property real glass: .83

    readonly property bool ready: s.ready === true
    readonly property color healthColor: s.seriesColor !== undefined ? s.seriesColor : "#6be38b"
    readonly property color overlayColor2: s.overlayColor !== undefined ? s.overlayColor : "#4fc3ff"
    readonly property real overlayCeiling: {
        if (s.overlayCeiling !== undefined) return s.overlayCeiling;
        var l = s.overlay || [], m = 0;
        for (var i = 0; i < l.length; i++) m = Math.max(m, l[i]);
        return Math.max(1, Math.ceil(m * 1.2 / 10) * 10);
    }
    readonly property color stateColor: !ready ? "#6b7683"
                                     : s.state === "critical" ? "#ff6b6b"
                                     : s.state === "degraded" ? "#ffbb66"
                                     : accent
    readonly property color noteColor: s.noteTone === "danger" ? "#ff9b8a"
                                     : s.noteTone === "warning" ? "#ffcf8a"
                                     : "#6f8f99"

    radius: 18
    color: Qt.rgba(.035, .065, .085, glass)
    border.color: Qt.rgba(accent.r, accent.g, accent.b, .20)
    border.width: 1
    clip: true

    function fmt(v, p) {
        return v === undefined ? "\u2014" : Number(v).toLocaleString(Qt.locale(), 'f', p);
    }

    Rectangle {
        anchors.fill: parent; radius: parent.radius
        gradient: Gradient {
            GradientStop { position: 0; color: "#15bce4ed" }
            GradientStop { position: .48; color: "#027cabb5" }
            GradientStop { position: 1; color: "#0a000000" }
        }
    }
    Rectangle { x: 22; y: 0; width: parent.width - 44; height: 1; color: Qt.rgba(.8, 1, 1, .25) }
    Rectangle { x: 22; y: 0; width: 42; height: 2; color: panel.stateColor }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 22; anchors.rightMargin: 22
        anchors.topMargin: 18; anchors.bottomMargin: 16
        spacing: 22

        ColumnLayout {
            spacing: 2
            Layout.preferredWidth: 190; Layout.fillHeight: true
            RowLayout {
                spacing: 8
                Text { text: panel.s.glyph || "\u0394"; color: panel.accent; font.family: "monospace"; font.pixelSize: 12 }
                Text { text: panel.s.title || ""; color: "#b4ced5"; font.pixelSize: 13; font.letterSpacing: 1 }
                Rectangle { width: 5; height: 5; radius: 3; color: panel.stateColor }
            }
            Text {
                text: panel.s.subtitle || ""; color: "#7898a5"
                font.pixelSize: 10; font.letterSpacing: .6
            }
            Item { Layout.fillHeight: true }
            RowLayout {
                spacing: 6
                Text {
                    text: panel.ready ? panel.fmt(panel.s.score, 0) : "\u2014"
                    font.pixelSize: 40; font.weight: Font.Light; font.letterSpacing: -2; color: "#effaff"
                }
                Text {
                    text: panel.s.scoreSuffix || ""
                    font.pixelSize: 13; color: panel.accent
                    Layout.alignment: Qt.AlignBottom; Layout.bottomMargin: 8
                }
            }
        }

        ColumnLayout {
            spacing: 7; Layout.fillWidth: true; Layout.fillHeight: true
            RowLayout {
                spacing: 10
                Rectangle {
                    implicitWidth: stateText.implicitWidth + 22; implicitHeight: 22; radius: 11
                    color: Qt.rgba(panel.stateColor.r, panel.stateColor.g, panel.stateColor.b, .12)
                    border.color: Qt.rgba(panel.stateColor.r, panel.stateColor.g, panel.stateColor.b, .45)
                    Text {
                        id: stateText
                        anchors.centerIn: parent; color: panel.stateColor
                        font.pixelSize: 10; font.letterSpacing: 1
                        text: !panel.ready ? qsTr("MEASURING\u2026") : (panel.s.stateLabel || "")
                    }
                }
                Text {
                    Layout.fillWidth: true; elide: Text.ElideRight
                    color: "#8ca6b0"; font.pixelSize: 11; textFormat: Text.PlainText
                    text: panel.s.statusDetail || ""
                }
            }
            GridLayout {
                columns: 4; columnSpacing: 22; rowSpacing: 0; Layout.fillWidth: true
                Repeater {
                    model: panel.s.metrics || []
                    ColumnLayout {
                        required property var modelData
                        spacing: 0
                        Text {
                            text: modelData.k; font.family: "monospace"
                            font.pixelSize: 9; font.letterSpacing: 1; color: "#52737e"
                        }
                        RowLayout {
                            spacing: 3
                            Text {
                                text: panel.ready ? modelData.v : "\u2014"
                                font.pixelSize: 20; font.weight: Font.Light; color: "#effaff"
                            }
                            Text {
                                text: panel.ready ? (modelData.u || "") : ""
                                font.pixelSize: 10; color: panel.accent
                                Layout.alignment: Qt.AlignBottom; Layout.bottomMargin: 3
                            }
                        }
                    }
                }
            }
            Text {
                Layout.fillWidth: true; elide: Text.ElideRight
                font.pixelSize: 11; textFormat: Text.PlainText
                color: panel.noteColor
                text: panel.ready ? (panel.s.note || "") : ""
            }
        }

        ColumnLayout {
            spacing: 4
            Layout.preferredWidth: Math.min(340, panel.width * .3)
            Layout.fillHeight: true
            visible: panel.width > 760
            Canvas {
                id: graph
                Layout.fillWidth: true; Layout.fillHeight: true
                property var samples: panel.s.series || []
                property var overlay: panel.s.overlay || []
                onSamplesChanged: requestPaint()
                onOverlayChanged: requestPaint()
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPaint: {
                    var c = getContext("2d"); c.reset();
                    c.strokeStyle = "#143641"; c.lineWidth = 1;
                    for (var j = 0; j < 3; j++) {
                        var gy = 4 + j * (height - 8) / 2;
                        c.beginPath(); c.moveTo(0, gy); c.lineTo(width, gy); c.stroke();
                    }
                    for (var k = 0; k < 7; k++) {
                        var gx = k * width / 6;
                        c.beginPath(); c.moveTo(gx, 0); c.lineTo(gx, height); c.stroke();
                    }
                    var w = width, h = height;
                    function line(data, ceiling, col, fill) {
                        if (data.length < 2) return;
                        var step = w / 89, first = w - (data.length - 1) * step;
                        function yy(v) { return h - 4 - Math.min(1, Math.max(0, v / ceiling)) * (h - 8); }
                        if (fill) {
                            c.beginPath(); c.moveTo(first, h);
                            for (var i = 0; i < data.length; i++) c.lineTo(first + i * step, yy(data[i]));
                            c.lineTo(w, h); c.closePath();
                            var g = c.createLinearGradient(0, 0, 0, h);
                            g.addColorStop(0, Qt.rgba(col.r, col.g, col.b, .25));
                            g.addColorStop(1, Qt.rgba(col.r, col.g, col.b, 0));
                            c.fillStyle = g; c.fill();
                        }
                        c.beginPath();
                        for (var n = 0; n < data.length; n++) {
                            var px = first + n * step, py = yy(data[n]);
                            if (n === 0) c.moveTo(px, py); else c.lineTo(px, py);
                        }
                        c.strokeStyle = col; c.lineWidth = 1.8; c.stroke();
                        c.beginPath(); c.arc(w - 2, yy(data[data.length - 1]), 3, 0, Math.PI * 2);
                        c.fillStyle = col; c.fill();
                    }
                    line(overlay, panel.overlayCeiling, panel.overlayColor2, false);
                    line(samples, panel.s.seriesMax || 100, panel.healthColor, true);
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 10
                Text {
                    text: "\u25CF " + (panel.s.seriesLabel || qsTr("SERIES"))
                    font.family: "monospace"; font.pixelSize: 9; font.letterSpacing: 1
                    color: panel.healthColor
                }
                Text {
                    visible: (panel.s.overlay || []).length > 0
                    text: "\u25CF " + (panel.s.overlayLabel || "") + " \u2264 " + Math.round(panel.overlayCeiling) + " " + (panel.s.overlayUnit || "")
                    font.family: "monospace"; font.pixelSize: 9; font.letterSpacing: 1
                    color: panel.overlayColor2
                }
                Text {
                    text: "/ " + (panel.s.window || ""); font.family: "monospace"
                    font.pixelSize: 9; color: "#52737e"
                }
                Item { Layout.fillWidth: true }
                Repeater {
                    model: panel.s.footer || []
                    Text {
                        required property var modelData
                        text: modelData.k; font.family: "monospace"
                        font.pixelSize: 9; color: panel.accent
                        ToolTip.text: modelData.tip || ""; ToolTip.visible: footerHover.hovered
                        HoverHandler { id: footerHover }
                    }
                }
            }
        }
    }
}
