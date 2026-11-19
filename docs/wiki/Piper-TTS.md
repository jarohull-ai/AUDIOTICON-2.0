# Piper TTS

**Piper** is a fast, local neural text-to-speech system developed by Michael Hansen (Rhasspy / Nabu Casa). It serves as the primary offline synthesis engine in Audioticon 2.0.

---

## 1. Engine Architecture

Piper executes neural acoustic models compiled in ONNX format via ONNX Runtime:
- **Audio Output:** 16-bit PCM WAV (typically 22.05 kHz or 44.1 kHz depending on model fidelity).
- **Execution:** Spawned in background threads inside `backend/crates/core/src/audio.rs`. The GUI thread never blocks on speech synthesis.
- **Phonemization:** Handled locally through `espeak-ng`.

---

## 2. Model Structure

A complete Piper voice model consists of two co-located files in `$XDG_DATA_HOME/audioticon/models/piper/`:
1. `<voice_id>.onnx`: The neural network weights (typically 20 MB to 120 MB).
2. `<voice_id>.onnx.json`: The model configuration file specifying sample rate, phoneme map, speaker IDs, and acoustic parameters.

### Example Polish Models in Audioticon 2.0:
- `pl_PL-darkman-medium`: Deep, authoritative male voice ideal for dark fiction, thriller, and sci-fi narration (CC0 license).
- `pl_PL-gosia-medium`: Clear, natural female voice ideal for classic prose, non-fiction, and dialog (CC0 license).
- `pl_PL-mc_speech-medium`: Expressive voice with broad dynamic range (CC0 license).

---

## 3. Directives & Phoneme Verbalization

Audioticon 2.0 includes a Polish verbalizer (`PolishVerbalizer.cpp`) and directive parser (`DirectiveParser.cpp`) that preprocess text before it reaches Piper:
- **Numbers & Dates:** Full verbalization of Polish cardinal numbers, ordinal numbers, years, percentages, and currencies.
- **Punctuation & Cadence:** Automated comma (clause) and period (sentence) micro-pause injection.
- **Emoji Directives:**
  - `📖`: Storyteller mode (900ms boundary pause)
  - `🛑`: Hard stop (900ms silence)
  - `⏸️`: Clause pause (400ms silence)
  - `😮‍💨`: Breath intake (600ms pause)
  - `🐢`: Slow speech (0.85x length scale)
  - `🐇`: Fast speech (1.15x length scale)
  - `🎙️`: Neutral speech reset

---

*Related Pages:*
* [Model Hub](Model-Hub.md)
* [Architecture](Architecture.md)
* [Licensing](Licensing.md)
