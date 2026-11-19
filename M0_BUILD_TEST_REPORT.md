# M0 — Raport buildów i testów Audioticon 2.0

**Data:** 2026-11-18
**Zakres:** inwentaryzacja narzędzi i uruchomienie dostępnych buildów/testów w trzech repozytoriach. **Kod produkcyjny nie był zmieniany** — jedyne zapisane artefakty to ten raport i `M0_ARCHITECTURE_MAP.md`. Buildy wykonano poza drzewami repo (`/tmp/aud-reports/…`), więc working tree każdego repozytorium jest czysty.

## 0. Podsumowanie (TL;DR)

| # | Check | Komenda | Wynik |
|---|---|---|---|
| 1 | Audioticon — kompilacja | `cargo check --workspace --all-targets` | ✅ **exit 0** (tylko ostrzeżenia o nieużywanych polach `[workspace.package]`) |
| 2 | Audioticon — testy | `cargo test --workspace` | ❌ **exit 101**: 127 PASS / **1 FAIL** (`audio::tests::test_export_to_flac_synthetic`), `cli_e2e` 34/34 PASS |
| 3 | Audioticon — formatowanie | `cargo fmt --all -- --check` | ✅ **exit 0** |
| 4 | Portable-Local-Studio — testy | `node --test scripts/tests/` | ⚠️ **exit 1**: 28 PASS / **1 FAIL** — brak `node_modules` (`@tauri-apps/api`) |
| 5 | Portable-Local-Studio — build frontendu | `npm ci && npm run build` | ⛔ **nie uruchomiony** — brak `node_modules`, instalacja wymaga sieci (nieautoryzowana w M0) |
| 6 | ASTRA-UI `agent-ui` — build | `qmake6 agent-console.pro && make -j` | ✅ **exit 0** |
| 7 | ASTRA-UI `agent-ui` — testy | `QT_QPA_PLATFORM=offscreen ./tests` | ✅ **15 passed, 0 failed** (3 979 ms) |
| 8 | ASTRA-UI — testy golden E0/E1 | j.w. w kopii z `src/core` | ℹ️ poza tym klonem (patrz §4.1) |

**Najważniejsze:** kod się kompiluje; jedyny czerwony test w Rust to niezgodność łańcucha `ffmpeg` z wersją 9.0.2, nie błąd logiki Audioticon (§2.2). Dawca nie ma w tym klonie żadnego modelu ani runtime'u, więc jego warstwa AI jest w tym środowisku nieuruchamialna — testowalny jest tylko rdzeń JS.

---

## 1. Inwentarz wymaganych narzędzi

### 1.1 Obecne

| Narzędzie | Wersja | Uwagi |
|---|---|---|
| `cargo` | 1.100.0 (2026-11-05) | spełnia `rust-version = "1.92"` |
| `rustc` | 1.100.0 (2026-11-09) | toolchain `stable-x86_64-unknown-linux-gnu` (jedyny), komponenty clippy/rustfmt w CI |
| `node` | v26.13.1 | wystarcza dla `node --test` w dawcy |
| `npm` | 11.20.0 | |
| `python3` | 3.14.8 (system) | **bez** pakietów `piper`, `onnxruntime`, `kokoro_onnx`, `numpy`, `edge_tts`, `faster_whisper` |
| `qmake6` | Qt **6.12.0** (`/usr/lib`) | `qmake` (Qt5) nie istnieje — trzeba używać `qmake6` |
| `g++`/`gcc` | 16.2.1 | `-std=gnu++17` z `.pro` |
| `make` | obecny | |
| `ffmpeg` | **9.0.2** | `--enable-libsoxr`; enkodery `flac`, `aac`, `libmp3lame` obecne |
| `git`, `jq` | obecne | |

### 1.2 Brakujące (blokujące pełny runtime, nie build)

