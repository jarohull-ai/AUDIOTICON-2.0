# Contributing to Audioticon 2.0

Thank you for your interest in contributing to **Audioticon 2.0**! We welcome bug fixes, documentation improvements, architectural reviews, and test additions.

---

## 1. Guiding Architectural Principles

Before contributing code, please review the core design constraints outlined in [M15_PRODUCT_DIRECTION_RESET.md](M15_PRODUCT_DIRECTION_RESET.md):

1. **Local-First & 100% Offline by Default:**
   - No HTTP daemon processes, no telemetry, and no embedded WebViews (`QWebEngineView`).
   - Network access is permitted exclusively for explicit, user-initiated model downloads, guarded by explicit consent modals.
2. **Domain Core in Rust, UI in Native Qt Quick / C++17:**
   - Domain logic (audio pipelines, job queues, catalog scanners, staging, hashing) belongs in `backend/crates/`.
   - The UI layer (`ui/`) consumes the backend strictly through the `cxx` FFI bridge (`AudioticonCore.{h,cpp}` and `StudioBridge.{h,cpp}`).
3. **No False Success or Placeholders as Finished Modules:**
   - Any operation that fails or lacks an active model must return a clear diagnostic state (`Failed`, `unavailable`, `missing_metadata`, or `corrupt`), never a premature `Finished` or dummy output.
4. **Director Workstation Design Language:**
   - Retain the liquid glass dark interface, three responsive breakpoints (`780×560`, `1180×850`, and `1920×1080`), and non-clipped wrapping toolbars (`Flow`).

---

## 2. Development Setup

### System Prerequisites (Linux)
- **Rust:** Stable toolchain (`rustup default stable`) with `clippy` and `rustfmt`.
- **Qt 6:** Qt 6.5+ (Qt Core, GUI, Quick, QuickControls2, Multimedia, Test, Network).
- **Compilers:** `g++` (C++17 support) and `qmake6`.
- **Audio & Media Utilities:** `ffmpeg` (version 7+ or 9+), `sox`, `libsox-fmt-all`, `espeak-ng`.

### Recommended Build Commands

```bash
# 1. Run all backend tests and lints
cd backend
cargo fmt --all -- --check
cargo clippy --workspace --all-targets -- -D warnings
cargo test --workspace

# 2. Build the bridge static library
cargo build --release --package audioticon-bridge

# 3. Build and execute Qt test suite (offscreen QPA)
cd ../ui
mkdir -p build-tests && cd build-tests
qmake6 ../tests/tests.pro
make -j$(nproc)
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./tests

# 4. Build standalone desktop console
cd ../
mkdir -p build-console && cd build-console
qmake6 ../agent-console.pro
make -j$(nproc)
```

---

## 3. Pull Request Guidelines

1. **Create a Topic Branch:**
   Branch off `main` (or the active milestone working branch) with a descriptive name, e.g., `feat/tts-parameters`, `fix/inspector-scroll`, `docs/wiki-update`.
2. **Commit Conventions:**
   Follow Conventional Commits (`feat:`, `fix:`, `docs:`, `refactor:`, `test:`, `chore:`).
3. **Keep Tests Green:**
   Every PR must pass both:
   - `cargo test --workspace` (221+ tests)
   - `ui/build-tests/tests` (40+ Qt tests)
4. **Add Regression Tests:**
   If fixing an issue or adding a feature, add corresponding assertions to `backend/crates/` unit tests or `ui/tests/tests.cpp`.
5. **Update Translations:**
   If adding user-visible QML strings or C++ error messages, use `qsTr("...")` or `QCoreApplication::translate("...", "...")` with English source keys and update both `ui/src/i18n/en.json` and `ui/src/i18n/pl.json`.

---

## 4. Code of Conduct

This project is governed by the [Contributor Covenant Code of Conduct](CODE_OF_CONDUCT.md). Please treat all contributors with respect, professionalism, and constructive feedback.
