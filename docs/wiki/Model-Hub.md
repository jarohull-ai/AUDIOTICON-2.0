# Model Hub

The **Model Hub** (`Ctrl+8`) is the central offline-first model management dashboard introduced in Milestone M19. It unifies neural voice engines (Piper), transcription models (Whisper), and local language models (LLM) under a single interface.

---

## 1. Core Model Categories

The hub provides dedicated filter tabs:
- **All:** Comprehensive list across all installed or curated assets.
- **Piper / TTS:** Neural voice models in ONNX format (`.onnx` + `.onnx.json`).
- **Whisper / ASR:** Speech-to-text models in GGML/bin format (`.bin` or directory with `config.json`).
- **LLM:** Quantized language model weights in GGUF format (`.gguf`).
- **Voices:** Audioticon speaker profile descriptions and JSON voice mappings.

---

## 2. Source Sections

1. **Curated / Recommended:**
   - Pre-cataloged models from the Portable Local Studio bundle (5 Piper Polish voices including `pl_PL-darkman-medium` and `pl_PL-gosia-medium`, English voices `en_US-amy-medium` and `en_US-lessac-medium`, whisper base/small models, and Qwen 2.5 3B Instruct GGUF).
   - **Listing is 100% offline.** Metadata (author, size, license, format, files) is embedded in the binary.
2. **Local:**
   - Displays all models discovered on the local filesystem under `$XDG_DATA_HOME/audioticon/models/`.
   - Displays empty catalog guidance when no models exist.
3. **GitHub Releases:**
   - On-demand download from authorized GitHub release assets (`gh:owner/repo@tag#asset`).
4. **Hugging Face:**
   - On-demand download from authorized Hugging Face repositories (`hf:owner/repo@rev#path`).

---

## 3. Model Status Life Cycle

Every local model is classified by the Rust catalog scanner into one of four states:

| Status Badge | Meaning | Can Be Selected as Active? |
|---|---|---|
| **AVAILABLE** | Complete weights, valid config sidecar, checksum matches | **Yes** |
| **MISSING METADATA** | Weights exist, but `.onnx.json` or `config.json` is missing | No (shows diagnostic detail) |
| **CORRUPT** | Empty file (0 B), truncated file (< 1024 B), or SHA-256 mismatch | No (shows error banner) |
| **INCOMPLETE** | Interrupted download or active staging in `.staging/<import-id>` | No (offers removal or resume) |

---

## 4. Model Actions

- **Select active:** Switches the workstation's default voice engine to the chosen model and persists the selection in `$XDG_DATA_HOME/audioticon/model_selection.json`.
- **Clear active:** Reverts selection to the automatic engine resolver.
- **Details:** Opens a Glass modal displaying exact filesystem paths, byte sizes, full SHA-256 hash, licenses, and diagnostic logs.
- **Verify:** Recalculates SHA-256 checksum on disk and compares it to `<weights>.sha256`.
- **Remove:** Deletes the model files from the catalog directory and clears active selection if this model was active.
- **Import local:** Opens a file dialog for `.onnx`, `.gguf`, `.bin`, `.json`, or a `.zip` archive.

---

*Related Pages:*
* [Model Import Security](Model-Import-Security.md)
* [Piper TTS](Piper-TTS.md)
* [XDG Paths and Runtime Data](XDG-Paths-and-Runtime-Data.md)
