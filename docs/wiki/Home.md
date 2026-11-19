# Home

Welcome to the **Audioticon 2.0 Documentation Wiki**.

Audioticon 2.0 is a professional, local-first Linux workstation for audiobook directing, neural speech synthesis, multi-track audio playback, and broadcast audio mastering. It combines a high-performance **Rust domain runtime** with an **uncluttered Qt Quick / C++17 Director Workstation interface**.

---

## Key Principles

- **100% Offline & Local-First:** No background HTTP daemons, no WebViews (`QWebEngineView`), no telemetry, and no mandatory cloud connectivity.
- **Memory Safety & Zero Unchecked ABI Drift:** C++ talks to Rust strictly via `cxx` typed DTOs and value semantics.
- **Accurate Diagnostic Truth:** No false success states. Operations without a voice model fail with explicit diagnostics rather than generating dummy audio or claiming completion.
- **Director Workstation Design Language:** Dark liquid glass visuals, adaptive layout across `780×560`, `1180×850`, and `1920×1080` screen resolutions, and full Polish/English internationalization.

---

## Quick Navigation

| Topic | Description |
|---|---|
| [Overview](Overview.md) | High-level summary of capabilities, philosophy, and history |
| [Architecture](Architecture.md) | In-depth breakdown of Rust domain core, C++ FFI, and Qt Quick UI |
| [Build Guide](Build-Guide.md) | Step-by-step compilation instructions for Linux |
| [Model Hub](Model-Hub.md) | Offline model catalog, curated presets, and on-demand download flow |
| [Model Import Security](Model-Import-Security.md) | Sandbox isolation, checksum hashing, and explicit consent modals |
| [Testing](Testing.md) | Executing the 221 Rust tests and 40 offscreen Qt tests |
| [Roadmap](Roadmap.md) | Milestone status (M0–M19 completed; M20+ upcoming) |

---

*Current Milestone: **M19 (Model Hub UI & Staging Integration)***
*Repository: [github.com/jarohull-ai/AUDIOTICON-2.0](https://github.com/jarohull-ai/AUDIOTICON-2.0)*