| Brak | Kto wymaga | Skutek |
|---|---|---|
| `piper` (binarka/venv) | dawca (`PIPER_BIN` → `app/tts-runtime/piper-venv/bin/piper`) | brak jakiejkolwiek syntezy w dawcy |
| `espeak-ng` | `kokoro.rs` (fonemizacja IPA) | Kokoro nie działa; commit `5c07069` dodaje tolerowanie braku w testach |
| `libonnxruntime` w ścieżce loadera | `ort` z `load-dynamic` + `api-31` | natywna inferencja nie wystartuje; w systemie jest tylko `/opt/superwhisper/lib/libonnxruntime.so` (1.24.2, poza `ldconfig`) |
| pakiety Pythona (`piper`, `onnxruntime`, `kokoro_onnx`) | `piper_worker.py`, `kokoro_worker.py` | mostki Python legacy nie działają |
| `sox` | opcjonalne narzędzia DSP | nieużywane przez kod (DSP robi ffmpeg/natywnie) |
| `node_modules` (dawca) | `app/frontend` (`@tauri-apps/api`, React, Vite) | 1 test JS fail; brak builda frontendu |
| modele (Piper/Kokoro/Whisper/LLM) | wszystkie ścieżki AI | **żadna synteza/transkrypcja/LLM nie jest uruchamialna** |

### 1.3 Stan modeli i runtime'ów w klonach

| Katalog | Oczekiwany przez kod | Stan w `~/.openclaw/workspace` |
|---|---|---|
| `Portable-Local-Studio/app/tts-runtime/` | Piper venv + OpenVoice | **brak** |
| `Portable-Local-Studio/app/tts-models/` | głosy Piper, Kokoro | **brak** |
| `Portable-Local-Studio/app/llm-backend/`, `app/llm-models/` | llama.cpp + GGUF | **brak** |
| `Portable-Local-Studio/app/speech-backend/` | whisper.cpp | **brak** |
| `Audioticon-legacy/models` (`dirs::data_dir()/audioticon`) | ONNX | **brak** (`~/.local/share/audioticon` nie istnieje) |

