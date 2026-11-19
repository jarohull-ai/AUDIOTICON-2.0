// ModelCard.qml — Director Workstation Glass card for local & curated models (M19)
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: cardRoot
    objectName: (isCurated ? "curatedCard_" : "localCard_") + (itemData && itemData.id ? itemData.id : "unknown")

    property var itemData: null
    property bool isCurated: false
    property bool isActive: false
    property color accent: "#22e58b"
    property real glass: 0.72

    signal selectRequested(string modelId)
    signal clearSelectionRequested()
    signal detailsRequested(var itemData)
    signal verifyRequested(string modelId)
    signal removeRequested(var itemData)
    signal downloadRequested(var itemData)

    readonly property string effectiveStatus: {
        if (!itemData) return "not_installed";
        if (isCurated) {
            return itemData.installedStatus || (itemData.installed ? "available" : "not_installed");
        }
        return itemData.status || "unavailable";
    }
    readonly property bool isAvailable: effectiveStatus === "available"
    readonly property bool isInstalled: isCurated ? Boolean(itemData && itemData.installed) : true
    readonly property real cardAlpha: Math.max(0.38, Math.min(0.92, glass * 0.88))

    function formatKind(k) {
        if (k === "piper_tts") return qsTr("Piper / TTS");
        if (k === "whisper_asr") return qsTr("Whisper / ASR");
        if (k === "local_llm") return qsTr("LLM");
        if (k === "voice") return qsTr("Voices");
        return k || qsTr("Unknown");
    }

    function requiredRuntime(k) {
        if (k === "piper_tts") return qsTr("Piper ONNX Runtime");
        if (k === "whisper_asr") return qsTr("whisper.cpp / GGML");
        if (k === "local_llm") return qsTr("llama.cpp (GGUF)");
        if (k === "voice") return qsTr("Audioticon Voice Profile");
        return qsTr("Local runtime");
    }

    function formatSize(bytes) {
        var b = Number(bytes || 0);
        if (b <= 0) return qsTr("Unknown");
        if (b < 1024) return b + " B";
        if (b < 1024 * 1024) return (b / 1024).toFixed(1) + " KB";
        if (b < 1024 * 1024 * 1024) return (b / (1024 * 1024)).toFixed(1) + " MB";
        return (b / (1024 * 1024 * 1024)).toFixed(2) + " GB";
    }

    function statusLabel(st) {
        if (st === "available") return qsTr("AVAILABLE");
        if (st === "missing_metadata") return qsTr("MISSING METADATA");
        if (st === "corrupt") return qsTr("CORRUPT");
        if (st === "incomplete") return qsTr("INCOMPLETE");
        if (st === "not_installed") return qsTr("NOT INSTALLED");
        return (st || "UNKNOWN").toUpperCase();
    }

    function statusColor(st) {
        if (st === "available") return cardRoot.accent;
        if (st === "missing_metadata") return "#ffd43b";
        if (st === "corrupt") return "#ff4d6d";
        if (st === "incomplete") return "#ff9f43";
        return "#7d9aa6";
    }

    function statusBg(st) {
        if (st === "available") return Qt.rgba(0.13, 0.90, 0.55, 0.14);
        if (st === "missing_metadata") return Qt.rgba(1.0, 0.83, 0.23, 0.14);
        if (st === "corrupt") return Qt.rgba(1.0, 0.30, 0.43, 0.16);
        if (st === "incomplete") return Qt.rgba(1.0, 0.62, 0.26, 0.16);
        return Qt.rgba(0.49, 0.60, 0.65, 0.14);
    }

    function sourceDisplay() {
        if (!itemData) return qsTr("Local");
        if (isCurated) {
            var origin = itemData.origin || "Curated";
            return qsTr("Curated") + " · " + origin;
        }
        return itemData.sourceDisplay || qsTr("Local catalog");
    }

    Layout.fillWidth: true
    implicitHeight: cardCol.implicitHeight + 24
    radius: 12
    color: isActive
           ? Qt.rgba(0.04, 0.13, 0.11, Math.min(0.86, cardAlpha + 0.06))
           : Qt.rgba(0.03, 0.06, 0.09, cardAlpha)
    border.width: isActive ? 1.5 : 1
    border.color: isActive
                  ? Qt.rgba(cardRoot.accent.r, cardRoot.accent.g, cardRoot.accent.b, 0.75)
                  : (effectiveStatus === "corrupt" ? "#7a2433"
                     : effectiveStatus === "missing_metadata" ? "#7a631b"
                     : effectiveStatus === "incomplete" ? "#7a4d1b"
                     : "#1e3a43")

    ColumnLayout {
        id: cardCol
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // Row 1: Title + Status / Active / Type badges
        Flow {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: itemData ? (itemData.name || itemData.id) : ""
                color: "#eff9ff"
                font.pixelSize: 13
                font.weight: Font.DemiBold
                height: 22
                verticalAlignment: Text.AlignVCenter
            }

            // Active model pill
            Rectangle {
                visible: cardRoot.isActive
                objectName: "activeBadge"
                implicitWidth: activeTxt.implicitWidth + 12
                implicitHeight: 20
                radius: 5
                color: Qt.rgba(cardRoot.accent.r, cardRoot.accent.g, cardRoot.accent.b, 0.22)
                border.color: cardRoot.accent
                Text {
                    id: activeTxt
                    anchors.centerIn: parent
                    text: qsTr("ACTIVE")
                    color: cardRoot.accent
                    font.family: "monospace"
                    font.pixelSize: 9
                    font.weight: Font.Bold
                    font.letterSpacing: 0.8
                }
            }

            // Status pill
            Rectangle {
                objectName: "statusBadge"
                implicitWidth: stTxt.implicitWidth + 12
                implicitHeight: 20
                radius: 5
                color: cardRoot.statusBg(cardRoot.effectiveStatus)
                border.color: cardRoot.statusColor(cardRoot.effectiveStatus)
                Text {
                    id: stTxt
                    anchors.centerIn: parent
                    text: cardRoot.statusLabel(cardRoot.effectiveStatus)
                    color: cardRoot.statusColor(cardRoot.effectiveStatus)
                    font.family: "monospace"
                    font.pixelSize: 9
                    font.weight: Font.Bold
                }
            }

            // Type pill
            Rectangle {
                implicitWidth: kindTxt.implicitWidth + 12
                implicitHeight: 20
                radius: 5
                color: Qt.rgba(0.0, 0.82, 1.0, 0.12)
                border.color: "#1e5668"
                Text {
                    id: kindTxt
                    anchors.centerIn: parent
                    text: cardRoot.formatKind(cardRoot.itemData ? cardRoot.itemData.kind : "")
                    color: "#7adfff"
                    font.family: "monospace"
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                }
            }
        }

        // Row 2: Complete metadata chips (language, format, size, license, author, source, runtime)
        Flow {
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: [
                    { label: qsTr("Lang"), value: (cardRoot.itemData && cardRoot.itemData.language) ? cardRoot.itemData.language : qsTr("Unknown") },
                    { label: qsTr("Format"), value: (cardRoot.itemData && cardRoot.itemData.format) ? cardRoot.itemData.format.toUpperCase() : "—" },
                    { label: qsTr("Size"), value: cardRoot.formatSize(cardRoot.itemData ? cardRoot.itemData.sizeBytes : 0) },
                    { label: qsTr("License"), value: (cardRoot.itemData && cardRoot.itemData.license) ? cardRoot.itemData.license : qsTr("Unknown") },
                    { label: qsTr("Author"), value: (cardRoot.itemData && cardRoot.itemData.author) ? cardRoot.itemData.author : qsTr("Local / Unknown") },
                    { label: qsTr("Source"), value: cardRoot.sourceDisplay() },
                    { label: qsTr("Runtime"), value: cardRoot.requiredRuntime(cardRoot.itemData ? cardRoot.itemData.kind : "") }
                ]

                Rectangle {
                    required property var modelData
                    implicitWidth: metaRow.implicitWidth + 12
                    implicitHeight: 22
                    radius: 5
                    color: Qt.rgba(0.015, 0.03, 0.045, 0.65)
                    border.color: "#1a2f3a"

                    Row {
                        id: metaRow
                        anchors.centerIn: parent
                        spacing: 4
                        Text {
                            text: modelData.label + ":"
                            color: "#5f818f"
                            font.family: "monospace"
                            font.pixelSize: 9
                        }
                        Text {
                            text: modelData.value
                            color: "#c6dfe8"
                            font.pixelSize: 10
                        }
                    }
                }
            }
        }

        // Row 3: Diagnostics detail or curated description
        Rectangle {
            visible: Boolean(cardRoot.itemData && ((cardRoot.itemData.detail && cardRoot.itemData.detail.length > 0)
                             || (isCurated && cardRoot.itemData.description && cardRoot.itemData.description.length > 0)))
            Layout.fillWidth: true
            implicitHeight: diagTxt.implicitHeight + 12
            radius: 7
            color: (cardRoot.effectiveStatus === "corrupt")
                   ? Qt.rgba(1.0, 0.25, 0.35, 0.14)
                   : (cardRoot.effectiveStatus === "missing_metadata" || cardRoot.effectiveStatus === "incomplete")
                     ? Qt.rgba(1.0, 0.78, 0.20, 0.12)
                     : Qt.rgba(0.02, 0.05, 0.07, 0.55)
            border.color: (cardRoot.effectiveStatus === "corrupt")
                          ? "#802938"
                          : (cardRoot.effectiveStatus === "missing_metadata" || cardRoot.effectiveStatus === "incomplete")
                            ? "#7a611c"
                            : "#19303b"

            Text {
                id: diagTxt
                anchors.fill: parent
                anchors.margins: 6
                text: {
                    if (!cardRoot.itemData) return "";
                    if (cardRoot.itemData.detail && cardRoot.itemData.detail.length > 0) return cardRoot.itemData.detail;
                    if (isCurated && cardRoot.itemData.description) return cardRoot.itemData.description;
                    return "";
                }
                color: cardRoot.effectiveStatus === "corrupt" ? "#ff9eaa"
                       : (cardRoot.effectiveStatus === "missing_metadata" || cardRoot.effectiveStatus === "incomplete")
                         ? "#ffe08a" : "#8eb0bd"
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }

        // Row 4: Action buttons (Flow prevents clipping at 780x560)
        Flow {
            Layout.fillWidth: true
            spacing: 8

            // Select active / Clear active
            Button {
                objectName: "selectModelBtn"
                visible: cardRoot.isInstalled && !cardRoot.isActive
                enabled: cardRoot.isAvailable
                text: qsTr("Select active")
                highlighted: cardRoot.isAvailable
                font.pixelSize: 11
                onClicked: if (cardRoot.itemData) cardRoot.selectRequested(cardRoot.itemData.id)
            }

            Button {
                objectName: "clearActiveBtn"
                visible: cardRoot.isActive
                text: qsTr("Clear active")
                font.pixelSize: 11
                onClicked: cardRoot.clearSelectionRequested()
            }

            // Curated download (opens explicit Glass consent modal)
            Button {
                objectName: "downloadPresetBtn"
                visible: cardRoot.isCurated && !cardRoot.isAvailable
                text: cardRoot.isInstalled ? qsTr("Re-download…") : qsTr("Download…")
                highlighted: !cardRoot.isInstalled
                font.pixelSize: 11
                onClicked: if (cardRoot.itemData) cardRoot.downloadRequested(cardRoot.itemData)
            }

            // Details
            Button {
                objectName: "detailsModelBtn"
                text: qsTr("Details")
                font.pixelSize: 11
                onClicked: if (cardRoot.itemData) cardRoot.detailsRequested(cardRoot.itemData)
            }

            // Verify (installed models)
            Button {
                objectName: "verifyModelBtn"
                visible: cardRoot.isInstalled
                text: qsTr("Verify")
                font.pixelSize: 11
                onClicked: if (cardRoot.itemData) cardRoot.verifyRequested(cardRoot.itemData.id)
            }

            // Remove (installed models)
            Button {
                objectName: "removeModelBtn"
                visible: cardRoot.isInstalled
                text: qsTr("Remove")
                font.pixelSize: 11
                onClicked: if (cardRoot.itemData) cardRoot.removeRequested(cardRoot.itemData)
            }
        }
    }
}
