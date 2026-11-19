# Development Setup

This guide provides practical workflow instructions for developers hacking on the Rust domain core, C++ bridge layer, or QML interface.

---

## 1. Project Directory Layout

```
.
├── backend/                  # Rust workspace
│   ├── crates/
│   │   ├── core/             # libaudioticon_core (Catalog, Staging, Audio, DSP)
│   │   ├── bridge/           # libaudioticon_bridge (cxx bridge, TaskRegistry)
│   │   └── cli/              # Headless CLI utility
│   ├── Cargo.toml            # Workspace root
│   └── Cargo.lock
├── ui/                       # Qt Quick / C++17 frontend
│   ├── src/
│   │   ├── agent/            # Application entrypoint (main.cpp, AgentModel)
│   │   ├── bridge/           # AudioticonCore.{h,cpp}, StudioBridge.{h,cpp}
│   │   ├── audio/            # AudioPlayer.{h,cpp} (QAudioSink stream)
│   │   ├── wallpaper/        # WallpaperModel.{h,cpp} (Glass background)
│   │   ├── doctor/           # DoctorModel.{h,cpp} (Diagnostics engine)
│   │   ├── qml/              # QML visual components (AgentMain, ModelHub, Studio)
│   │   └── i18n/             # en.json and pl.json translation dictionaries
│   ├── tests/                # Offscreen QtTest regression suite (tests.cpp)
│   └── agent-console.pro     # qmake6 project specification
└── docs/                     # Documentation, Wiki, and screenshots
```

---

## 2. Iterative Development Cycle

### Hacking on the Rust Backend
When making changes inside `backend/crates/core` or `backend/crates/bridge`:

```bash
cd backend
# Format checking
cargo fmt --all -- --check

# Strict Clippy linter
cargo clippy --workspace --all-targets -- -D warnings

# Execute backend tests
cargo test --workspace

# Rebuild bridge static library for the UI
cargo build --release --package audioticon-bridge
```

### Hacking on Qt / QML
When editing QML files in `ui/src/qml/` or C++ models:

```bash
cd ui/build-tests
make -j$(nproc)
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./tests
```
*Tip: Running tests with `QT_QPA_PLATFORM=offscreen` allows complete UI validation in headless terminals or CI pipelines without opening physical X11/Wayland windows.*

---

## 3. Working with Translations (`i18n`)

If you introduce any new user-facing strings:
1. In QML: Use `qsTr("English string")`. Never use Polish accents directly in source QML keys.
2. In C++: Use `QCoreApplication::translate("Context", "English string")`.
3. Add the key and Polish translation to both:
   - `ui/src/i18n/en.json` (value = key)
   - `ui/src/i18n/pl.json` (value = Polish translation)
4. Verify complete dictionary alignment:
   ```bash
   python3 -c '
   import json
   en = json.load(open("ui/src/i18n/en.json"))
   pl = json.load(open("ui/src/i18n/pl.json"))
   assert set(en.keys()) == set(pl.keys()), "Dictionaries drifted!"
   '
   ```

---

*Related Pages:*
* [Testing](Testing.md)
* [Architecture](Architecture.md)
* [Contributing](Contributing.md)
