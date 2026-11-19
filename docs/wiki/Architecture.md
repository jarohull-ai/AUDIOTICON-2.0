# Architecture

Audioticon 2.0 is split cleanly into two distinct runtime tiers: the **Rust Domain Backend** and the **Qt Quick C++ Presentation Layer**.

```
+---------------------------------------------------------------------------------+
|                               Qt 6 Quick Front-End                              |
|                                                                                 |
|   AgentMain.qml  <--->  StudioView.qml  <--->  ModelHubView.qml  <--->  Doctor  |
|         ^                     ^                      ^                     ^    |
|         |                     |                      |                     |    |
|   NavRail.qml           MasteringStrip        ModelCard.qml       FirstRunWizard|
+---------------------------------------------------------------------------------+
                                      | (Qt Signals, Slots, Q_PROPERTY)
                                      v
+---------------------------------------------------------------------------------+
|                       C++ Façade (ui/src/bridge/)                               |
|                                                                                 |
|   StudioBridge.{h,cpp}       AudioticonCore.{h,cpp}        AudioPlayer.{h,cpp}  |
|   (QML API, UI State,        (Single C++ unit that         (QAudioSink PCM/WAV  |
|    Worker Threads)            includes cxx headers)         audio playback)     |
+---------------------------------------------------------------------------------+
                                      | (cxx C++ <-> Rust FFI Contract)
                                      v
+---------------------------------------------------------------------------------+
|                      audioticon-bridge (backend/crates/bridge/)                 |
|                                                                                 |
|   cxx::bridge  --->  BridgeEngine  --->  TaskRegistry (Arc<RwLock<...>>)        |
+---------------------------------------------------------------------------------+
                                      |
                                      v
+---------------------------------------------------------------------------------+
|                      audioticon-core (backend/crates/core/)                     |
|                                                                                 |
|   model_catalog.rs     model_sources.rs     model_import.rs     audio.rs        |
|   (XDG scan,           (Curated presets,    (Staging, SHA-256,  (Piper worker,  |
|    status validation)   remote inspect)      atomic extract)     DSP, loudnorm) |
+---------------------------------------------------------------------------------+
```

---

## 1. Rust Domain Core (`backend/crates/`)

The backend is structured into modular crates:
- **`audioticon-core`**:
  - `model_catalog.rs`: Scans `$XDG_DATA_HOME/audioticon/models/` across `piper/`, `whisper/`, `llm/`, and `voices/`. It verifies weight file existence, sidecar metadata, and calculates current status (`available`, `missing_metadata`, `corrupt`, `incomplete`).
  - `model_sources.rs`: Hardcoded definitions for curated Portable Local Studio presets and remote reference parsing (`preset:...`, `hf:...`, `gh:...`). Contains no network transport.
  - `model_import.rs`: Staging lifecycle (`.staging/<import-id>/intent.json`), atomic directory moves, ZIP archive safety checks, and SHA-256 validation.
  - `audio.rs`: Synthesis invocation and audio normalization logic.
- **`audioticon-bridge`**:
  - Bridges the core functionality to C++ using `cxx`.
  - Exposes `BridgeEngine`, registering long-running asynchronous synthesis tasks in an in-memory thread-safe registry (`TaskRegistry`).
- **`audioticon-cli`**:
  - Command-line tool for headless script rendering, catalog querying, and benchmarking.

---

## 2. Safe FFI Boundary (`cxx`)

Unlike traditional C ABI wrappers (`extern "C"`) that pass raw pointers or `void*`, Audioticon 2.0 uses `cxx`:
- **Strong Typing:** Structs (such as `ModelInfoDto`, `CuratedPresetDto`, `ImportResultDto`) are defined in `backend/crates/bridge/src/lib.rs` and compiled into static C++ headers during the build.
- **Single Seam:** In the entire `ui/` codebase, **only** `ui/src/bridge/AudioticonCore.cpp` is permitted to include cxx headers. QML models, audio sinks, and view controllers talk exclusively to `AudioticonCore.h`, ensuring clean decoupling.

---

## 3. UI Presentation Layer (`ui/`)

- **`StudioBridge` (`ui/src/bridge/`):** The primary QML interface singleton. It implements polling timers (50ms) to update task progression without locking the GUI event loop, exposes model catalog methods, and orchestrates background import worker threads.
- **`AudioPlayer` (`ui/src/audio/`):** Low-latency audio stream player built on native `QAudioSink`. Supports real-time seek, pause, dynamic duration calculation, and error notifications.
- **`DoctorModel` (`ui/src/doctor/`):** Diagnostics model that queries system executables (`piper`, `ffmpeg`, `sox`, `espeak-ng`) and validates directory write permissions.

---

*Related Pages:*
* [Model Hub](Model-Hub.md)
* [Piper TTS](Piper-TTS.md)
* [XDG Paths and Runtime Data](XDG-Paths-and-Runtime-Data.md)
