# Build Guide

This document describes how to compile Audioticon 2.0 from source on modern Linux distributions (Arch Linux, Ubuntu 22.04/24.04, Fedora, Debian 12).

---

## 1. System Dependencies

### Required Toolchains:
- **Rust:** Stable 1.80+ (`rustc`, `cargo`)
- **C++ Compiler:** GCC 11+ or Clang 14+ supporting C++17
- **Qt 6:** Qt 6.5 or later (Core, GUI, Quick, QuickControls2, Multimedia, Test, Network, OpenGL)
- **Build Tools:** `qmake6` (or `qmake` pointing to Qt 6), `make`, `pkg-config`

### Runtime Audio Utilities:
- **FFmpeg:** Version 7.x or 9.x (`ffmpeg`, `ffprobe`)
- **SoX:** Sound eXchange with format libraries (`sox`, `libsox-fmt-all`)
- **Piper TTS Engine:** Local executable (usually in `/usr/bin/piper` or `~/.local/bin/piper`)
- **espeak-ng:** Phonemizer dependency for voice models

### Package Manager Commands

#### Arch Linux / Omarchy:
```bash
sudo pacman -S --needed base-devel rust qt6-base qt6-declarative qt6-multimedia \
  qt6-quickcontrols2 ffmpeg sox espeak-ng
```

#### Ubuntu 24.04 LTS / Debian 12:
```bash
sudo apt-get update
sudo apt-get install -y build-essential curl pkg-config \
  qt6-base-dev qt6-declarative-dev qt6-multimedia-dev libgl1-mesa-dev libglx-dev \
  ffmpeg sox libsox-fmt-all espeak-ng
```

---

## 2. Compilation Sequence

Audioticon 2.0 requires the Rust bridge crate to be compiled into a static library (`libaudioticon_bridge.a`) before the Qt application links against it.

### Step 1: Clone the Repository
```bash
git clone https://github.com/jarohull-ai/AUDIOTICON-2.0.git
cd AUDIOTICON-2.0
```

### Step 2: Build the Rust Backend
```bash
cd backend
cargo build --release --package audioticon-bridge
cd ..
```
*Output artifact:* `backend/target/release/libaudioticon_bridge.a`

### Step 3: Build the Qt User Interface
```bash
cd ui
mkdir -p build-console
cd build-console
qmake6 ../agent-console.pro
make -j$(nproc)
cd ../..
```
*Output binary:* `ui/build-console/astra-agent-console`

---

## 3. Launching the Application

Ensure your display server is active (Wayland or X11):

```bash
# Launch the Director Workstation
./ui/build-console/astra-agent-console
```

For Wayland environments (e.g. Hyprland / Omarchy):
```bash
QT_QPA_PLATFORM=wayland ./ui/build-console/astra-agent-console
```

---

*Related Pages:*
* [Development Setup](Development-Setup.md)
* [Testing](Testing.md)
* [Troubleshooting](Troubleshooting.md)
