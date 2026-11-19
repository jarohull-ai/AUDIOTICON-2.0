// One row of the transcript.
//
// `message` is an AgentModel message map:
//   user  { role: "user",  text, time }
//   agent { role: "agent", text, time, streaming }
//   tool  { role: "tool",  tool, status: running|done|failed, ms, time }
//   note  { role: "note",  text, time }
//
// Sizing is deliberately one-directional: the Column inside each bubble owns
// the text height, the bubble follows it, the row follows the bubble. Anchoring
// the layout to the bubble instead would make the two depend on each other and
// collapse the delegate to zero height.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: rowRoot
    required property var message
    property color accent: "#31f7a5"
    property real glass: .83
    signal readInStudioRequested(string text)
    // Follow the viewport through the parent (the list's content item). The
    // attached ListView.view is null while the delegate is being built, and a
    // delegate whose width resolves to 0 draws nothing at all.
    width: parent ? parent.width : 0

    readonly property string role: message.role || "note"
    readonly property bool isUser: role === "user"
    readonly property bool isTool: role === "tool"
    readonly property bool isNote: role === "note"

    readonly property real naturalHeight: isTool ? 40
                                        : isNote ? noteText.contentHeight + 12
                                        : bubble.height + 8
    implicitHeight: naturalHeight
    height: naturalHeight

    // ---- tool call: a compact, monospace trace line ------------------------
    Rectangle {
        visible: rowRoot.isTool
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.top: parent.top
        anchors.topMargin: 4
        width: Math.min(rowRoot.width - 12, toolRow.implicitWidth + 28)
        height: 30
        radius: 10
        color: Qt.rgba(.035, .065, .085, rowRoot.glass)
        border.color: rowRoot.message.status === "failed" ? "#7a3a3a" : "#25414a"

        RowLayout {
            id: toolRow
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 8
            Text {
                text: "\u25B8"
                color: rowRoot.message.status === "running" ? rowRoot.accent : "#5c7c88"
                font.family: "monospace"; font.pixelSize: 11
            }
            Text {
                text: rowRoot.message.tool || ""
                color: "#b4ced5"; font.family: "monospace"; font.pixelSize: 11
            }
            Text {
                text: rowRoot.message.status === "running" ? qsTr("running\u2026")
                    : rowRoot.message.status === "failed" ? qsTr("failed") : qsTr("done")
                color: rowRoot.message.status === "running" ? rowRoot.accent
                     : rowRoot.message.status === "failed" ? "#ff8f8f" : "#7f9aa4"
                font.family: "monospace"; font.pixelSize: 10
            }
            Item { Layout.fillWidth: true }
            Text {
                text: rowRoot.message.status === "running"
                      ? (rowRoot.message.ms || 0) + " ms"
                      : (rowRoot.message.detail || "")
                color: "#5c7c88"; font.family: "monospace"; font.pixelSize: 10
            }
        }
    }

    // ---- note: a quiet aside ----------------------------------------------
    Text {
        id: noteText
        visible: rowRoot.isNote
        anchors.top: parent.top
        anchors.topMargin: 4
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(rowRoot.width - 60, 520)
        text: rowRoot.message.text || ""
        color: "#6f8f99"; font.pixelSize: 11; font.italic: true
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }

    // ---- user and agent bubbles -------------------------------------------
    Rectangle {
        id: bubble
        visible: rowRoot.isUser || rowRoot.role === "agent"
        anchors.top: parent.top
        anchors.topMargin: 2
        anchors.right: rowRoot.isUser ? parent.right : undefined
        anchors.left: rowRoot.isUser ? undefined : parent.left
        anchors.leftMargin: rowRoot.isUser ? 0 : 6
        anchors.rightMargin: rowRoot.isUser ? 6 : 0
        // A fraction of the row, never the content's own width: tying the bubble
        // to content.implicitWidth while the column is sized from the bubble is
        // a circular binding that resolves to zero.
        width: rowRoot.width * (rowRoot.isUser ? .66 : .86)
        height: content.implicitHeight + 26
        radius: 16
        color: rowRoot.isUser ? Qt.rgba(.035, .065, .085, rowRoot.glass)
                              : Qt.rgba(.02, .045, .06, rowRoot.glass)
        border.width: 1
        border.color: rowRoot.isUser ? Qt.rgba(rowRoot.accent.r, rowRoot.accent.g, rowRoot.accent.b, .28)
                                     : Qt.rgba(.8, 1, 1, .10)

        Column {
            id: content
            x: 14
            y: 12
            width: bubble.width - 28
            spacing: 6

            Row {
                width: parent.width
                spacing: 8
                Rectangle {
                    visible: !rowRoot.isUser
                    width: 18; height: 18; radius: 6
                    color: "#163136"; border.color: rowRoot.accent
                    Text {
                        anchors.centerIn: parent; text: "\u039B"
                        color: rowRoot.accent; font.pixelSize: 11
                    }
                }
                Text {
                    text: rowRoot.isUser ? qsTr("YOU") : qsTr("AGENT")
                    color: rowRoot.isUser ? "#8ca6b0" : rowRoot.accent
                    font.pixelSize: 9; font.letterSpacing: 1.4
                    anchors.verticalCenter: parent.verticalCenter
                }
                Item {
                    width: parent.width - (rowRoot.isUser ? 70 : 88)
                    height: 1
                }
                Text {
                    text: rowRoot.message.time || ""
                    color: "#52737e"; font.family: "monospace"; font.pixelSize: 9
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Text {
                width: parent.width
                text: rowRoot.message.text || ""
                color: "#e7f3f8"; font.pixelSize: 13
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
            }

            // Przycisk akcji M6: Przeczytaj w studiu
            Button {
                id: readInStudioBtn
                objectName: "readInStudioBtn"
                visible: !rowRoot.isUser && !(rowRoot.message.streaming === true) && (rowRoot.message.text || "").length > 0
                text: qsTr("🎙️ Read in Studio")
                implicitHeight: 26
                font.pixelSize: 11
                onClicked: {
                    if (typeof studio !== "undefined" && studio) {
                        studio.parseDirectedText(rowRoot.message.text);
                    }
                    if (typeof win !== "undefined" && win) {
                        win.activeModule = "studio";
                    }
                    rowRoot.readInStudioRequested(rowRoot.message.text || "");
                }
            }

            Text {
                visible: rowRoot.message.streaming === true
                text: "\u258D"
                color: rowRoot.accent; font.pixelSize: 12
                SequentialAnimation on opacity {
                    running: rowRoot.message.streaming === true
                    loops: Animation.Infinite
                    NumberAnimation { to: 0; duration: 480 }
                    NumberAnimation { to: 1; duration: 480 }
                }
            }
        }
    }
}
