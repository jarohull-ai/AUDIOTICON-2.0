# Third-Party Notices and Open Source Licenses

Audioticon 2.0 incorporates, links against, and interfaces with various third-party open-source libraries, neural runtimes, voice datasets, and media tools. Below is an itemized record of their licenses, copyrights, and terms of use.

---

## 1. Application Core & UI

### Audioticon 2.0 Codebase
- **Root Repository:** [MIT License](LICENSE) (Copyright © 2026 jarohull-ai).
- **Backend Crates (`backend/`):** Dual-licensed under [MIT License](backend/LICENSE-MIT) and [Apache License 2.0](backend/LICENSE-APACHE).
- **Qt Quick Front-End (`ui/`):** [MIT License](ui/LICENSE) (Copyright © 2026 Jaro). Translator and application shell adapted from Omarchy app templates (MIT License, David Heinemeier Hansson).

### Qt Framework
- **Project:** [Qt 6 (The Qt Company)](https://www.qt.io/)
- **License:** LGPLv3 / GPLv3.
- **Components Used:** Qt Core, Qt GUI, Qt Quick, Qt QuickControls2, Qt Multimedia, Qt Network, Qt Test.
- **Linking:** Dynamically linked against system-installed shared libraries (`/usr/lib/libQt6*.so`). Users may replace Qt shared libraries in conformance with LGPLv3 provisions.

### CXX (Safe FFI Interoperability)
- **Project:** [cxx.rs](https://github.com/dtolnay/cxx) (David Tolnay)
- **License:** MIT or Apache-2.0.
- **Description:** Generates safe, verified C++ bindings to Rust data types.

---

## 2. Audio Processing & System Tools

### FFmpeg
- **Project:** [FFmpeg](https://ffmpeg.org/)
- **License:** LGPL v2.1+ (or GPL v3+ depending on enabled encoders).
- **Usage:** External CLI invocation via process spawning for multi-format audio transcoding (FLAC, M4B, MP3) and loudnorm mastering.

### SoX (Sound eXchange)
- **Project:** [SoX SourceForge](https://sourceforge.net/projects/sox/)
- **License:** GPL v2 / LGPL v2.1.
- **Usage:** Optional local audio manipulation and spectrogram diagnostics.

---

## 3. Neural TTS Engines & Voice Models

### Piper Neural Text-to-Speech
- **Project:** [Piper](https://github.com/rhasspy/piper)
- **Author:** Michael Hansen (Rhasspy / Nabu Casa)
- **License:** [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0)
- **Description:** Local neural text-to-speech engine running via ONNX Runtime.

### Voice Model Datasets (rhasspy/piper-voices)
Voice models distributed in the catalog or downloaded on demand carry distinct dataset provenance and licenses:

#### Commercial-Safe Models (CC0 / Public Domain / MIT / Apache-2.0):
- `pl_PL-darkman-medium`: NabuCasa / voice-datasets — **CC0 (Public Domain)**
- `pl_PL-gosia-medium`: NabuCasa / voice-datasets — **CC0 (Public Domain)**
- `pl_PL-mc_speech-medium`: MC Speech Corpus — **CC0 (Public Domain)**
- `en_US-amy-medium`: Amy Dataset — **CC0 (Public Domain)**
- `en_US-ryan-medium`: Ryan Dataset — **CC0 (Public Domain)**
- `de_DE-thorsten-medium`: Thorsten Voice — **CC0 (Public Domain)**

#### Attribution Models (CC-BY-4.0):
- `pl_PL-mls_6892-low`: Multilingual LibriSpeech (MLS) — **Creative Commons Attribution 4.0 International (CC-BY-4.0)**. Commercial use requires prominent credit to the dataset authors.

#### Research / Non-Commercial Models:
- `en_US-lessac-medium`: Blizzard Challenge 2013 dataset — **Research and Non-Commercial Educational License Only**. Commercial audiobook publishing is strictly prohibited with this voice model.

*Audioticon's Model Hub displays the exact license string of every voice model before selection or download.*

### Kokoro-82M (Planned ONNX TTS Integration)
- **Model:** [hexgrad/Kokoro-82M](https://huggingface.co/hexgrad/Kokoro-82M)
- **License:** Apache License 2.0 (`CommercialSafe`).

---

## 4. Media & Visual Assets

### Wallpapers
- `assets/wallpapers/pexels-artyusufpatel-8998289.jpg`: Photography by Art Yusuf Patel via Pexels. Distributed under the [Pexels License](https://www.pexels.com/license/) (Free to use, attribution not strictly required, modification permitted).
