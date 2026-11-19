# Director Workstation UX

The **Director Workstation** design language is an ergonomically tuned, dark liquid-glass interface created specifically for sound designers, audiobook directors, and audio narrators.

---

## 1. Visual Philosophy: Liquid Glass

Rather than an opaque dark box, Audioticon 2.0 uses multi-layered translucent surfaces:
- **Unified Alpha Parameter (`glassOpacity`):** Controlled by a single slider in `Settings ⚙` (ranging from `0.20` to `0.95`). All panels, tiles, and headers inherit this alpha dynamically.
- **Dynamic Wallpapers:** The interface supports solid backdrops, static images (`.jpg`, `.png`), video loops via Qt Multimedia, and rotating local wallpaper collections.
- **Deleted Asset Fallback:** If a user-selected wallpaper file is deleted or unmounted, `WallpaperModel::validateSource()` safely detects the failure and restores the embedded default wallpaper (`qrc:/assets/wallpapers/pexels-artyusufpatel-8998289.jpg`) with an on-screen notice.

---

## 2. Responsive Breakpoint Matrix

The workstation layout scales fluidly without UI clipping or overflowing buttons:

| Resolution | NavRail State | Inspector Drawer | Toolbar / Chip Rows |
|---|---|---|---|
| **780 × 560** (Minimum) | Collapsed (56px) with icon tooltips | Floating overlay drawer with scrim | Wrapped in `Flow`, zero clipping |
| **1180 × 850** (Default) | Expanded (176px) with labels and keys | Docked right panel (pinned or unpinned) | Side-by-side grids and control bars |
| **1920 × 1080** (Full HD) | Expanded (176px) with full labels | Fully expanded context inspector | Generous working canvas, spacious tiles |

---

## 3. Left Navigation Rail (`NavRail.qml`)

Direct keyboard shortcuts allow instant switching between views:
- **`Ctrl+1` Studio:** Script editor, segment timeline, transport controls, and directive palette.
- **`Ctrl+2` AI Chat:** Conversational session history, transcription inspector, and prompt console.
- **`Ctrl+3` VoiceLab:** Character voice assignment and speaker profiling *(Planned, currently placeholder)*.
- **`Ctrl+4` Projects:** Multi-chapter audiobook project tree *(Planned, currently placeholder)*.
- **`Ctrl+5` Foley/SFX:** Sound effects and ambience library *(Planned, currently placeholder)*.
- **`Ctrl+6` ACX Ledger:** Broadcast loudness and ACX compliance ledger *(Planned, currently placeholder)*.
- **`Ctrl+7` Doctor:** System health, dependency detection, and offline verification.
- **`Ctrl+8` Models:** Native Model Hub for local and remote voice/language models.

---

## 4. Internationalization (PL / EN)

- 100% of user interface strings, buttons, tooltips, and C++ diagnostics pass through `qsTr(...)` or `QCoreApplication::translate(...)`.
- Translation catalogs (`ui/src/i18n/en.json` and `ui/src/i18n/pl.json`) contain **503 verified keys** with zero missing definitions.
- Language switching is instantaneous in the First Run Wizard or Settings drawer, persisting across application restarts via `QSettings`.

---

*Related Pages:*
* [Overview](Overview.md)
* [Model Hub](Model-Hub.md)
* [Troubleshooting](Troubleshooting.md)
