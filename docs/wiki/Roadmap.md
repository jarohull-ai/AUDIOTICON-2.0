# Roadmap

This roadmap documents the sequence of past, current, and upcoming milestones for Audioticon 2.0 based on the master product blueprint ([M15_PRODUCT_DIRECTION_RESET.md](https://github.com/jarohull-ai/AUDIOTICON-2.0/blob/main/M15_PRODUCT_DIRECTION_RESET.md)).

---

## 1. Completed Milestones

### Phase 0: Safe Bridge & Core Transplant
- [x] **M0–M4: FFI Link Proof & Foundation:** Replaced legacy Web/Node architecture with pure Rust core + C++17 Qt 6. Validated `cxx` thread safety and static linkage.
- [x] **M5–M8: Render Pipeline & Audio Player:** Non-blocking job registry in Rust; native `QAudioSink` low-latency PCM/WAV player in Qt.
- [x] **M9–M14: Studio Layout & Diagnostics:** Directive timeline editor, Doctor diagnostics module, and First Run Wizard.

### Phase 1: Product Reset & UX Solidification (P0)
- [x] **M15: Product Direction Reset:** Removed remaining web artifacts; audited functional gaps across P0, P1, and P2 tiers.
- [x] **M16: P0 UX Stabilization:**
  - Resolved all vertical clipping and scrolling issues across `780×560`, `1180×850`, and `1920×1080`.
  - 100% Polish/English translation coverage (503 keys).
  - Unified liquid glass opacity and automated fallback for deleted wallpaper assets.

### Phase 2: Offline Model Management (P1 Foundation)
- [x] **M17: Local Model Catalog & FFI Contract:**
  - XDG-compliant scanner for Piper, Whisper, LLM, and voice models.
  - Identification of `available`, `missing_metadata`, and `corrupt` states.
  - Atomic selection persistence in `model_selection.json`.
- [x] **M18: Safe Model Import Backend:**
  - Curated presets (100% offline listing).
  - Atomic staging in `.staging/<import-id>/intent.json`.
  - Checksum validation, ZIP path traversal defense, and executable denylists.
- [x] **M19: Native Model Hub UI:**
  - Filter tabs (All, Piper/TTS, Whisper/ASR, LLM, Voices).
  - Source sections (Curated, Local, GitHub, Hugging Face).
  - Modal Glass consent dialog with strict checkbox protection.
  - Zero false-success reporting on failed/missing assets.

---

## 2. In Progress & Upcoming Milestones

### Milestone M20: TTS Synthesis Parameters & Cache Hash *(In Progress)*
- [ ] Expose dynamic speech rate (`0.5x` to `2.0x`), pitch modulation, and volume sliders in the Studio timeline and Context Inspector.
- [ ] Add clause pause and sentence pause duration configuration.
- [ ] Incorporate speech parameters into the Rust cache hash key to prevent false cache hits.

### Milestone M21: Custom Directives & TOML Specification
- [ ] Allow users to define custom emoji directives in `~/.config/audioticon/audioticon.toml`.
- [ ] Align the C++ directive parser with the Rust emoji director.
- [ ] Provide golden parity regression tests between Rust and C++ parsers.

### Milestone M22: VoiceLab & Cast Management
- [ ] Replace the `VoiceLab` (`Ctrl+3`) navigation placeholder with an active character profile manager.
- [ ] Assign distinct voice models, pitch offsets, and acoustic filters to specific book characters.

### Milestone M23: Whisper ASR Integration
- [ ] Integrate local `whisper.cpp` speech-to-text runner without external daemons.
- [ ] Transcribe recorded voice actor takes directly into the Studio timeline.

### Milestone M24: Local LLM Script Directing
- [ ] Connect quantized GGUF models (`llama.cpp`) to automate script directive annotation.
- [ ] Zero-hallucination guarantee: only pauses and emojis may be injected; raw book text is preserved verbatim.

### Milestone M25: Final Visual Polish (Fable / Hydrogen / Carbon)
- [ ] Final typography, micro-motion animations, and 3-theme palette pass (frozen until all P1 functional milestones are complete).

---

*Related Pages:*
* [Overview](Overview.md)
* [Architecture](Architecture.md)
* [Model Hub](Model-Hub.md)
