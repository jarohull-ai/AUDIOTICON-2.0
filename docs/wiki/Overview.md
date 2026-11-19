# Overview

## Purpose

Audioticon was originally conceived as a browser-based audiobook production tool. However, browser environments introduced fundamental limitations: fragile Node.js HTTP servers, heavy WebViews, latency in audio rendering, IPC security issues, and lack of deterministic memory control.

**Audioticon 2.0 is a complete ground-up transplant** into a native desktop architecture:
1. **Domain Engine:** Written in Rust (`backend/crates/`), managing audio synthesis, multi-track normalization, master loudness calculation, and model catalog scanning.
2. **Application Shell:** Written in C++17 and Qt 6 (`ui/`), providing responsive, fluid 60fps liquid-glass rendering with Qt QuickControls2.

---

## Current Status (Milestone M19)

Audioticon 2.0 has closed its foundational architecture and first-run UX stages:
- **M0–M4:** FFI Link Proof, thread boundaries, and `cxx` bridge validation.
- **M5–M8:** Render job registry, non-blocking polling, and `QAudioSink` PCM/WAV playback.
- **M9–M14:** Studio layout, Doctor diagnostics, and First Run Wizard.
- **M15–M16:** Product reset: removal of non-native elements, resolution of 780×560 overflow, 100% Polish/English i18n coverage (503 keys), and wallpaper fallback.
- **M17–M19:** Local model catalog, offline metadata extraction, staging, SHA-256 verification, and the native **Model Hub** interface.

---

## What Audioticon 2.0 Does NOT Do

To maintain complete architectural honesty:
- **No HTTP Server:** There is no local web server (`express`, `actix-web`, etc.) running in the background.
- **No Embedded Browser:** No `QWebEngineView`, Electron, or Tauri runtimes.
- **No Silent Cloud Telemetry:** No user data or script content ever leaves the computer.
- **No Unsupervised Downloads:** Remote models are fetched only when the user explicitly clicks download and approves the network consent modal.
- **No Fake Success:** If dependencies or models are missing, the engine reports `Failed` or `unavailable`.

---

## Feature Matrix Summary

| Subsystem | Milestone | Implementation Status | Notes |
|---|---|---|---|
| Safe Rust ↔ Qt FFI | M4 / M17 | **Implemented** | Memory safe via `cxx` DTOs |
| Piper TTS Synthesis | M7 / M9 | **Implemented** | Local ONNX execution via worker thread |
| Audio Playback | M8 | **Implemented** | Real-time `QAudioSink` with seek/pause |
| Doctor Diagnostics | M13 / M16 | **Implemented** | Verifies FFmpeg, Sox, models, XDG paths |
| First Run Wizard | M13 / M16 | **Implemented** | Responsive onboarding at 780×560+ |
| Full i18n (PL / EN) | M16 | **Implemented** | 503 keys, zero untranslated UI strings |
| Wallpaper & Glass | M10 / M16 | **Implemented** | Video/static wallpapers, unified glass opacity |
| Model Hub UI | M19 | **Implemented** | Offline catalog, curated presets, consent modal |
| TTS Parameters & Cache | M20 | *In Progress* | Sliders for rate/pitch/pause, cache hash key |
| Custom Directives / TOML | M21 | *Planned* | User-defined emoji directives in C++ parser |
| VoiceLab Profiles | M22 | *Planned* | Speaker assignment to characters |
| Whisper STT (ASR) | M23 | *Planned* | Local audio transcription via whisper.cpp |
| Local LLM Directing | M24 | *Planned* | Automated emoji script directing via GGUF |
| Final Visual Polish | M25 | *Planned* | Visual theme touch (Fable / Hydrogen / Carbon) |

---

*Related Pages:*
* [Architecture](Architecture.md)
* [Director Workstation UX](Director-Workstation-UX.md)
* [Model Hub](Model-Hub.md)
