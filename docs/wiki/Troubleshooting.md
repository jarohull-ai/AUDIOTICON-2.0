# Troubleshooting

This guide addresses common setup issues, audio pipeline errors, and environment diagnostics in Audioticon 2.0.

---

## 1. Running the Built-In Doctor Diagnostics

Audioticon 2.0 features an integrated environment diagnostic utility accessible via **`Ctrl+7`** (or by clicking the **🩺 Doctor** tab in the navigation rail).

The Doctor view inspects:
- **Piper TTS binary:** Checks `$PATH` or custom location.
- **FFmpeg 7+ / 9+:** Validates audio transcoders and `loudnorm` filter availability.
- **SoX:** Verifies optional audio effects and format libraries.
- **espeak-ng:** Ensures phoneme libraries are installed.
- **Voice Models:** Confirms at least one valid voice model is available in the catalog.
- **XDG Write Permissions:** Confirms write access to data and cache directories.

If issues are found, the Doctor view provides exact repair commands for your Linux distribution.

---

## 2. Common Issues & Solutions

### A. "Brak modelu głosu" / "No voice model selected"
- **Cause:** No voice model exists in `$XDG_DATA_HOME/audioticon/models/piper/` or none has been selected as active.
- **Solution:**
  1. Open the Model Hub with **`Ctrl+8`**.
  2. Switch to the **Curated / Recommended** tab.
  3. Click **Download…** next to a recommended voice (e.g. `pl_PL-darkman-medium`).
  4. Confirm the network consent dialog. Once downloaded, click **Select active**.

### B. "Nie można odtworzyć dźwięku" / QAudioSink Errors
- **Cause:** No functional audio output sink found, or PulseAudio/PipeWire daemon is stalled.
- **Solution:**
  1. Verify PipeWire / PulseAudio is running: `pactl info`
  2. Verify your user is in the `audio` group: `groups`
  3. Test playback with standard utilities: `pw-play /path/to/test.wav` or `aplay /path/to/test.wav`

### C. Wayland Display or OpenGL Warning
- **Symptom:** Terminal outputs `radv is not a conformant Vulkan implementation` or black window on Wayland.
- **Solution:** Run with software Quick backend or force Wayland QPA:
  ```bash
  QT_QPA_PLATFORM=wayland ./ui/build-console/astra-agent-console
  # or if GPU drivers are misconfigured:
  QT_QUICK_BACKEND=software ./ui/build-console/astra-agent-console
  ```

### D. Corrupt Model Status in Model Hub
- **Symptom:** Model card shows red `CORRUPT` badge.
- **Solution:** Click **Details** to see the diagnostic reason. Typically, the download was interrupted (file < 1024 bytes) or the SHA-256 checksum mismatched. Click **Remove** and import or download the model again.

---

*Related Pages:*
* [Build Guide](Build-Guide.md)
* [Model Hub](Model-Hub.md)
* [XDG Paths and Runtime Data](XDG-Paths-and-Runtime-Data.md)
