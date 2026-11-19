# Whisper ASR (Planned Integration Design)

> **Status:** Planned / Architecture Foundation Available in Model Hub (M17–M19).
> *Audio transcription runtime is not yet active in the current build.*

---

## 1. Goal & Requirements

Audiobook production frequently requires turning spoken recordings, voice actor auditions, or author audio notes into synchronized, directed scripts.

Audioticon 2.0 plans to incorporate **Whisper ASR** (Automatic Speech Recognition) under the following constraints:
1. **Local-First & Offline:** Powered by `whisper.cpp` (GGML format), executing directly on CPU (AVX2/NEON) or Vulkan/ROCm/CUDA GPU acceleration.
2. **No HTTP Daemons:** No local Whisper REST API servers or external cloud transcription endpoints.
3. **Synchronous or Job-Queued:** Transcription jobs will run through the existing `BridgeEngine` task registry with percentage progression.

---

## 2. Model Catalog Integration (Implemented in M17–M19)

The model catalog in `backend/crates/core/src/model_catalog.rs` already supports Whisper models:
- **Storage Directory:** `$XDG_DATA_HOME/audioticon/models/whisper/`
- **Supported Formats:**
  - `ggml-base.bin`, `ggml-small.bin`, `ggml-medium.bin` (whisper.cpp monolithic binaries with embedded metadata).
  - Multi-file models with accompanying `config.json`.
- **Pre-configured Curated Presets:**
  - `whisper/ggml-base` (approx. 142 MB, MIT license)
  - `whisper/ggml-small` (approx. 466 MB, MIT license)

---

## 3. Planned Workflow

```
[Audio File (.wav / .mp3 / .flac)]
               │
               ▼
      [Whisper ASR Worker]
    (whisper.cpp engine binding)
               │
               ▼
   [Timestamped Segment Stream]
               │
               ▼
[Automated Directive Generator / Script Timeline]
```

When implemented, the generated transcription will populate the Studio timeline directly, allowing sound designers to edit verbalized segments, adjust tempo, and insert emotional directives immediately.

---

*Related Pages:*
* [Model Hub](Model-Hub.md)
* [Roadmap](Roadmap.md)
* [Architecture](Architecture.md)
