# Testing

Audioticon 2.0 employs a dual-tier testing methodology:
1. **Rust Unit & Property Tests (`backend`):** Validating domain logic, catalog integrity, checksums, archive safety, and IPC serialization.
2. **QtTest Offscreen Regression Suite (`ui`):** Validating QML rendering, responsive layout constraints, dialog flows, FFI link proofs, and translation completeness.

---

## 1. Rust Test Suite (221 Tests)

The backend test suite runs with standard cargo tooling:

```bash
cd backend
cargo test --workspace --verbose
```

### Coverage Areas:
- **`model_catalog.rs` (12 tests):**
  - Empty catalog handling without panics.
  - Correct model identification (`piper_tts`, `whisper_asr`, `local_llm`, `voice`).
  - Corrupt weight detection (0 bytes, truncated < 1024 bytes, checksum mismatches).
  - Missing metadata detection (ONNX weight without `.onnx.json`).
  - Selection persistence in `model_selection.json` and cleanup.
- **`model_import.rs` (22 tests):**
  - Curated preset listing without opening network sockets.
  - Atomic directory staging and intent persistence.
  - Path traversal defense (`../` in ZIP archives).
  - Rejection of executable binaries (`ELF`, `MZ`, `.sh`, `.py`).
  - Immediate cancellation cleanup.
- **`bridge` (23 tests):**
  - CXX struct translation and string conversions.
  - Asynchronous task registry lifecycle (Queued -> Synthesizing -> Finished / Failed).

---

## 2. QtTest Suite (40 Tests)

The UI test binary (`ui/build-tests/tests`) compiles against the real `libaudioticon_bridge.a` static library, verifying real FFI linkage:

```bash
cd ui/build-tests
qmake6 ../tests/tests.pro
make -j$(nproc)
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./tests
```

### Key UI Test Cases:
- **`ffiLinksRustCore`:** Asserts that `coreSampleRate()` returns `44100` from the compiled Rust staticlib.
- **`firstRunNoOverflowAt780x560`:** Instantiates `FirstRunWizard` at `780×560`, `1180×850`, and `1920×1080`, asserting that action buttons and footers are strictly within the screen boundary.
- **`i18nSwitchPlEnAndRestartPersistence`:** Verifies dynamic language switching between Polish and English without restart, and tests `QSettings` persistence.
- **`glassOpacityUnifiedAndWallpaperVisibility`:** Verifies that wallpaper images show through panels with consistent alpha values.
- **`modelCatalogFfiContract`:** Tests model selection, corrupt model rejection, and B13 failure guarantees (no voice model produces `Failed`, never `Finished`).
- **`modelHubUiEmptyCuratedAndConsentModal`:** Tests Model Hub empty state, curated presets, type filtering, and the explicit network consent modal.
- **`modelHubLocalStatesCancelAndScreenshots`:** Exercises all 4 model states (`available`, `corrupt`, `missing_metadata`, `incomplete`) and generates offscreen screenshot captures.

---

## 3. Running Headless in CI

Both test suites run entirely headless:

```bash
# In an environment with no physical display:
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./ui/build-tests/tests
```

---

*Related Pages:*
* [Build Guide](Build-Guide.md)
* [Development Setup](Development-Setup.md)
* [Contributing](Contributing.md)
