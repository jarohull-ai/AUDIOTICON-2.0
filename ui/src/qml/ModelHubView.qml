// ModelHubView.qml — Native Model Hub / Katalog modeli for Audioticon 2.0 (M19)
// Offline-first catalog UI over M17 + M18 Rust FFI via StudioBridge.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Rectangle {
    id: hubRoot
    objectName: "modelHubView"

    property color accent: "#22e58b"
    property real glass: 0.72

    // Sections: "curated" | "local" | "github_release" | "hugging_face"
    property string activeSection: "curated"
    // Kind filter: "all" | "piper_tts" | "whisper_asr" | "local_llm" | "voice"
    property string activeKindFilter: "all"

    property var catalogStatus: ({})
    property var localModels: []
    property var presets: []
    property var sources: []
    property var filteredLocalModels: []
    property var filteredPresets: []

    property string remoteInspectError: ""
    property var consentCandidate: null
    property string consentReference: ""
    property string consentExpectedSha256: ""
    property real consentExpectedSize: 0

    property var detailsItem: null
    property var removeTarget: null
    property var lastVerifyResult: null

    readonly property real tileAlpha: Math.max(0.36, Math.min(0.92, glass * 0.90))
    readonly property string activeModelId: (typeof studio !== "undefined" && studio) ? studio.activeModelId : ""

    signal returnToStudioRequested()

    color: Qt.rgba(0.02, 0.04, 0.06, glass)
    radius: 16
    border.color: "#1e3a43"
    clip: true

    function formatSize(bytes) {
        var b = Number(bytes || 0);
        if (b <= 0) return qsTr("Unknown");
        if (b < 1024) return b + " B";
        if (b < 1024 * 1024) return (b / 1024).toFixed(1) + " KB";
        if (b < 1024 * 1024 * 1024) return (b / (1024 * 1024)).toFixed(1) + " MB";
        return (b / (1024 * 1024 * 1024)).toFixed(2) + " GB";
    }

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

    function sourceInfoFor(kind) {
        for (var i = 0; i < sources.length; ++i) {
            if (sources[i].kind === kind) return sources[i];
        }
        return null;
    }

    function applyFilters() {
        var fLocal = [];
        for (var i = 0; i < localModels.length; ++i) {
            var m = localModels[i];
            if (activeKindFilter === "all" || m.kind === activeKindFilter)
                fLocal.push(m);
        }
        filteredLocalModels = fLocal;

        var fPresets = [];
        for (var j = 0; j < presets.length; ++j) {
            var p = presets[j];
            if (activeKindFilter === "all" || p.kind === activeKindFilter)
                fPresets.push(p);
        }
        filteredPresets = fPresets;
    }

    function refreshCatalog() {
        if (typeof studio === "undefined" || !studio)
            return;
        catalogStatus = studio.modelStatus();
        sources = studio.modelSources();
        var rawPresets = studio.curatedPresets();
        presets = rawPresets;

        var presetById = {};
        for (var i = 0; i < rawPresets.length; ++i) {
            presetById[rawPresets[i].id] = rawPresets[i];
        }

        var rawModels = studio.models();
        var enriched = [];
        for (var j = 0; j < rawModels.length; ++j) {
            var item = rawModels[j];
            var copy = Object.assign({}, item);
            var matchedPreset = presetById[item.id];
            if (matchedPreset) {
                copy.author = matchedPreset.author || qsTr("Local / Unknown");
                copy.sourceDisplay = qsTr("Local") + " · " + (matchedPreset.origin || "Curated");
            } else {
                copy.author = qsTr("Local / Unknown");
                copy.sourceDisplay = item.status === "incomplete"
                                     ? qsTr("Staging (incomplete)")
                                     : qsTr("Local catalog");
            }
            enriched.push(copy);
        }
        localModels = enriched;
        applyFilters();
    }

    function openConsentForReference(reference, expectedSha256, expectedSize) {
        remoteInspectError = "";
        if (typeof studio === "undefined" || !studio)
            return false;
        var trimmed = (reference || "").trim();
        if (trimmed.length === 0) {
            remoteInspectError = qsTr("Enter a valid remote reference or URL first.");
            return false;
        }
        var candidate = studio.inspectRemoteCandidate(trimmed);
        if (!candidate || candidate.error) {
            remoteInspectError = (candidate && candidate.error)
                                 ? candidate.error
                                 : qsTr("Unable to inspect remote candidate.");
            return false;
        }
        consentCandidate = candidate;
        consentReference = trimmed;
        consentExpectedSha256 = (expectedSha256 || "").trim();
        consentExpectedSize = Number(expectedSize || 0);
        remoteConsentCheckbox.checked = false;
        remoteConsentModal.open();
        return true;
    }

    function openLocalImport(isZipMode) {
        localImportModal.zipMode = Boolean(isZipMode);
        localImportPathField.text = "";
        localImportKindCombo.currentIndex = 0;
        localImportAcceptLicense.checked = false;
        localImportModal.open();
    }

    function openDetails(itemData) {
        if (!itemData) return;
        var full = Object.assign({}, itemData);
        if (typeof studio !== "undefined" && studio && itemData.id) {
            var inspected = studio.inspectModel(itemData.id);
            if (inspected && inspected.id) {
                full = Object.assign(full, inspected);
            }
        }
        detailsItem = full;
        modelDetailsModal.open();
    }

    function openRemoveConfirm(itemData) {
        if (!itemData || !itemData.id) return;
        removeTarget = itemData;
        removeConfirmModal.open();
    }

    function runVerify(modelId) {
        if (typeof studio === "undefined" || !studio || !modelId)
            return;
        lastVerifyResult = studio.verifyModelDetailed(modelId);
        refreshCatalog();
    }

    onActiveKindFilterChanged: applyFilters()

    Component.onCompleted: refreshCatalog()

    Connections {
        target: (typeof studio !== "undefined" && studio) ? studio : null
        function onModelSelectionChanged() {
            hubRoot.refreshCatalog();
            if (typeof activeDoctor !== "undefined" && activeDoctor)
                activeDoctor.refresh();
        }
        function onModelImportFinished(status) {
            hubRoot.refreshCatalog();
            if (typeof activeDoctor !== "undefined" && activeDoctor)
                activeDoctor.refresh();
        }
    }

    // Native file/folder dialogs for local import
    FileDialog {
        id: localFileDialog
        title: qsTr("Select model file (.onnx, .gguf, .bin, .json)")
        nameFilters: [
            qsTr("Model files (*.onnx *.gguf *.bin *.json)"),
            qsTr("All files (*)")
        ]
        onAccepted: {
            if (typeof studio !== "undefined" && studio)
                localImportPathField.text = studio.urlToLocalPath(selectedFile);
        }
    }

    FolderDialog {
        id: localFolderDialog
        title: qsTr("Select local model folder")
        onAccepted: {
            if (typeof studio !== "undefined" && studio)
                localImportPathField.text = studio.urlToLocalPath(selectedFolder);
        }
    }

    FileDialog {
        id: localZipDialog
        title: qsTr("Select model ZIP archive")
        nameFilters: [qsTr("ZIP archives (*.zip)"), qsTr("All files (*)")]
        onAccepted: {
            if (typeof studio !== "undefined" && studio)
                localImportPathField.text = studio.urlToLocalPath(selectedFile);
        }
    }

    // ========================================================================
    // MAIN SCROLLABLE WORKSPACE
    // ========================================================================
    ScrollView {
        id: hubScroll
        anchors.fill: parent
        anchors.margins: 18
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: hubScroll.availableWidth
            spacing: 14

            // ----------------------------------------------------------------
            // 1. HEADER
            // ----------------------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    width: 42
                    height: 42
                    radius: 12
                    color: Qt.rgba(hubRoot.accent.r, hubRoot.accent.g, hubRoot.accent.b, 0.12)
                    border.color: Qt.rgba(hubRoot.accent.r, hubRoot.accent.g, hubRoot.accent.b, 0.4)
                    Text {
                        anchors.centerIn: parent
                        text: "📦"
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
                            text: qsTr("MODEL HUB · LOCAL CATALOG")
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            font.letterSpacing: 1.1
                            color: "#eff9ff"
                            elide: Text.ElideRight
                        }
                        Rectangle {
                            implicitWidth: offlineTagTxt.implicitWidth + 10
                            implicitHeight: 18
                            radius: 4
                            color: Qt.rgba(0.13, 0.90, 0.55, 0.14)
                            border.color: "#1b6e4b"
                            Text {
                                id: offlineTagTxt
                                anchors.centerIn: parent
                                text: qsTr("OFFLINE-FIRST")
                                color: hubRoot.accent
                                font.family: "monospace"
                                font.pixelSize: 8
                                font.weight: Font.Bold
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("PIPER TTS · WHISPER ASR · LOCAL LLM · VOICE PROFILES · EXPLICIT CONSENT ONLY")
                        font.family: "monospace"
                        font.pixelSize: 9
                        font.letterSpacing: 1.0
                        color: "#6c8c97"
                        elide: Text.ElideRight
                    }
                }

                Button {
                    text: qsTr("🎙️ Back to Studio")
                    font.pixelSize: 11
                    onClicked: hubRoot.returnToStudioRequested()
                }
            }

            // ----------------------------------------------------------------
            // 2. ACTIVE MODEL & CATALOG STATUS SUMMARY CARD
            // ----------------------------------------------------------------
            Rectangle {
                id: activeStatusCard
                objectName: "activeModelStatusCard"
                Layout.fillWidth: true
                implicitHeight: statusSummaryCol.implicitHeight + 20
                radius: 12
                color: (hubRoot.catalogStatus.activeStatus === "available")
                       ? Qt.rgba(0.04, 0.16, 0.11, Math.min(0.70, hubRoot.tileAlpha))
                       : Qt.rgba(0.05, 0.08, 0.11, hubRoot.tileAlpha)
                border.color: (hubRoot.catalogStatus.activeStatus === "available")
                              ? "#1b6e4b"
                              : "#24404d"

                ColumnLayout {
                    id: statusSummaryCol
                    anchors.fill: parent
                    anchors.margins: 11
                    spacing: 8

                    Flow {
                        Layout.fillWidth: true
                        spacing: 10

                        Row {
                            spacing: 8
                            height: 24
                            Rectangle {
                                width: 10; height: 10; radius: 5
                                anchors.verticalCenter: parent.verticalCenter
                                color: hubRoot.catalogStatus.activeStatus === "available"
                                       ? hubRoot.accent
                                       : (hubRoot.catalogStatus.total > 0 ? "#ffd43b" : "#7d9aa6")
                            }
                            Text {
                                objectName: "activeModelLabel"
                                anchors.verticalCenter: parent.verticalCenter
                                text: hubRoot.activeModelId.length > 0
                                      ? qsTr("Active model: %1").arg(hubRoot.activeModelId)
                                      : qsTr("No active model selected")
                                color: "#eff9ff"
                                font.family: "monospace"
                                font.pixelSize: 11
                                font.weight: Font.Bold
                            }
                        }

                        // Counters
                        Rectangle {
                            implicitWidth: cntTotal.implicitWidth + 12; implicitHeight: 22; radius: 5
                            color: Qt.rgba(0.01, 0.025, 0.04, 0.65); border.color: "#1e3a43"
                            Text {
                                id: cntTotal; anchors.centerIn: parent
                                text: qsTr("Local: %1").arg(hubRoot.catalogStatus.total || 0)
                                color: "#a8c7d4"; font.family: "monospace"; font.pixelSize: 10
                            }
                        }
                        Rectangle {
                            implicitWidth: cntAvail.implicitWidth + 12; implicitHeight: 22; radius: 5
                            color: Qt.rgba(0.13, 0.90, 0.55, 0.12); border.color: "#1b6e4b"
                            Text {
                                id: cntAvail; anchors.centerIn: parent
                                text: qsTr("Available: %1").arg(hubRoot.catalogStatus.available || 0)
                                color: hubRoot.accent; font.family: "monospace"; font.pixelSize: 10
                            }
                        }
                        Rectangle {
                            visible: Number(hubRoot.catalogStatus.unavailable || 0) > 0
                            implicitWidth: cntUnavail.implicitWidth + 12; implicitHeight: 22; radius: 5
                            color: Qt.rgba(1.0, 0.30, 0.43, 0.14); border.color: "#7a2433"
                            Text {
                                id: cntUnavail; anchors.centerIn: parent
                                text: qsTr("Unavailable: %1").arg(hubRoot.catalogStatus.unavailable || 0)
                                color: "#ff8093"; font.family: "monospace"; font.pixelSize: 10
                            }
                        }
                        Rectangle {
                            visible: Number(hubRoot.catalogStatus.incomplete || 0) > 0
                            implicitWidth: cntInc.implicitWidth + 12; implicitHeight: 22; radius: 5
                            color: Qt.rgba(1.0, 0.62, 0.26, 0.16); border.color: "#80501c"
                            Text {
                                id: cntInc; anchors.centerIn: parent
                                text: qsTr("Staging / Incomplete: %1").arg(hubRoot.catalogStatus.incomplete || 0)
                                color: "#ffb86c"; font.family: "monospace"; font.pixelSize: 10
                            }
                        }

                        Button {
                            visible: hubRoot.activeModelId.length > 0
                            text: qsTr("Clear active")
                            font.pixelSize: 10
                            implicitHeight: 26
                            onClicked: if (typeof studio !== "undefined" && studio) studio.clearModelSelection()
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Catalog directory: %1").arg(hubRoot.catalogStatus.catalogRoot || "—")
                        color: "#6c8c97"
                        font.family: "monospace"
                        font.pixelSize: 10
                        elide: Text.ElideMiddle
                    }
                }
            }

            // ----------------------------------------------------------------
            // 3. IMPORT / STAGING / VERIFICATION STATUS BANNER
            //    Never reports success before verification completes!
            // ----------------------------------------------------------------
            Rectangle {
                id: importStatusBanner
                objectName: "importStatusBanner"
                readonly property bool isBusy: (typeof studio !== "undefined" && studio) ? studio.importBusy : false
                readonly property string stage: (typeof studio !== "undefined" && studio) ? studio.importStage : ""
                readonly property string msg: (typeof studio !== "undefined" && studio) ? studio.importMessage : ""
                readonly property string err: (typeof studio !== "undefined" && studio) ? studio.importError : ""

                visible: isBusy || stage.length > 0 || err.length > 0
                Layout.fillWidth: true
                implicitHeight: importCol.implicitHeight + 20
                radius: 10
                color: isBusy
                       ? Qt.rgba(0.0, 0.55, 0.78, 0.16)
                       : (stage === "completed" ? Qt.rgba(0.13, 0.90, 0.55, 0.14)
                          : (stage === "cancelled" ? Qt.rgba(1.0, 0.78, 0.20, 0.14)
                             : Qt.rgba(1.0, 0.25, 0.35, 0.16)))
                border.color: isBusy
                              ? "#00a6d6"
                              : (stage === "completed" ? "#1b6e4b"
                                 : (stage === "cancelled" ? "#8f7014" : "#ff3b56"))

                ColumnLayout {
                    id: importCol
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Text {
                            objectName: "importStatusTitle"
                            text: {
                                if (importStatusBanner.isBusy) return qsTr("STAGING & VERIFICATION IN PROGRESS");
                                if (importStatusBanner.stage === "completed") return qsTr("VERIFIED & INSTALLED");
                                if (importStatusBanner.stage === "cancelled") return qsTr("IMPORT CANCELLED");
                                return qsTr("IMPORT / DOWNLOAD ERROR");
                            }
                            color: importStatusBanner.isBusy ? "#7adfff"
                                   : (importStatusBanner.stage === "completed" ? hubRoot.accent
                                      : (importStatusBanner.stage === "cancelled" ? "#ffd43b" : "#ff8093"))
                            font.family: "monospace"
                            font.pixelSize: 10
                            font.weight: Font.Bold
                            Layout.fillWidth: true
                        }

                        Button {
                            id: cancelImportBtn
                            objectName: "cancelImportBtn"
                            visible: importStatusBanner.isBusy
                            text: qsTr("Cancel import")
                            font.pixelSize: 10
                            implicitHeight: 26
                            onClicked: if (typeof studio !== "undefined" && studio) studio.cancelActiveImport()
                        }

                        ToolButton {
                            visible: !importStatusBanner.isBusy
                            text: "✕"
                            font.pixelSize: 12
                            onClicked: if (typeof studio !== "undefined" && studio) studio.resetImportState()
                        }
                    }

                    ProgressBar {
                        visible: importStatusBanner.isBusy
                        Layout.fillWidth: true
                        indeterminate: true
                    }

                    Text {
                        objectName: "importStatusDetail"
                        Layout.fillWidth: true
                        text: importStatusBanner.err.length > 0 ? importStatusBanner.err : importStatusBanner.msg
                        color: "#e6f5fb"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // Verification result banner (after clicking "Verify")
            Rectangle {
                id: verifyBanner
                objectName: "verifyResultBanner"
                visible: hubRoot.lastVerifyResult !== null
                Layout.fillWidth: true
                implicitHeight: verifyCol.implicitHeight + 18
                radius: 10
                color: (hubRoot.lastVerifyResult && hubRoot.lastVerifyResult.ok)
                       ? Qt.rgba(0.13, 0.90, 0.55, 0.12)
                       : Qt.rgba(1.0, 0.25, 0.35, 0.16)
                border.color: (hubRoot.lastVerifyResult && hubRoot.lastVerifyResult.ok)
                              ? "#1b6e4b" : "#ff3b56"

                ColumnLayout {
                    id: verifyCol
                    anchors.fill: parent
                    anchors.margins: 9
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: (hubRoot.lastVerifyResult && hubRoot.lastVerifyResult.ok)
                                  ? qsTr("VERIFICATION PASSED: %1").arg(hubRoot.lastVerifyResult.id || "")
                                  : qsTr("VERIFICATION FAILED: %1").arg(hubRoot.lastVerifyResult ? (hubRoot.lastVerifyResult.id || "") : "")
                            color: (hubRoot.lastVerifyResult && hubRoot.lastVerifyResult.ok) ? hubRoot.accent : "#ff8093"
                            font.family: "monospace"
                            font.pixelSize: 10
                            font.weight: Font.Bold
                            Layout.fillWidth: true
                        }
                        ToolButton {
                            text: "✕"
                            font.pixelSize: 11
                            onClicked: hubRoot.lastVerifyResult = null
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: {
                            if (!hubRoot.lastVerifyResult) return "";
                            if (hubRoot.lastVerifyResult.error) return hubRoot.lastVerifyResult.error;
                            var sha = hubRoot.lastVerifyResult.actualSha256 || "";
                            var base = hubRoot.lastVerifyResult.message || "";
                            return sha.length > 0 ? (base + " · SHA-256: " + sha) : base;
                        }
                        color: "#d4eaf2"
                        font.family: "monospace"
                        font.pixelSize: 10
                        wrapMode: Text.WrapAnywhere
                    }
                }
            }

            // ----------------------------------------------------------------
            // 4. SECTIONS BAR (Curated / Local / GitHub / Hugging Face)
            // ----------------------------------------------------------------
            Flow {
                id: modelSectionBar
                objectName: "modelSectionBar"
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: [
                        { id: "curated",        icon: "⭐", label: qsTr("Curated / Recommended"), count: hubRoot.presets.length,      net: false },
                        { id: "local",          icon: "💾", label: qsTr("Local"),                 count: hubRoot.localModels.length,  net: false },
                        { id: "github_release", icon: "🐙", label: qsTr("GitHub"),                count: -1,                          net: true  },
                        { id: "hugging_face",   icon: "🤗", label: qsTr("Hugging Face"),          count: -1,                          net: true  }
                    ]

                    Rectangle {
                        required property var modelData
                        objectName: "sectionTab_" + modelData.id
                        readonly property bool selected: hubRoot.activeSection === modelData.id
                        implicitWidth: secRow.implicitWidth + 20
                        implicitHeight: 34
                        radius: 9
                        color: selected
                               ? Qt.rgba(hubRoot.accent.r, hubRoot.accent.g, hubRoot.accent.b, 0.16)
                               : Qt.rgba(0.03, 0.06, 0.09, hubRoot.tileAlpha)
                        border.color: selected ? hubRoot.accent : "#1e3a43"

                        Row {
                            id: secRow
                            anchors.centerIn: parent
                            spacing: 6
                            Text { text: modelData.icon; font.pixelSize: 13 }
                            Text {
                                text: modelData.label
                                color: selected ? "#eff9ff" : "#9bb8c2"
                                font.pixelSize: 11
                                font.weight: selected ? Font.DemiBold : Font.Normal
                            }
                            Rectangle {
                                visible: modelData.count >= 0
                                implicitWidth: cntLbl.implicitWidth + 8
                                implicitHeight: 16
                                radius: 8
                                anchors.verticalCenter: parent.verticalCenter
                                color: Qt.rgba(0.0, 0.0, 0.0, 0.35)
                                Text {
                                    id: cntLbl
                                    anchors.centerIn: parent
                                    text: String(modelData.count)
                                    color: selected ? hubRoot.accent : "#7d9aa6"
                                    font.family: "monospace"
                                    font.pixelSize: 9
                                }
                            }
                            Rectangle {
                                visible: modelData.net
                                implicitWidth: netLbl.implicitWidth + 8
                                implicitHeight: 16
                                radius: 4
                                anchors.verticalCenter: parent.verticalCenter
                                color: Qt.rgba(1.0, 0.83, 0.23, 0.14)
                                border.color: "#7a611c"
                                Text {
                                    id: netLbl
                                    anchors.centerIn: parent
                                    text: qsTr("CONSENT")
                                    color: "#ffd43b"
                                    font.family: "monospace"
                                    font.pixelSize: 8
                                    font.weight: Font.Bold
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                hubRoot.activeSection = modelData.id;
                                hubRoot.remoteInspectError = "";
                            }
                        }
                    }
                }
            }

            // ----------------------------------------------------------------
            // 5. TYPE FILTERS + QUICK ACTIONS TOOLBAR (Flow -> zero overflow)
            // ----------------------------------------------------------------
            Flow {
                id: modelFilterBar
                objectName: "modelFilterBar"
                Layout.fillWidth: true
                spacing: 6
                visible: hubRoot.activeSection === "curated" || hubRoot.activeSection === "local"

                Repeater {
                    model: [
                        { id: "all",         label: qsTr("All") },
                        { id: "piper_tts",   label: qsTr("Piper / TTS") },
                        { id: "whisper_asr", label: qsTr("Whisper / ASR") },
                        { id: "local_llm",   label: qsTr("LLM") },
                        { id: "voice",       label: qsTr("Voices") }
                    ]

                    Rectangle {
                        required property var modelData
                        objectName: "filterChip_" + modelData.id
                        readonly property bool selected: hubRoot.activeKindFilter === modelData.id
                        implicitWidth: chipTxt.implicitWidth + 18
                        implicitHeight: 26
                        radius: 13
                        color: selected
                               ? Qt.rgba(0.0, 0.82, 1.0, 0.18)
                               : Qt.rgba(0.02, 0.045, 0.065, hubRoot.tileAlpha)
                        border.color: selected ? "#00d2ff" : "#1e3a43"

                        Text {
                            id: chipTxt
                            anchors.centerIn: parent
                            text: modelData.label
                            color: selected ? "#eff9ff" : "#8daab5"
                            font.pixelSize: 11
                            font.weight: selected ? Font.DemiBold : Font.Normal
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: hubRoot.activeKindFilter = modelData.id
                        }
                    }
                }
            }

            Flow {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    id: importLocalBtn
                    objectName: "importLocalBtn"
                    text: qsTr("Import file / folder")
                    font.pixelSize: 11
                    onClicked: hubRoot.openLocalImport(false)
                }

                Button {
                    id: importZipBtn
                    objectName: "importZipBtn"
                    text: qsTr("Import ZIP")
                    font.pixelSize: 11
                    onClicked: hubRoot.openLocalImport(true)
                }

                Button {
                    id: addRemoteBtn
                    objectName: "addRemoteBtn"
                    text: qsTr("Add from GitHub / Hugging Face")
                    font.pixelSize: 11
                    onClicked: hubRoot.activeSection = (hubRoot.activeSection === "github_release" ? "hugging_face" : "github_release")
                }

                Button {
                    id: refreshModelsBtn
                    objectName: "refreshModelsBtn"
                    text: qsTr("Refresh")
                    font.pixelSize: 11
                    onClicked: hubRoot.refreshCatalog()
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#1a2e38" }

            // ----------------------------------------------------------------
            // 6A. SECTION: CURATED / RECOMMENDED
            // ----------------------------------------------------------------
            ColumnLayout {
                visible: hubRoot.activeSection === "curated"
                Layout.fillWidth: true
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Curated presets (Portable Local Studio). Listing is 100% offline; downloading requires explicit consent.")
                    color: "#7d9aa6"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }

                Repeater {
                    id: curatedPresetsRepeater
                    objectName: "curatedPresetsRepeater"
                    model: hubRoot.filteredPresets
                    ModelCard {
                        required property var modelData
                        itemData: modelData
                        isCurated: true
                        isActive: hubRoot.activeModelId.length > 0 && modelData.id === hubRoot.activeModelId
                        accent: hubRoot.accent
                        glass: hubRoot.glass
                        onSelectRequested: function(id) { if (studio) studio.selectModel(id); }
                        onClearSelectionRequested: if (studio) studio.clearModelSelection()
                        onDetailsRequested: function(item) { hubRoot.openDetails(item); }
                        onVerifyRequested: function(id) { hubRoot.runVerify(id); }
                        onRemoveRequested: function(item) { hubRoot.openRemoveConfirm(item); }
                        onDownloadRequested: function(item) {
                            hubRoot.openConsentForReference("preset:" + item.id, "", item.sizeBytes || 0);
                        }
                    }
                }

                Rectangle {
                    visible: hubRoot.filteredPresets.length === 0
                    Layout.fillWidth: true
                    implicitHeight: 72
                    radius: 10
                    color: Qt.rgba(0.03, 0.06, 0.09, hubRoot.tileAlpha)
                    border.color: "#1e3a43"
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("No curated presets match the selected category.")
                        color: "#7d9aa6"
                        font.pixelSize: 12
                    }
                }
            }

            // ----------------------------------------------------------------
            // 6B. SECTION: LOCAL MODELS
            // ----------------------------------------------------------------
            ColumnLayout {
                visible: hubRoot.activeSection === "local"
                Layout.fillWidth: true
                spacing: 10

                // Empty catalog notice
                Rectangle {
                    id: emptyCatalogNotice
                    objectName: "emptyCatalogNotice"
                    visible: hubRoot.filteredLocalModels.length === 0
                    Layout.fillWidth: true
                    implicitHeight: emptyCol.implicitHeight + 28
                    radius: 12
                    color: Qt.rgba(0.03, 0.06, 0.09, hubRoot.tileAlpha)
                    border.color: "#1e3a43"

                    ColumnLayout {
                        id: emptyCol
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8

                        Text {
                            text: hubRoot.localModels.length === 0
                                  ? qsTr("Local model catalog is empty")
                                  : qsTr("No local models in this category")
                            color: "#eff9ff"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Import a local model file, directory, or ZIP archive, or inspect a curated preset before downloading.")
                            color: "#7d9aa6"
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 8
                            Button {
                                text: qsTr("Import file / folder")
                                highlighted: true
                                font.pixelSize: 11
                                onClicked: hubRoot.openLocalImport(false)
                            }
                            Button {
                                text: qsTr("Import ZIP")
                                font.pixelSize: 11
                                onClicked: hubRoot.openLocalImport(true)
                            }
                            Button {
                                text: qsTr("Browse Curated")
                                font.pixelSize: 11
                                onClicked: hubRoot.activeSection = "curated"
                            }
                        }
                    }
                }

                Repeater {
                    id: localModelsRepeater
                    objectName: "localModelsRepeater"
                    model: hubRoot.filteredLocalModels
                    ModelCard {
                        required property var modelData
                        itemData: modelData
                        isCurated: false
                        isActive: hubRoot.activeModelId.length > 0 && modelData.id === hubRoot.activeModelId
                        accent: hubRoot.accent
                        glass: hubRoot.glass
                        onSelectRequested: function(id) { if (studio) studio.selectModel(id); }
                        onClearSelectionRequested: if (studio) studio.clearModelSelection()
                        onDetailsRequested: function(item) { hubRoot.openDetails(item); }
                        onVerifyRequested: function(id) { hubRoot.runVerify(id); }
                        onRemoveRequested: function(item) { hubRoot.openRemoveConfirm(item); }
                    }
                }
            }

            // ----------------------------------------------------------------
            // 6C. SECTION: GITHUB RELEASES / HUGGING FACE (EXPLICIT CONSENT)
            // ----------------------------------------------------------------
            Rectangle {
                id: remoteSourcePanel
                objectName: "remoteSourcePanel"
                visible: hubRoot.activeSection === "github_release" || hubRoot.activeSection === "hugging_face"
                Layout.fillWidth: true
                implicitHeight: remoteCol.implicitHeight + 28
                radius: 12
                color: Qt.rgba(0.03, 0.06, 0.09, hubRoot.tileAlpha)
                border.color: "#224350"

                readonly property bool isGitHub: hubRoot.activeSection === "github_release"
                readonly property var srcMeta: hubRoot.sourceInfoFor(hubRoot.activeSection)

                ColumnLayout {
                    id: remoteCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Text {
                            text: remoteSourcePanel.isGitHub
                                  ? qsTr("Add model from GitHub Releases")
                                  : qsTr("Add model from Hugging Face")
                            color: "#eff9ff"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            Layout.fillWidth: true
                        }
                        Rectangle {
                            implicitWidth: netWarnPill.implicitWidth + 12
                            implicitHeight: 20
                            radius: 5
                            color: Qt.rgba(1.0, 0.83, 0.23, 0.14)
                            border.color: "#8f7014"
                            Text {
                                id: netWarnPill
                                anchors.centerIn: parent
                                text: qsTr("NETWORK · EXPLICIT CONSENT")
                                color: "#ffd43b"
                                font.family: "monospace"
                                font.pixelSize: 8
                                font.weight: Font.Bold
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: remoteSourcePanel.srcMeta && remoteSourcePanel.srcMeta.detail
                              ? remoteSourcePanel.srcMeta.detail
                              : qsTr("Inspection is performed offline. Network access happens only after you verify metadata and confirm in the consent modal.")
                        color: "#88a8b5"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        text: remoteSourcePanel.isGitHub
                              ? qsTr("GITHUB RELEASE REFERENCE OR HTTPS URL")
                              : qsTr("HUGGING FACE REFERENCE OR HTTPS URL")
                        color: "#5f818f"
                        font.family: "monospace"
                        font.pixelSize: 9
                        font.weight: Font.Bold
                    }

                    TextField {
                        id: remoteRefField
                        objectName: "remoteRefField"
                        Layout.fillWidth: true
                        placeholderText: remoteSourcePanel.isGitHub
                                         ? "gh:owner/repo@v1.0.0#model.gguf"
                                         : "hf:owner/repo@main#model.gguf"
                        font.family: "monospace"
                        font.pixelSize: 11
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: hubRoot.width >= 680 ? 2 : 1
                        columnSpacing: 10
                        rowSpacing: 8

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Text {
                                text: qsTr("EXPECTED SHA-256 CHECKSUM (64 HEX CHARS)")
                                color: "#5f818f"
                                font.family: "monospace"
                                font.pixelSize: 9
                                font.weight: Font.Bold
                            }
                            TextField {
                                id: remoteShaField
                                objectName: "remoteShaField"
                                Layout.fillWidth: true
                                placeholderText: qsTr("Required for non-curated remote sources")
                                font.family: "monospace"
                                font.pixelSize: 11
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Text {
                                text: qsTr("EXPECTED SIZE (BYTES)")
                                color: "#5f818f"
                                font.family: "monospace"
                                font.pixelSize: 9
                                font.weight: Font.Bold
                            }
                            TextField {
                                id: remoteSizeField
                                objectName: "remoteSizeField"
                                Layout.fillWidth: true
                                placeholderText: qsTr("e.g. 63201294")
                                font.family: "monospace"
                                font.pixelSize: 11
                            }
                        }
                    }

                    // Inline inspection error
                    Rectangle {
                        visible: hubRoot.remoteInspectError.length > 0
                        Layout.fillWidth: true
                        implicitHeight: remErrTxt.implicitHeight + 12
                        radius: 7
                        color: Qt.rgba(1.0, 0.25, 0.35, 0.16)
                        border.color: "#ff3b56"
                        Text {
                            id: remErrTxt
                            anchors.fill: parent
                            anchors.margins: 6
                            text: hubRoot.remoteInspectError
                            color: "#ff9eaa"
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        Button {
                            id: inspectRemoteBtn
                            objectName: "inspectRemoteBtn"
                            highlighted: true
                            text: remoteSourcePanel.isGitHub
                                  ? qsTr("Inspect & Add from GitHub")
                                  : qsTr("Inspect & Add from Hugging Face")
                            font.pixelSize: 11
                            onClicked: {
                                var sz = parseInt(remoteSizeField.text.trim(), 10);
                                if (isNaN(sz) || sz < 0) sz = 0;
                                hubRoot.openConsentForReference(remoteRefField.text, remoteShaField.text, sz);
                            }
                        }
                        Button {
                            text: qsTr("Clear form")
                            font.pixelSize: 11
                            onClicked: {
                                remoteRefField.text = "";
                                remoteShaField.text = "";
                                remoteSizeField.text = "";
                                hubRoot.remoteInspectError = "";
                            }
                        }
                    }
                }
            }
        }
    }

    // ========================================================================
    // MODAL 1: REMOTE DOWNLOAD EXPLICIT CONSENT MODAL (GLASS)
    // ========================================================================
    Popup {
        id: remoteConsentModal
        objectName: "remoteConsentModal"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(580, (parent ? parent.width : hubRoot.width) - 32)
        height: Math.min(620, (parent ? parent.height : hubRoot.height) - 32)
        modal: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        readonly property var cand: hubRoot.consentCandidate
        readonly property bool isCuratedCand: Boolean(cand && cand.curated)
        readonly property bool hasValidSha: hubRoot.consentExpectedSha256.length === 64
        readonly property bool hasValidSize: hubRoot.consentExpectedSize > 0
        readonly property bool canConfirm: remoteConsentCheckbox.checked
                                           && Boolean(cand)
                                           && (isCuratedCand || (hasValidSha && hasValidSize))

        background: Rectangle {
            radius: 16
            color: Qt.rgba(0.03, 0.06, 0.09, Math.max(0.86, hubRoot.glass))
            border.color: "#2b5263"
            border.width: 1.5
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            // Modal Header
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Text {
                    text: qsTr("Confirm remote model download")
                    color: "#eff9ff"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                }
                ToolButton {
                    text: "✕"
                    font.pixelSize: 14
                    onClicked: remoteConsentModal.close()
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#1c3340" }

            // Scrollable metadata body (zero clipping at 780x560)
            ScrollView {
                id: consentScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: consentScroll.availableWidth
                    spacing: 10

                    // Network connection warning
                    Rectangle {
                        objectName: "remoteNetworkWarning"
                        Layout.fillWidth: true
                        implicitHeight: netWarnCol.implicitHeight + 16
                        radius: 10
                        color: Qt.rgba(1.0, 0.78, 0.20, 0.14)
                        border.color: "#b38612"

                        ColumnLayout {
                            id: netWarnCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 4
                            Text {
                                text: qsTr("NETWORK CONNECTION WARNING")
                                color: "#ffd43b"
                                font.family: "monospace"
                                font.pixelSize: 10
                                font.weight: Font.Bold
                            }
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Confirming will initiate an on-demand download from the remote host below. No background downloads occur. Files are written to .staging and verified before activation.")
                                color: "#f6e7b8"
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    // Candidate metadata table
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: metaGrid.implicitHeight + 18
                        radius: 10
                        color: Qt.rgba(0.015, 0.035, 0.055, 0.75)
                        border.color: "#1e3a43"

                        GridLayout {
                            id: metaGrid
                            anchors.fill: parent
                            anchors.margins: 9
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 6

                            Text { text: qsTr("Source"); color: "#6c8c97"; font.family: "monospace"; font.pixelSize: 10 }
                            Text {
                                objectName: "consentSourceValue"
                                Layout.fillWidth: true
                                text: remoteConsentModal.cand
                                      ? ((remoteConsentModal.cand.source || "") + " · " + (remoteConsentModal.cand.sourceReference || hubRoot.consentReference))
                                      : "—"
                                color: "#eff9ff"; font.pixelSize: 11; wrapMode: Text.WrapAnywhere
                            }

                            Text { text: qsTr("Author"); color: "#6c8c97"; font.family: "monospace"; font.pixelSize: 10 }
                            Text {
                                objectName: "consentAuthorValue"
                                Layout.fillWidth: true
                                text: (remoteConsentModal.cand && remoteConsentModal.cand.author) ? remoteConsentModal.cand.author : qsTr("Unknown")
                                color: "#eff9ff"; font.pixelSize: 11; wrapMode: Text.WordWrap
                            }

                            Text { text: qsTr("License"); color: "#6c8c97"; font.family: "monospace"; font.pixelSize: 10 }
                            Text {
                                objectName: "consentLicenseValue"
                                Layout.fillWidth: true
                                text: (remoteConsentModal.cand && remoteConsentModal.cand.license) ? remoteConsentModal.cand.license : qsTr("Unknown")
                                color: "#eff9ff"; font.pixelSize: 11; wrapMode: Text.WordWrap
                            }

                            Text { text: qsTr("Size"); color: "#6c8c97"; font.family: "monospace"; font.pixelSize: 10 }
                            Text {
                                objectName: "consentSizeValue"
                                Layout.fillWidth: true
                                text: {
                                    var sz = (remoteConsentModal.cand && remoteConsentModal.cand.sizeBytes > 0)
                                             ? remoteConsentModal.cand.sizeBytes
                                             : hubRoot.consentExpectedSize;
                                    return hubRoot.formatSize(sz);
                                }
                                color: "#eff9ff"; font.pixelSize: 11
                            }

                            Text { text: qsTr("Checksum"); color: "#6c8c97"; font.family: "monospace"; font.pixelSize: 10 }
                            Text {
                                objectName: "consentChecksumValue"
                                Layout.fillWidth: true
                                text: {
                                    if (hubRoot.consentExpectedSha256.length > 0)
                                        return hubRoot.consentExpectedSha256;
                                    if (remoteConsentModal.cand && remoteConsentModal.cand.files) {
                                        for (var i = 0; i < remoteConsentModal.cand.files.length; ++i) {
                                            if (remoteConsentModal.cand.files[i].sha256)
                                                return remoteConsentModal.cand.files[i].sha256;
                                        }
                                    }
                                    return qsTr("Not published upstream — SHA-256 will be computed and saved after download");
                                }
                                color: "#c4dfe8"; font.family: "monospace"; font.pixelSize: 10; wrapMode: Text.WrapAnywhere
                            }
                        }
                    }

                    // File list
                    Text {
                        text: qsTr("FILES TO DOWNLOAD")
                        color: "#5f818f"
                        font.family: "monospace"
                        font.pixelSize: 9
                        font.weight: Font.Bold
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: filesCol.implicitHeight + 14
                        radius: 10
                        color: Qt.rgba(0.015, 0.035, 0.055, 0.75)
                        border.color: "#1e3a43"

                        ColumnLayout {
                            id: filesCol
                            anchors.fill: parent
                            anchors.margins: 7
                            spacing: 6

                            Repeater {
                                model: (remoteConsentModal.cand && remoteConsentModal.cand.files) ? remoteConsentModal.cand.files : []
                                ColumnLayout {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    spacing: 1
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            text: (modelData.fileName || "file") + " (" + (modelData.role || "asset") + ")"
                                            color: "#eff9ff"
                                            font.family: "monospace"
                                            font.pixelSize: 10
                                            Layout.fillWidth: true
                                            elide: Text.ElideMiddle
                                        }
                                        Text {
                                            text: hubRoot.formatSize(modelData.sizeBytes || 0)
                                            color: "#7d9aa6"
                                            font.family: "monospace"
                                            font.pixelSize: 9
                                        }
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.url || ""
                                        color: "#5f818f"
                                        font.family: "monospace"
                                        font.pixelSize: 9
                                        elide: Text.ElideMiddle
                                    }
                                }
                            }
                        }
                    }

                    // Blockers (if any)
                    Rectangle {
                        visible: Boolean(remoteConsentModal.cand && remoteConsentModal.cand.blockers && remoteConsentModal.cand.blockers.length > 0)
                        Layout.fillWidth: true
                        implicitHeight: blockersCol.implicitHeight + 14
                        radius: 8
                        color: Qt.rgba(1.0, 0.25, 0.35, 0.14)
                        border.color: "#802938"

                        ColumnLayout {
                            id: blockersCol
                            anchors.fill: parent
                            anchors.margins: 7
                            spacing: 4
                            Text {
                                text: qsTr("REQUIREMENTS / BLOCKERS")
                                color: "#ff8093"
                                font.family: "monospace"
                                font.pixelSize: 9
                                font.weight: Font.Bold
                            }
                            Repeater {
                                model: (remoteConsentModal.cand && remoteConsentModal.cand.blockers) ? remoteConsentModal.cand.blockers : []
                                Text {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    text: "• " + String(modelData)
                                    color: "#ffb4bf"
                                    font.pixelSize: 10
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }

                    // Explicit user consent checkbox
                    CheckBox {
                        id: remoteConsentCheckbox
                        objectName: "remoteConsentCheckbox"
                        Layout.fillWidth: true
                        text: qsTr("I explicitly confirm this source, license, and network download")
                        font.pixelSize: 11
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#1c3340" }

            // Modal Footer
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }
                Button {
                    id: remoteConsentCancelBtn
                    objectName: "remoteConsentCancelBtn"
                    text: qsTr("Cancel")
                    onClicked: remoteConsentModal.close()
                }
                Button {
                    id: remoteConsentConfirmBtn
                    objectName: "remoteConsentConfirmBtn"
                    highlighted: true
                    enabled: remoteConsentModal.canConfirm
                    text: qsTr("Confirm & Download")
                    onClicked: {
                        remoteConsentModal.close();
                        if (typeof studio !== "undefined" && studio) {
                            studio.startDownloadRemote(
                                hubRoot.consentReference,
                                hubRoot.consentExpectedSha256,
                                hubRoot.consentExpectedSize,
                                true
                            );
                        }
                    }
                }
            }
        }
    }

    // ========================================================================
    // MODAL 2: LOCAL IMPORT MODAL (FILE / FOLDER / ZIP)
    // ========================================================================
    Popup {
        id: localImportModal
        objectName: "localImportModal"
        property bool zipMode: false
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(520, (parent ? parent.width : hubRoot.width) - 32)
        height: Math.min(400, (parent ? parent.height : hubRoot.height) - 32)
        modal: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: 16
            color: Qt.rgba(0.03, 0.06, 0.09, Math.max(0.86, hubRoot.glass))
            border.color: "#2b5263"
            border.width: 1.5
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: localImportModal.zipMode
                          ? qsTr("Import local ZIP archive")
                          : qsTr("Import local model file or folder")
                    color: "#eff9ff"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                }
                ToolButton { text: "✕"; font.pixelSize: 14; onClicked: localImportModal.close() }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#1c3340" }

            ScrollView {
                id: localImportScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: localImportScroll.availableWidth
                    spacing: 10

                    Text {
                        text: qsTr("SOURCE PATH (100% OFFLINE)")
                        color: "#5f818f"
                        font.family: "monospace"
                        font.pixelSize: 9
                        font.weight: Font.Bold
                    }

                    TextField {
                        id: localImportPathField
                        objectName: "localImportPathField"
                        Layout.fillWidth: true
                        placeholderText: localImportModal.zipMode
                                         ? qsTr("/path/to/model.zip")
                                         : qsTr("/path/to/model.onnx or model directory")
                        font.family: "monospace"
                        font.pixelSize: 11
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        Button {
                            visible: !localImportModal.zipMode
                            text: qsTr("Choose file…")
                            font.pixelSize: 11
                            onClicked: localFileDialog.open()
                        }
                        Button {
                            visible: !localImportModal.zipMode
                            text: qsTr("Choose folder…")
                            font.pixelSize: 11
                            onClicked: localFolderDialog.open()
                        }
                        Button {
                            visible: localImportModal.zipMode
                            text: qsTr("Choose ZIP…")
                            font.pixelSize: 11
                            onClicked: localZipDialog.open()
                        }
                    }

                    Text {
                        text: qsTr("MODEL TYPE (OPTIONAL OVERRIDE)")
                        color: "#5f818f"
                        font.family: "monospace"
                        font.pixelSize: 9
                        font.weight: Font.Bold
                    }

                    ComboBox {
                        id: localImportKindCombo
                        Layout.fillWidth: true
                        readonly property var codes: ["", "piper_tts", "whisper_asr", "local_llm", "voice"]
                        model: [qsTr("Auto-detect"), qsTr("Piper / TTS"), qsTr("Whisper / ASR"), qsTr("LLM"), qsTr("Voices")]
                    }

                    CheckBox {
                        id: localImportAcceptLicense
                        objectName: "localImportAcceptLicense"
                        Layout.fillWidth: true
                        text: qsTr("Accept model if license metadata is missing")
                        font.pixelSize: 11
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#1c3340" }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }
                Button { text: qsTr("Cancel"); onClicked: localImportModal.close() }
                Button {
                    objectName: "localImportConfirmBtn"
                    highlighted: true
                    enabled: localImportPathField.text.trim().length > 0
                    text: qsTr("Import & Verify")
                    onClicked: {
                        var path = localImportPathField.text.trim();
                        var kind = localImportKindCombo.codes[localImportKindCombo.currentIndex];
                        var acceptLic = localImportAcceptLicense.checked;
                        localImportModal.close();
                        if (typeof studio !== "undefined" && studio)
                            studio.startImportLocal(path, kind, acceptLic);
                    }
                }
            }
        }
    }

    // ========================================================================
    // MODAL 3: MODEL DETAILS MODAL (GLASS)
    // ========================================================================
    Popup {
        id: modelDetailsModal
        objectName: "modelDetailsModal"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(540, (parent ? parent.width : hubRoot.width) - 32)
        height: Math.min(520, (parent ? parent.height : hubRoot.height) - 32)
        modal: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: 16
            color: Qt.rgba(0.03, 0.06, 0.09, Math.max(0.86, hubRoot.glass))
            border.color: "#2b5263"
            border.width: 1.5
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: hubRoot.detailsItem ? (hubRoot.detailsItem.name || hubRoot.detailsItem.id) : qsTr("Model details")
                    color: "#eff9ff"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                ToolButton { text: "✕"; font.pixelSize: 14; onClicked: modelDetailsModal.close() }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#1c3340" }

            ScrollView {
                id: detailsScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                GridLayout {
                    width: detailsScroll.availableWidth
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 8

                    Text { text: qsTr("ID"); color: "#6c8c97"; font.family: "monospace"; font.pixelSize: 10 }
                    Text { Layout.fillWidth: true; text: hubRoot.detailsItem ? (hubRoot.detailsItem.id || "—") : "—"; color: "#eff9ff"; font.family: "monospace"; font.pixelSize: 10; wrapMode: Text.WrapAnywhere }

                    Text { text: qsTr("Type"); color: "#6c8c97"; font.family: "monospace"; font.pixelSize: 10 }
                    Text { Layout.fillWidth: true; text: hubRoot.formatKind(hubRoot.detailsItem ? hubRoot.detailsItem.kind : ""); color: "#eff9ff"; font.pixelSize: 11 }

                    Text { text: qsTr("Runtime"); color: "#6c8c97"; font.family: "monospace"; font.pixelSize: 10 }
                    Text { Layout.fillWidth: true; text: hubRoot.requiredRuntime(hubRoot.detailsItem ? hubRoot.detailsItem.kind : ""); color: "#eff9ff"; font.pixelSize: 11 }

                    Text { text: qsTr("Status"); color: "#6c8c97"; font.family: "monospace"; font.pixelSize: 10 }
                    Text { Layout.fillWidth: true; text: hubRoot.detailsItem ? (hubRoot.detailsItem.status || hubRoot.detailsItem.installedStatus || "—") : "—"; color: "#eff9ff"; font.family: "monospace"; font.pixelSize: 11 }

                    Text { text: qsTr("Language"); color: "#6c8c97"; font.family: "monospace"; font.pixelSize: 10 }
                    Text { Layout.fillWidth: true; text: (hubRoot.detailsItem && hubRoot.detailsItem.language) ? hubRoot.detailsItem.language : qsTr("Unknown"); color: "#eff9ff"; font.pixelSize: 11 }

                    Text { text: qsTr("Format"); color: "#6c8c97"; font.family: "monospace"; font.pixelSize: 10 }
                    Text { Layout.fillWidth: true; text: (hubRoot.detailsItem && hubRoot.detailsItem.format) ? hubRoot.detailsItem.format.toUpperCase() : "—"; color: "#eff9ff"; font.pixelSize: 11 }

                    Text { text: qsTr("Size"); color: "#6c8c97"; font.family: "monospace"; font.pixelSize: 10 }
                    Text { Layout.fillWidth: true; text: hubRoot.formatSize(hubRoot.detailsItem ? hubRoot.detailsItem.sizeBytes : 0); color: "#eff9ff"; font.pixelSize: 11 }

                    Text { text: qsTr("License"); color: "#6c8c97"; font.family: "monospace"; font.pixelSize: 10 }
                    Text { Layout.fillWidth: true; text: (hubRoot.detailsItem && hubRoot.detailsItem.license) ? hubRoot.detailsItem.license : qsTr("Unknown"); color: "#eff9ff"; font.pixelSize: 11 }

                    Text { text: qsTr("Author"); color: "#6c8c97"; font.family: "monospace"; font.pixelSize: 10 }
                    Text { Layout.fillWidth: true; text: (hubRoot.detailsItem && hubRoot.detailsItem.author) ? hubRoot.detailsItem.author : qsTr("Local / Unknown"); color: "#eff9ff"; font.pixelSize: 11 }

                    Text { text: qsTr("Path"); color: "#6c8c97"; font.family: "monospace"; font.pixelSize: 10 }
                    Text { Layout.fillWidth: true; text: (hubRoot.detailsItem && hubRoot.detailsItem.path) ? hubRoot.detailsItem.path : "—"; color: "#a8c7d4"; font.family: "monospace"; font.pixelSize: 10; wrapMode: Text.WrapAnywhere }

                    Text { text: qsTr("SHA-256"); color: "#6c8c97"; font.family: "monospace"; font.pixelSize: 10 }
                    Text { Layout.fillWidth: true; text: (hubRoot.detailsItem && hubRoot.detailsItem.checksumSha256) ? hubRoot.detailsItem.checksumSha256 : qsTr("No sidecar checksum"); color: "#a8c7d4"; font.family: "monospace"; font.pixelSize: 10; wrapMode: Text.WrapAnywhere }

                    Text { text: qsTr("Diagnostics"); color: "#6c8c97"; font.family: "monospace"; font.pixelSize: 10 }
                    Text { Layout.fillWidth: true; text: (hubRoot.detailsItem && hubRoot.detailsItem.detail) ? hubRoot.detailsItem.detail : "—"; color: "#e6f5fb"; font.pixelSize: 11; wrapMode: Text.WordWrap }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#1c3340" }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button { text: qsTr("Close"); onClicked: modelDetailsModal.close() }
            }
        }
    }

    // ========================================================================
    // MODAL 4: REMOVE MODEL CONFIRMATION MODAL (GLASS)
    // ========================================================================
    Popup {
        id: removeConfirmModal
        objectName: "removeConfirmModal"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(440, (parent ? parent.width : hubRoot.width) - 32)
        modal: true
        padding: 18
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: 16
            color: Qt.rgba(0.03, 0.06, 0.09, Math.max(0.88, hubRoot.glass))
            border.color: "#7a2433"
            border.width: 1.5
        }

        ColumnLayout {
            width: parent.width
            spacing: 12

            Text {
                text: qsTr("Remove local model?")
                color: "#eff9ff"
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }
            Text {
                Layout.fillWidth: true
                text: hubRoot.removeTarget
                      ? qsTr("This removes local files for '%1' from the catalog. If active, selection will be cleared.").arg(hubRoot.removeTarget.id)
                      : ""
                color: "#c6dfe8"
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }
                Button { text: qsTr("Cancel"); onClicked: removeConfirmModal.close() }
                Button {
                    objectName: "confirmRemoveModelBtn"
                    text: qsTr("Remove")
                    onClicked: {
                        var id = hubRoot.removeTarget ? hubRoot.removeTarget.id : "";
                        removeConfirmModal.close();
                        if (typeof studio !== "undefined" && studio && id.length > 0)
                            studio.removeModel(id);
                    }
                }
            }
        }
    }
}
