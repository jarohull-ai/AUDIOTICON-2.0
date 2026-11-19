# VoiceLab (Planned Integration Design)

> **Status:** Planned / Navigation placeholder in UI (`Ctrl+3`).
> *Full speaker assignment and voice cloning runtime is not yet active in the current build.*

---

## 1. Goal & Requirements

Audiobooks with multiple characters require distinct vocal identities:
- **Narrator:** Calm, neutral, consistent baseline cadence.
- **Protagonist / Characters:** Specific pitch modulation, distinct Piper models, or custom voice profiles.

**VoiceLab** (`Ctrl+3`) is planned as the character cast management studio for Audioticon 2.0.

---

## 2. Planned Features

1. **Character Roster:**
   - Define character names (e.g. *Narrator*, *Detective Kowalski*, *Elena*).
   - Assign primary neural voice models from the Model Hub.
   - Configure default pitch shifts, speed rates, and DSP filters (e.g. telephone `📱`, cathedral `🏛️`, whisper `🤫`).
2. **Profile File Format:**
   - Stored as standard JSON files under `$XDG_DATA_HOME/audioticon/models/voices/<character>.json`.
   - The M17–M19 catalog already recognizes `voice` kind profiles in this path.
3. **Voice Cloning & Custom Voice Policy:**
   - Voice cloning (zero-shot TTS) involves strict ethical and licensing considerations.
   - Any future voice adaptation feature must operate **100% locally** and require explicit consent documentation to prevent synthetic voice abuse. Cloud voice services (e.g. ElevenLabs) will not be embedded by default to protect the local-first security boundary.

---

*Related Pages:*
* [Model Hub](Model-Hub.md)
* [Piper TTS](Piper-TTS.md)
* [Roadmap](Roadmap.md)
