# XDG Paths and Runtime Data

Audioticon 2.0 strictly follows the **XDG Base Directory Specification** on Linux. It does not clutter the user's `$HOME` root directory.

---

## 1. Directory Structure

| Category | Environment Variable | Default Location | Contents |
|---|---|---|---|
| **Data** | `$XDG_DATA_HOME` | `~/.local/share/audioticon/` | Model catalog, installed weights, active selection |
| **Config** | `$XDG_CONFIG_HOME` | `~/.config/audioticon/` | User preferences, custom directives (`audioticon.toml`) |
| **Cache** | `$XDG_CACHE_HOME` | `~/.cache/audioticon/` | Rendered sentence audio cache, spectrogram thumbnails |

---

## 2. Model Catalog Layout (`$XDG_DATA_HOME/audioticon/`)

```
~/.local/share/audioticon/
├── model_selection.json            # Persisted active model selection
└── models/
    ├── .staging/                   # Temporary atomic import & download workspace
    │   └── <import-id>/
    │       ├── intent.json         # Manifest of ongoing import
    │       └── <staged-files>
    ├── piper/                      # Piper TTS ONNX models
    │   ├── pl_PL-darkman-medium.onnx
    │   ├── pl_PL-darkman-medium.onnx.json
    │   └── pl_PL-darkman-medium.onnx.sha256
    ├── whisper/                    # Whisper speech-to-text models
    │   └── ggml-base.bin
    ├── llm/                        # Quantized local language models
    │   └── qwen2.5-3b-instruct-q4.gguf
    └── voices/                     # Character voice profiles
        └── narrator.json
```

---

## 3. Active Selection Persistence (`model_selection.json`)

When a model is chosen in the Model Hub or via FFI (`select_model`), Audioticon 2.0 saves an atomic JSON file:

```json
{
  "active_id": "piper/pl_PL-darkman-medium",
  "active_kind": "piper_tts",
  "updated_at": "2026-11-19T08:00:00Z"
}
```

If the referenced file is subsequently deleted from disk, the catalog scanner detects this during boot, reports `unavailable` with an on-screen warning, and prevents runtime crashes.

---

## 4. Cache Management (`$XDG_CACHE_HOME/audioticon/`)

The audio cache stores rendered speech segments to eliminate redundant synthesis during editing:
- Every segment is hashed by verbalized text, voice model identifier, and audio rate.
- Cache hits skip neural network synthesis and return the cached audio segment in under 5 ms.

---

*Related Pages:*
* [Model Hub](Model-Hub.md)
* [Architecture](Architecture.md)
* [Release and Distribution](Release-and-Distribution.md)
