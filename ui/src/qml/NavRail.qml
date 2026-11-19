// NavRail.qml — Left vertical navigation rail for Audioticon 2.0 (ASTRA Glass)
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: railRoot
    objectName: "navRail"
    property string activeModule: "studio"
    property color accent: "#22e58b"
    property real glass: 0.72
    property int windowWidth: 1180
    property bool collapsed: width < 120

    signal moduleSelected(string moduleId)

    implicitWidth: windowWidth >= 1180 ? 176 : 56
    width: implicitWidth
    Layout.preferredWidth: implicitWidth
    color: Qt.rgba(.035, .055, .078, glass)
    border.color: "#1a2632"
    border.width: 1
    radius: 14

    readonly property var modules: [
        { id: "studio",   icon: "🎙️", label: qsTr("Studio"),     key: "Ctrl+1" },
        { id: "agent",    icon: "💬", label: qsTr("AI Chat"),    key: "Ctrl+2" },
        { id: "voicelab", icon: "👥", label: qsTr("VoiceLab"),   key: "Ctrl+3" },
        { id: "projects", icon: "📁", label: qsTr("Projects"),   key: "Ctrl+4" },
        { id: "sfx",      icon: "🔊", label: qsTr("Foley/SFX"),  key: "Ctrl+5" },
        { id: "ledger",   icon: "📊", label: qsTr("ACX Ledger"), key: "Ctrl+6" },
        { id: "doctor",   icon: "🩺", label: qsTr("Doctor"),     key: "Ctrl+7" },
        { id: "models",   icon: "📦", label: qsTr("Models"),     key: "Ctrl+8" }
    ]

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 16
        anchors.bottomMargin: 16
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        spacing: 6

        // Section header
        Text {
            visible: !railRoot.collapsed
            text: qsTr("MODULES")
            color: "#52737e"
            font.family: "monospace"
            font.pixelSize: 9
            font.letterSpacing: 1.4
            Layout.leftMargin: 8
            Layout.bottomMargin: 4
        }

        Repeater {
            model: railRoot.modules
            Rectangle {
                required property var modelData
                required property int index
                Layout.fillWidth: true
                implicitHeight: 42
                radius: 10
                color: railRoot.activeModule === modelData.id
                       ? Qt.rgba(railRoot.accent.r, railRoot.accent.g, railRoot.accent.b, .14)
                       : itemMouse.containsMouse ? Qt.rgba(1, 1, 1, .05) : "transparent"
                border.width: railRoot.activeModule === modelData.id ? 1 : 0
                border.color: Qt.rgba(railRoot.accent.r, railRoot.accent.g, railRoot.accent.b, .45)

                // Active accent indicator
                Rectangle {
                    visible: railRoot.activeModule === modelData.id
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: 3
                    height: 20
                    radius: 2
                    color: railRoot.accent
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: railRoot.collapsed ? 0 : 12
                    anchors.rightMargin: railRoot.collapsed ? 0 : 8
                    spacing: 10

                    Text {
                        text: modelData.icon
                        font.pixelSize: 16
                        Layout.alignment: railRoot.collapsed ? Qt.AlignHCenter : Qt.AlignVCenter
                        Layout.fillWidth: railRoot.collapsed
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        visible: !railRoot.collapsed
                        text: modelData.label
                        color: railRoot.activeModule === modelData.id ? "#eff9ff" : "#9bb8c2"
                        font.pixelSize: 12
                        font.weight: railRoot.activeModule === modelData.id ? Font.DemiBold : Font.Normal
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Text {
                        visible: !railRoot.collapsed
                        text: modelData.key
                        color: "#466570"
                        font.family: "monospace"
                        font.pixelSize: 9
                    }
                }

                MouseArea {
                    id: itemMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        railRoot.activeModule = modelData.id;
                        railRoot.moduleSelected(modelData.id);
                    }
                }

                ToolTip.visible: railRoot.collapsed && itemMouse.containsMouse
                ToolTip.text: modelData.label + " (" + modelData.key + ")"
                ToolTip.delay: 300
            }
        }

        Item { Layout.fillHeight: true }

        // Engine & environment status footer
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: railRoot.collapsed ? 36 : 48
            radius: 10
            color: Qt.rgba(.01, .025, .035, Math.max(0.45, railRoot.glass * 0.85))
            border.color: "#1e3a43"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 8
                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: {
                        if (typeof studio !== "undefined" && studio && studio.isBusy) return "#00d2ff";
                        if (typeof activeDoctor !== "undefined" && activeDoctor) {
                            if (activeDoctor.inspectionMode) return "#ffd43b";
                            if (!activeDoctor.isEnvironmentReady) return "#ff4d6d";
                        }
                        return railRoot.accent;
                    }
                    Layout.alignment: railRoot.collapsed ? Qt.AlignHCenter : Qt.AlignVCenter
                    Layout.fillWidth: railRoot.collapsed
                }
                ColumnLayout {
                    visible: !railRoot.collapsed
                    spacing: 0
                    Layout.fillWidth: true
                    Text {
                        text: {
                            if (typeof studio !== "undefined" && studio && studio.isBusy) return qsTr("PROCESSING");
                            if (typeof activeDoctor !== "undefined" && activeDoctor) {
                                if (activeDoctor.inspectionMode) return qsTr("NO TTS");
                                if (!activeDoctor.isEnvironmentReady) return qsTr("REPAIR");
                            }
                            return qsTr("ENV READY");
                        }
                        color: {
                            if (typeof studio !== "undefined" && studio && studio.isBusy) return "#00d2ff";
                            if (typeof activeDoctor !== "undefined" && activeDoctor) {
                                if (activeDoctor.inspectionMode) return "#ffd43b";
                                if (!activeDoctor.isEnvironmentReady) return "#ff4d6d";
                            }
                            return railRoot.accent;
                        }
                        font.family: "monospace"
                        font.pixelSize: 8
                        font.letterSpacing: 1.1
                    }
                    Text {
                        text: {
                            if (typeof studio !== "undefined" && studio && studio.isBusy) return studio.taskStatus;
                            if (typeof activeDoctor !== "undefined" && activeDoctor) {
                                if (activeDoctor.inspectionMode) return qsTr("Director mode");
                                if (!activeDoctor.isEnvironmentReady) return qsTr("Check Doctor");
                            }
                            return qsTr("Local runtime");
                        }
                        color: "#6c8c97"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }
}
