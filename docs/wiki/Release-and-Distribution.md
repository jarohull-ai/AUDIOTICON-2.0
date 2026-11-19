# Release and Distribution

This page details packaging, local release bundling, and distribution mechanics for Audioticon 2.0.

---

## 1. Portable Local Studio Bundle

Audioticon 2.0 can be packaged into a standalone, portable desktop distribution directory for Linux:

```
audioticon-2.0-linux-x86_64/
├── bin/
│   ├── astra-agent-console     # Standalone Qt Quick binary
│   └── audioticon-cli          # Headless CLI utility
├── lib/
│   └── (System libraries for isolated distributions)
├── share/
│   ├── audioticon/
│   │   └── models/             # Pre-seeded models (optional)
│   └── applications/
│       └── audioticon.desktop  # Desktop launcher entry
└── run.sh                      # Portable launcher script
```

---

## 2. Desktop Integration (`.desktop`)

To integrate Audioticon 2.0 into desktop application menus (GNOME, KDE Plasma, Hyprland, Sway):

```ini
[Desktop Entry]
Name=Audioticon 2.0
Comment=Audiobook Directing and Voice Synthesis Workstation
Exec=/usr/local/bin/astra-agent-console
Icon=audioticon
Terminal=false
Type=Application
Categories=AudioVideo;Audio;AudioVideoEditing;Qt;
StartupWMClass=astra-agent-console
```

---

## 3. Sandboxing & Offline Execution

Audioticon 2.0 is designed to operate safely inside strict namespaces or bubblewrap sandboxes:

```bash
# Example unshare execution ensuring zero network access:
unshare -n ./ui/build-console/astra-agent-console
```

When network access is blocked by the OS or container, Audioticon 2.0 gracefully displays `NetworkUnavailable` in the Model Hub when attempting to download remote models, while local rendering, playback, and editing remain 100% operational.

---

*Related Pages:*
* [Build Guide](Build-Guide.md)
* [XDG Paths and Runtime Data](XDG-Paths-and-Runtime-Data.md)
* [Model Import Security](Model-Import-Security.md)
