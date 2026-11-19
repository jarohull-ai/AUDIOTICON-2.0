# Audioticon 2.0 — architecture and pinned decisions

This file is the durable copy of the decisions taken before the first line of
code. If a later session changes one of them, change it here in the same commit.

## Product shape (not up for grabs)

1. **Local desktop application + CLI.** No web UI, no server that exists to serve
   the app, no browser wrapper. Qt 6 / QML / C++ is the shell.
2. **Layout:** module list on the left, chat or working area in the centre,
   runtime panel on the right. No everything-at-once dashboard.
3. **Chat:** two independent threads with local models ("variant A"), plus
   "Read in the studio" to push an answer into the voice workspace.
4. **UI base:** ASTRA UI Template, branch `agent-ui` (commit 4252f4f at the
   time of writing). The `main` branch is the telemetry template, not this app.
5. **Donor:** Portable Local Studio is frozen. It is an oracle for tests only —
   no new features there, no second parallel application.
6. **Vertical slice:** Studio UI → emoji direction → Piper → sentence cache →
   playback. Chat, VoiceLab, SFX and export come after it.
7. Everything else is decided while building, without blocking a stage.

## Working defaults

- DSP is native C++ (biquad/gain). ffmpeg only enters if MP3 export needs it.
- One binary, `audioticon`, with subcommands: `speak`, `voices`, `install-voice`,
  `models`, `chat`, `doctor`.

## Technical facts established by inspection

- The local clone was on `main` (`astraui.pro`, `TARGET = astraui-demo`,
  `src/astra/*`) — a telemetry template. The agent console lives on `agent-ui`:
  `agent-console.pro`, `TARGET = astra-agent-console`,
  `src/agent/{main,agentmodel}.cpp`, `src/qml/{AgentMain,ChatBubble,StateStrip}.qml`.
- The console shipped without `multimedia`, `sql` and `network`; they are now in
  `agent-console.pro` because the voice stage needs audio, history and model
  downloads.
- Donor measurements: `serve.cjs` 8 435 lines, `tts-directives.cjs` 641 lines
  with no dependencies, Polish voices ~240 MB, `llm-backend` 1.3 GB,
  `llm-models` 4.4 GB, `tts-runtime` 7.1 GB (OpenVoice — **not** transplanted).
- Directive grammar: 📖 900 ms · 🛑 900 ms · ⏸️ 400 ms · 😮‍💨 600 ms · 🐢 ×0.85 ·
  🐇 ×1.15 · 🎙️ reset · 📻/📱/🏛️/🤫 acoustic plan. Emoji outside the grammar are
  dropped; text without letters or digits never reaches the engine (that was the
  `wave.Error: # channels not specified` crash).
- Slice targets: five segments ≈ 5.6 s on the first run, a repeat under 50 ms
  with 5/5 cache hits.

## Module map

```
src/
├── agent/        session host (agentmodel.*), to be wired to the LLM host later
├── core/         libaudioticon_core — no QML, no UI, shared with the CLI
│   ├── voice/    DirectiveParser, PolishVerbalizer, (PiperEngine, SegmentCache,
│   │             WavIo, Dsp, RenderJob — stages E2/E3)
│   ├── llm/      BackendProbe, LlamaServer, ModelCatalog, Downloader, ChatSession
│   ├── store/    SQLite: sessions, threads, messages, outputs, settings
│   └── cli/      subcommands
├── qml/          AgentMain + ChatBubble + StateStrip, plus StudioView and friends
└── translator.*  i18n (pl/en)
```

`src/core/core.pri` is the single list of core sources; the app and the test
target both include it, so the CLI and the GUI can never drift apart.

## Stage status

- **E0 — base:** done. Branch `agent-ui`, Qt modules added, XDG data/cache dirs in
  `core/Paths`.
- **E1 — text and direction:** `DirectiveParser` + `PolishVerbalizer` ported to
  C++, verified against `tests/data/tts-golden.json` generated from the donor.
- **E2–E5:** Piper, render/cache, Studio UI, CLI.
