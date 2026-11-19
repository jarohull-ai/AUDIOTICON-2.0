// One glass telemetry tile: caption, big reading, detail line and a sparkline
// that scales to `scaleMax`.
//
// `metric` is a DashboardModel card:
//   { id, label, value, unit, detail, level, available, color, history }
// `color` inside the card overrides `accent` for the sparkline and its top bar,
// so several metrics stacked together stay distinguishable.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: tile

    property var metric: ({})
    property color accent: "#31f7a5"
    property bool editing: false
    property real glass: .83
    property int serial: 0
    // Chart axis. 100 for percentages, or a metric-specific maximum.
    property real scaleMax: 100
    property color series: metric.color !== undefined && metric.color !== ""
                            ? metric.color : accent

    signal removeRequested()
    signal moveRequested(int direction)

    radius: 18
    color: Qt.rgba(.035, .065, .085, glass)
    border.color: Qt.rgba(accent.r, accent.g, accent.b, .20)
    border.width: 1
    clip: true

    Rectangle {
        anchors.fill: parent; radius: parent.radius
        gradient: Gradient {
            GradientStop { position: 0; color: "#15bce4ed" }
            GradientStop { position: .48; color: "#027cabb5" }
            GradientStop { position: 1; color: "#0a000000" }
        }
    }
    Rectangle { x: 22; y: 0; width: parent.width - 44; height: 1; color: Qt.rgba(.8, 1, 1, .25) }
    Rectangle { x: 22; y: 0; width: 42; height: 2; color: tile.series }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: tile.height < 210 ? 16 : 22
        spacing: tile.height < 210 ? 3 : 5

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: ("0" + (tile.serial + 1)).slice(-2)
                color: tile.accent; font.family: "monospace"; font.pixelSize: 11
            }
            Text {
                text: tile.metric.label || ""
                color: "#b4ced5"; font.pixelSize: 13; font.letterSpacing: 1
                Layout.fillWidth: true; textFormat: Text.PlainText
            }
            Rectangle {
                width: 5; height: 5; radius: 3
                color: tile.metric.available ? tile.accent : "#6b7683"
                visible: !tile.editing
            }
            ToolButton {
                text: "\u2039"; visible: tile.editing; implicitWidth: 28; implicitHeight: 28
                onClicked: tile.moveRequested(-1)
                ToolTip.text: qsTr("Move earlier"); ToolTip.visible: hovered
            }
            ToolButton {
                text: "\u203A"; visible: tile.editing; implicitWidth: 28; implicitHeight: 28
                onClicked: tile.moveRequested(1)
                ToolTip.text: qsTr("Move later"); ToolTip.visible: hovered
            }
            ToolButton {
                text: "\u00D7"; visible: tile.editing; implicitWidth: 28; implicitHeight: 28
                onClicked: tile.removeRequested()
                ToolTip.text: qsTr("Remove tile"); ToolTip.visible: hovered
            }
        }

        RowLayout {
            spacing: 8
            Layout.topMargin: tile.height < 210 ? 0 : 8
            Text {
                text: tile.metric.value || "\u2014"
                font.pixelSize: tile.height < 210 ? 34 : tile.height < 250 ? 42 : 50
                font.weight: Font.Light; font.letterSpacing: -2; color: "#effaff"
            }
            Text {
                text: tile.metric.unit || ""
                font.pixelSize: 14; color: tile.accent
                Layout.alignment: Qt.AlignBottom
                Layout.bottomMargin: tile.height < 210 ? 8 : 12
            }
            Item { Layout.fillWidth: true }
        }

        Text {
            text: tile.metric.detail || ""
            font.pixelSize: 11; color: "#8ca6b0"; Layout.fillWidth: true
            elide: Text.ElideRight; textFormat: Text.PlainText
            ToolTip.text: text; ToolTip.visible: detailHover.hovered
            HoverHandler { id: detailHover }
        }

        Item { Layout.fillHeight: true; Layout.minimumHeight: tile.height < 210 ? 2 : 8 }

        Canvas {
            id: graph
            Layout.fillWidth: true
            Layout.preferredHeight: tile.height < 210 ? 32 : tile.height < 250 ? 45 : 67
            property var samples: tile.metric.history || []
            onSamplesChanged: requestPaint()
            Connections { target: tile; function onSeriesChanged() { graph.requestPaint() } }
            onWidthChanged: requestPaint()
            onPaint: {
                var c = getContext("2d"); c.reset();
                c.strokeStyle = "#143641"; c.lineWidth = 1;
                for (var j = 0; j < 4; j++) {
                    var gy = 4 + j * (height - 8) / 3;
                    c.beginPath(); c.moveTo(0, gy); c.lineTo(width, gy); c.stroke();
                }
                for (var k = 0; k < 7; k++) {
                    var gx = k * width / 6;
                    c.beginPath(); c.moveTo(gx, 0); c.lineTo(gx, height); c.stroke();
                }
                if (samples.length < 2) return;
                var axis = Math.max(.0001, tile.scaleMax);
                var step = width / 89;
                var first = width - (samples.length - 1) * step;
                function yy(v) { return height - 4 - Math.min(1, Math.max(0, v / axis)) * (height - 8); }
                c.beginPath(); c.moveTo(first, height);
                for (var i = 0; i < samples.length; i++) c.lineTo(first + i * step, yy(samples[i]));
                c.lineTo(width, height); c.closePath();
                var grad = c.createLinearGradient(0, 0, 0, height);
                grad.addColorStop(0, Qt.rgba(tile.series.r, tile.series.g, tile.series.b, .28));
                grad.addColorStop(1, Qt.rgba(tile.series.r, tile.series.g, tile.series.b, 0));
                c.fillStyle = grad; c.fill();
                c.beginPath();
                for (var n = 0; n < samples.length; n++) {
                    var px = first + n * step, py = yy(samples[n]);
                    if (n === 0) c.moveTo(px, py); else c.lineTo(px, py);
                }
                c.strokeStyle = tile.series; c.lineWidth = 1.8; c.stroke();
                c.beginPath();
                c.arc(width - 2, yy(samples[samples.length - 1]), 3, 0, Math.PI * 2);
                c.fillStyle = "#eafffa"; c.fill();
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("HISTORY / 90 s"); font.family: "monospace"
                font.pixelSize: 9; font.letterSpacing: 1; color: "#52737e"
            }
            Item { Layout.fillWidth: true }
            Text {
                text: tile.metric.available ? qsTr("LIVE") : qsTr("NO DATA")
                font.family: "monospace"; font.pixelSize: 9
                color: tile.metric.available ? tile.accent : "#87969b"
            }
        }
    }
}