Klon dawcy jest „czysty”: `app/` zawiera wyłącznie `frontend/` (744 KB). Wszystkie katalogi modeli są w `.gitignore`. Drugi, roboczy katalog dawcy (`~/Portable-Local-Studio`, z runtime'ami ~7 GB i modelami PLS) **nie był używany ani modyfikowany** w M0 — jego zawartość pochodzi z wcześniejszych notatek, nie z tego pomiaru.

---

## 2. Audioticon (Rust) — `cargo check` / `test`

### 2.1 Kompilacja

```
cd ~/.openclaw/workspace/Audioticon-legacy
cargo check --workspace --all-targets      → exit 0
```

Skala: ~100 crate'ów zależności skompilowanych z lokalnego cache rejestru (bez pobierania modeli). Jedyny szum to trzy ostrzeżenia:

```
warning: unused field `rust-version` in `workspace.package`
warning: unused field `repository`   in `workspace.package`
warning: unused field `description`  in `workspace.package`
```

To nie błąd: `crates/core` i `crates/cli` dziedziczą tylko `version`, `edition`, `license`, `authors`. Warto ujednolicić, ale nie blokuje.

### 2.2 Testy

```
cargo test --workspace   → exit 101
  unittests src/main.rs (cli)       : 0 passed
  tests/cli_e2e.rs                  : 34 passed, 0 failed
  unittests src/lib.rs (core)       : 127 passed, 1 failed
  cargo fmt --all -- --check        : exit 0
```

Jedyny fail:

```
thread 'audio::tests::test_export_to_flac_synthetic' panicked at crates/core/src/audio.rs:2803
FLAC export failed: Some(Konwersja do FLAC zakończyła się niepowodzeniem: ExitStatus(unix_wait_status(59904)))
```

**Przyczyna (ustalona eksperymentalnie, nie zgadywana).** `exit 234` = ffmpeg kończy pracę, zanim zapisze ramkę. Test woła `export_to_flac(..., bit_depth = 24)`, a ta funkcja buduje filtr:

`aresample=44100:resampler=soxr:precision=24,loudnorm=I=-19:TP=-3.0:LRA=7` (dla 24 bit: `48000`/`precision=28`) i **nie podaje `-ar` po `loudnorm`**.

Odtworzenie poza Rustem (czysty `ffmpeg`, ten sam łańcuch):

```
[flac @ …] invalid block size: 96000
[aost#0:0/flac @ …] Error while opening encoder … Invalid argument
exit 234, output 0 B
```

Kontrola:

| Wariant | Wynik |
|---|---|
| `aresample=48000:resampler=soxr:precision=28` (bez `loudnorm`) | ✅ exit 0, 15 508 B |
| `loudnorm=I=-19:TP=-3.0:LRA=7` + `-ar 48000` | ✅ exit 0, 36 742 B |
| `aresample=…:soxr…,loudnorm=…` (jak w kodzie) | ❌ exit 234, `invalid block size: 96000` |

Czyli: `loudnorm` w ffmpeg 9 pracuje wewnętrznie w 192 kHz i bez ponownego `-ar` przekazuje do enkodera FLAC strumień o niedopuszczalnym rozmiarze bloku. **To niezgodność z wersją ffmpeg, nie brak enkodera** (`flac`, `aac`, `libmp3lame` są w tej instalacji obecne, `libsoxr` też).

Zgodnie z poleceniem „nie zmieniaj kodu produkcyjnego” **nie poprawiałem** `audio.rs` ani testu. Poprawka (do decyzji przy E2/E5) to jedno z: `-ar` po `loudnorm`, `aresample` **po** `loudnorm`, albo rezygnacja z `soxr`/`loudnorm` w eksporcie FLAC. Dopóki tego nie ma, `cargo test --workspace` **nie nadaje się na bramkę regresji**.

### 2.3 Co jeszcze mówią testy

- `cli_e2e` **34/34** — kontrakt CLI (`--help`, `presets`, `script --demo`, render) działa bez modeli w większości przypadków; commit `ee50eca` jawnie toleruje brak wcześniej wyrenderowanego audio, więc testy e2e są zaprojektowane do działania headless.
- `core` 127 testów przechodzi bez żadnego modelu ONNX ani `espeak-ng` — parser, verbalizer, cache, DSP, ledger, importer, presets są testowalne czysto.
- W repo jest 118 `#[test]` w źródłach + testy e2e; rozbieżność z liczbą uruchomień wynika z modułów testowych z wieloma przypadkami.

---

## 3. Portable-Local-Studio — dostępne testy

```
cd ~/.openclaw/workspace/Portable-Local-Studio
node --test scripts/tests/    → exit 1
  tests 29 · pass 28 · fail 1 · duration 80 ms
```

Fail:

```
✖ text-to-image sends seed and sampling settings to sdapi
  Error [ERR_MODULE_NOT_FOUND]: Cannot find package '@tauri-apps/api'
  imported from …/app/frontend/src/services/api.js
```

**Przyczyna:** `scripts/tests/issue-regressions.test.cjs` importuje `app/frontend/src/services/api.js`, który na pierwszej linii robi `import { invoke } from "@tauri-apps/api/core"`. W klonie nie ma `node_modules` (katalog w `.gitignore`), więc test nie może się załadować. To brak zależności, nie regresja kodu. Pozostałe 28 testów (klasyfikacja modeli GGUF/safetensors/whisper `.bin`, walidacja pobrań, regresje issue 82/85/86/87/96) przechodzi **bez żadnej instalacji** — co jest mocną stroną dawcy: `scripts/search` i `tts-directives.cjs` mają zero zależności.

Czego **nie** uruchamiałem i dlaczego:

- `npm ci` / `npm run build` w `app/frontend` — wymaga pobrania ~300 pakietów z sieci; w M0 nie było to potrzebne do oceny architektury, a repo ma pozostać nietknięte.
- `linux.sh` / `serve.cjs` — brak runtime'ów i modeli; start serwera niczego by nie zweryfikował poza „port otwarty”.
- `scripts/setup/*.sh` — pobierają setki MB (Piper, llama.cpp, whisper.cpp); to nie jest praca M0.

---

## 4. ASTRA-UI (`agent-ui`) — build i testy

### 4.1 Build

```
mkdir -p /tmp/aud-reports/build-astra && cd $_
qmake6 ~/.openclaw/workspace/ASTRA-UI-Template/agent-console.pro
make -j$(nproc)          → exit 0
```

Wymagane moduły Qt 6 są w systemie; `agent-console.pro` linkuje `Qt6 QuickControls2/Quick/OpenGL/Gui/Qml/Network/Core` — bez `multimedia`, `sql`, `network` (to dodaje dopiero E0 w drugiej kopii). Build poza drzewem repo, więc `build*/` nie zaśmieca projektu.

`bin/build-agent` **istnieje** i jest w gicie (commit `4252f4f`); `bin/run-agent` ma poprawną składnię (`sh -n`) i woła istniejący skrypt.

### 4.2 Testy

```
qmake6 …/tests/tests.pro && make -j$(nproc)   → exit 0
QT_QPA_PLATFORM=offscreen ./tests              → exit 0
Totals: 15 passed, 0 failed, 0 skipped, 0 blacklisted, 3979 ms
```

Przypadki: `initTestCase`, `catalogAndDefaults`, `toggleMoveReset`, `chartsAxes`, `historyIsBounded`, `unavailableGreysOut`, `persistence`, `stripRoundTrip`, `languageSwitch`, `rendersAndResponds`, `agentTurnStreamsAndShowsTools`, `agentStopHaltsTheStream`, `agentSessionsKeepTheirOwnTranscript`, `agentConsoleRenders`, `cleanupTestCase`.

### 4.3 Ważne zastrzeżenie: to nie jest kopia z E0/E1

Ten klon (`~/.openclaw/workspace/ASTRA-UI-Template`, `4252f4f`, working tree czysty) **nie zawiera** `src/core/`, `core.pri`, `tests/data/tts-golden.json` ani `generate-golden.cjs`. Etapy E0–E1 (golden 44 + 11 przypadków, 17/17 testów) istnieją wyłącznie w drugiej kopii: `~/.config/freebuff-desktop/local-projects/32b70bfb-…/` i są tam **niezacommitowane**.

Konsekwencje dla testowania:

- uruchomienie `qmake6 tests/tests.pro` w tej kopii nie sprawdza parsera ni verbalizera — tylko dashboard i konsolę agenta;
- „zielone 15/15” **nie** potwierdza E1;
- dopóki E0–E1 nie są w jednej kopii, wynik testów zależy od tego, z którego katalogu się je uruchomi. To defekt procesu, nie kodu (§5, poz. 1).

---

## 5. Wykryte braki zależności i problemy — priorytet

| # | Problem | Dowód | Wpływ | Proponowane działanie (poza M0) |
|---|---|---|---|---|
| 1 | Dwie kopie ASTRA, E0–E1 tylko w jednej i niezacommitowane | `git status` / brak `src/core` w `.openclaw` | testy nie odzwierciedlają stanu projektu | commit E0–E1 na `agent-ui`, potem jedna kopia jako źródło prawdy |
| 2 | `cargo test` czerwony na FLAC | 127/128 + odtworzenie ffmpeg | brak bramki regresji dla legacy | poprawka łańcucha `loudnorm`/`-ar` (§2.2) albo oznaczenie testu jako wymagającego ffmpeg ≤ 8/9 z `-ar` |
| 3 | brak `libonnxruntime` w ścieżce loadera | `find / -name libonnxruntime*` → tylko `/opt/superwhisper/lib` | natywny TTS nie ruszy | E2: wskazać `ORT_DYLIB_PATH` albo dołączyć ORT; `audioticon doctor` ma to raportować |
| 4 | brak `espeak-ng` | `command -v espeak-ng` | Kokoro nie działa, część testów pomijana | udokumentować jako zależność opcjonalną (GPL-3.0, granica procesu) |
| 5 | brak `node_modules` w dawcy | test `@tauri-apps/api` | 1 fail, brak builda UI dawcy | `npm ci` tylko jeśli frontend dawcy ma być jeszcze użyty jako referencja UX |
| 6 | brak jakiegokolwiek modelu | §1.3 | zero syntezy/ASR/LLM | pobrać wyłącznie `pl_PL-bass-high` (114 MB) na potrzeby E2 |
| 7 | ostrzeżenia `[workspace.package]` | `cargo check` | kosmetyka | ujednolicić przy E2 |
| 8 | `ffmpeg` 9.0.2 to wersja „future” | `ffmpeg -version` | zachowanie filtrów różni się od CI (ubuntu-latest ma zwykle 6/7) | testy audio nie mogą zależeć od dokładnej wersji ffmpeg — fallback natywny (DSP w C++) jest właściwą odpowiedzią |

---

## 6. Co da się testować bez pobierania dużych modeli

| Obszar | Testowalne bez modeli | Jak / gdzie |
|---|---|---|
| Gramatyka emoji (C++) | ✅ w 100% | `tests/data/tts-golden.json` (44 przypadki) + paleta (11) — kopia z E1 |
| Verbalizer PL (C++, dawca, legacy Rust) | ✅ | golden + 6 testów `verbalizer.rs` |
| Parser Rust (role, style, pauzy, DSP) | ✅ | 15 testów w `parser.rs`, `cargo test -p audioticon-core` |
| Cache zdań (klucz, LRU, statystyki) | ✅ | `cache.rs` 4 testy; w C++ do napisania w E3 |
| Składanie WAV / cisza / pauzy / join | ✅ | `create_silence_wav`, `stitch_wav_files`; dawca: `joinSegments`/`parseWav` |
| DSP (biquad, whisper LPC, leveler, WSOLA) | ✅ | czysta matematyka na syntetycznym WAV — `whisper_dsp`, `studio_leveler`, `wsola` (3+2+1 testów) |
| Eksport MP3/M4B/FLAC + metadane/rozdziały | ⚠️ częściowo | wymaga `ffmpeg`; FLAC 24-bit pada na 9.0.2 (§2.2), MP3/M4B nie były uruchamiane osobno |
| CLI: `--help`, `presets`, `script --demo`, `import`, `cache --stats` | ✅ | `cli_e2e` 34/34 |
| UI ASTRA: layout, sesje, streaming, i18n, strip | ✅ | 15 testów QtTest + `--screenshot` offscreen |
| Synteza Piper (jakość i czas) | ❌ | wymaga `pl_PL-bass-high.onnx` (114 MB) + ORT/Piper |
| Kokoro, Whisper ASR, LLM chat | ❌ | modele 80 MB–4,4 GB + runtime |
| Odsłuch / ocena brzmienia | ❌ | wymaga człowieka — żaden automat tego nie zastąpi |

**Wniosek:** cały pionowy wycinek **do momentu syntezy** jest testowalny bez modeli. Jedyny wydatek na E2 to 114 MB głosu `pl_bass` — i dopiero wtedy potrzebny jest ORT.

---

## 7. Reprodukcja

```bash
# 1. Audioticon
cd ~/.openclaw/workspace/Audioticon-legacy
cargo check --workspace --all-targets ; echo "check=$?"
cargo test  --workspace              ; echo "test=$?"
cargo fmt --all -- --check           ; echo "fmt=$?"

# 2. Portable-Local-Studio
cd ~/.openclaw/workspace/Portable-Local-Studio
node --test scripts/tests/ ; echo "node-tests=$?"

# 3. ASTRA-UI (poza drzewem repo)
mkdir -p /tmp/astra-build /tmp/astra-tests
cd /tmp/astra-build && qmake6 ~/.openclaw/workspace/ASTRA-UI-Template/agent-console.pro && make -j"$(nproc)"
cd /tmp/astra-tests && qmake6 ~/.openclaw/workspace/ASTRA-UI-Template/tests/tests.pro && make -j"$(nproc)"
QT_QPA_PLATFORM=offscreen /tmp/astra-tests/tests ; echo "astra-tests=$?"

# 4. Dowód przyczyny failu FLAC (bez Rusta)
ffmpeg -y -f lavfi -i "sine=frequency=440:duration=0.5" /tmp/t.wav
printf ';FFMETADATA1\n' > /tmp/t.txt
ffmpeg -y -i /tmp/t.wav -i /tmp/t.txt -map_metadata 1 -map 0:a \
  -af "aresample=48000:resampler=soxr:precision=28,loudnorm=I=-19:TP=-3.0:LRA=7" \
  -c:a flac -sample_fmt s32 /tmp/t.flac ; echo "ffmpeg=$?"   # 234, "invalid block size: 96000"
```

---

## 8. Ograniczenia tego raportu

- Nie uruchamiałem dawcy (`linux.sh`, `serve.cjs`) — brak runtime'ów i modeli, więc test byłby bezwartościowy.
- Nie instalowałem zależności (`npm ci`, `pip install`, ORT) — zmieniłoby to środowisko użytkownika poza zakresem M0.
- Nie testowałem syntezy, odsłuchu, transkrypcji ani czatu — brak modeli; jakość głosu pozostaje do oceny przez człowieka.
- Nie zmieniałem kodu produkcyjnego, w tym nie naprawiałem testu FLAC — zgodnie z poleceniem. Przyczyna jest zdiagnozowana i opisana powyżej.
