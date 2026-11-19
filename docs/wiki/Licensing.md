# Licensing

Audioticon 2.0 is an open-source project committed to clear license hygiene and transparent attribution.

---

## 1. Application Codebase

| Component | Path | License | Copyright |
|---|---|---|---|
| **Root Workspace** | `/` | [MIT](https://opensource.org/licenses/MIT) | © 2026 jarohull-ai |
| **Backend Crates** | `backend/` | [MIT](https://opensource.org/licenses/MIT) / [Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0) | Dual licensed |
| **Qt Quick Front-End** | `ui/` | [MIT](https://opensource.org/licenses/MIT) | © 2026 Jaro (Shell adapted from Omarchy) |

---

## 2. Voice Models & Commercial Publishing

A critical concern for audiobook producers is whether an audio file generated with a neural TTS model can be legally distributed or sold on commercial platforms (e.g. Audible, Spotify, Apple Books, Storytel).

Audioticon 2.0 classifies models into three license tiers in the Model Hub:

### Tier 1: Commercial-Safe (Unrestricted)
These models are built on public domain or permissive open-source audio datasets:
- **`pl_PL-darkman-medium`:** CC0 (Public Domain)
- **`pl_PL-gosia-medium`:** CC0 (Public Domain)
- **`pl_PL-mc_speech-medium`:** CC0 (Public Domain)
- **`en_US-amy-medium`:** CC0 (Public Domain)
- **`en_US-ryan-medium`:** CC0 (Public Domain)
- **`de_DE-thorsten-medium`:** CC0 (Public Domain)
- **Kokoro-82M:** Apache License 2.0

*You may sell, broadcast, and distribute audio produced with Tier 1 models without royalty obligations.*

### Tier 2: Attribution Required (CC-BY-4.0)
- **`pl_PL-mls_6892-low`:** Requires prominent attribution in book credits (e.g. *"Synthesized using Multilingual LibriSpeech voice model under CC-BY-4.0"*).

### Tier 3: Non-Commercial / Research Only
- **`en_US-lessac-medium`:** Blizzard Challenge 2013 research dataset. Commercial distribution of audiobooks synthesized with this voice is strictly prohibited by upstream dataset terms.

*Always review the license displayed on each Model Card in Audioticon's Model Hub before mastering commercial projects.*

---

*Related Pages:*
* [Third-Party Notices](https://github.com/jarohull-ai/AUDIOTICON-2.0/blob/main/THIRD_PARTY_NOTICES.md)
* [Piper TTS](Piper-TTS.md)
* [Model Hub](Model-Hub.md)
