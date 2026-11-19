# Contributing

Please refer to the repository's root [CONTRIBUTING.md](https://github.com/jarohull-ai/AUDIOTICON-2.0/blob/main/CONTRIBUTING.md) for full instructions on branching, coding standards, and pull requests.

---

## Key Rules for Contributors

1. **No External HTTP Servers in the Codebase:**
   Audioticon 2.0 is an offline-first desktop application. Do not introduce background HTTP, REST, or WebSocket listener daemons.
2. **Safe FFI Boundary:**
   Keep all `cxx` includes isolated in `ui/src/bridge/AudioticonCore.cpp`. Do not leak Rust headers into other C++ units or QML.
3. **No Unchecked False Success:**
   If a model, executable, or file is missing, fail explicitly with descriptive error states.
4. **All Tests Must Pass:**
   Before opening a PR, ensure both test suites succeed:
   - Backend: `cd backend && cargo test --workspace`
   - Frontend: `cd ui/build-tests && ./tests`
5. **Update Both Translation Files:**
   When introducing user-facing text, supply matching entries in both `ui/src/i18n/en.json` and `ui/src/i18n/pl.json`.

---

*Related Pages:*
* [Development Setup](Development-Setup.md)
* [Testing](Testing.md)
* [Security Policy](Security-Policy.md)
