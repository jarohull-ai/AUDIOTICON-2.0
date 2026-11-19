# Local LLM (Planned Integration Design)

> **Status:** Planned / Model Catalog Foundation Available (M17–M19).
> *LLM inference runtime is not yet active in the current build.*

---

## 1. Goal & Requirements

Audiobook directing is time-consuming. Narrators must read prose, identify speaker changes, insert pauses, and apply vocal inflections (whisper, excitement, sorrow, urgency).

Audioticon 2.0 incorporates local LLMs for two specific offline tasks:
1. **Script Directing:** Analyzing plain book text and automatically annotating it with Audioticon emoji directives (`📖`, `😮‍💨`, `🐢`, `⏸️`, `📻`).
2. **Conversational Assistant:** Assisting the narrator in the `AI Chat` view with pronunciation guides, dialect coaching, and ACX guidelines.

---

## 2. Model Catalog Integration (Implemented in M17–M19)

The model catalog in `backend/crates/core/src/model_catalog.rs` fully supports quantized local LLMs:
- **Storage Directory:** `$XDG_DATA_HOME/audioticon/models/llm/`
- **Supported Format:** Quantized GGUF files (`*.gguf`).
- **GGUF Validation:** The scanner validates the 4-byte `GGUF` magic header. Because GGUF encodes architecture, vocabulary, and context parameters internally, companion sidecar files are not required.
- **Curated Preset:**
  - `llm/qwen2.5-3b-instruct-q4` (approx. 2.1 GB, Apache-2.0 license).

---

## 3. Planned Engine: `llama.cpp`

- **Execution:** Embedded through `llama.cpp` C++ bindings or an isolated local subprocess communicating via standard IPC (no web server).
- **Prompt Isolation:** System prompts are strictly deterministic, instructed to output exclusively the Audioticon markup syntax.
- **Zero Hallucination Guarantee:** The directing pipeline enforces that the raw text words cannot be altered by the LLM—only directives (emoji and pauses) may be inserted between words.

---

*Related Pages:*
* [Model Hub](Model-Hub.md)
* [Piper TTS](Piper-TTS.md)
* [Roadmap](Roadmap.md)
