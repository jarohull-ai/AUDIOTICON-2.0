# M0 — Mapa architektury Audioticon 2.0

**Data:** 2026-11-18
**Zakres:** analiza read-only trzech lokalnych repozytoriów. **Kod produkcyjny nie został zmieniony** — powstały wyłącznie raporty M0.
**Metoda:** inwentaryzacja plików, odczyt manifestów i kluczowych modułów, zrzut list tras HTTP, uruchomienie buildów i testów (szczegóły: `M0_BUILD_TEST_REPORT.md`).

## Źródła i ich stan

| Repo | Ścieżka | Wersja | Stack | Licencja |
|---|---|---|---|---|
| Audioticon (legacy / baza domenowa) | `~/.openclaw/workspace/Audioticon-legacy` | `main`, commit `5c07069` | Rust 2021, workspace `core` + `cli`, `ort` (ONNX), `tiny_http` | MIT OR Apache-2.0 |
| Portable-Local-Studio (dawca, zamrożony) | `~/.openclaw/workspace/Portable-Local-Studio` | `473debf` | Node (CommonJS, bez zależności w rdzeniu TTS) + React 19 / Vite 8 / Tauri 2 | MIT („Tech Jarves”) |
| ASTRA-UI-Template (gospodarz UI) | `~/.openclaw/workspace/ASTRA-UI-Template` | `agent-ui`, commit `4252f4f`, working tree czysty | Qt 6.12 / QML / C++17, qmake6 | MIT (+ translator MIT, DHH) |

**Uwaga o dwóch kopiach ASTRA.** W katalogu `~/.openclaw/workspace/ASTRA-UI-Template` nie ma kodu etapów E0–E1 (`src/core/`, `core.pri`, testy golden). Ten kod żyje w drugiej kopii roboczej: `~/.config/freebuff-desktop/local-projects/32b70bfb-…/` (gałąź `agent-ui`, zmiany niezacommitowane). To realne ryzyko rozjazdu — patrz §6, R-01.

---

## 1. Mapa modułów i odpowiedzialności

### 1.1 Audioticon-legacy — rdzeń domenowy (28 684 linii Rust)

`Cargo.toml`: `members = ["crates/core", "crates/cli"]`, `rust-version = "1.92"`, zależności workspace: `tokio`, `anyhow`, `thiserror`, `serde`, `toml`, `regex`, `hound`, `sha2`, `ort` (`load-dynamic`, `api-31`), `ndarray`, `zip`, `tiny_http`, `clap`.

**`crates/core` — logika bez I/O konsolowego (30 modułów):**

| Modul | LOC | Odpowiedzialność | Uwagi z inspekcji |
|---|---|---|---|
| `parser.rs` | 1483 | Gramatyka emoji → `DirectedScript` (pauzy, tempo, pitch, filtry, role, SFX, ambience), 3 style produkcji | 15 testów; mapuje też `[whisper]`, `🤫`, `🪖`, `💋` |
| `audio.rs` | 2808 | Silniki TTS (`PiperBridge`, `KokoroBridge`, `EdgeTtsBridge`, `ElevenLabsBridge`), stitcher z crossfade, eksport MP3/FLAC/M4B, `AudioDirectorEngine` | `piper_worker.py` wbudowany przez `include_str!`; `ffmpeg` wołany jako proces |
| `auto_director.rs` | 1658 | Reżyseria AI (prompt + walidacja pokrycia tekstu, propozycje/approve/reject) | Wywołania LLM przez HTTP (Gemini/OpenAI/Ollama) |
| `importer.rs` | 1229 | EPUB/DOCX/TXT/MD → rozdziały → scenariusz | 9 testów |
| `kokoro.rs` | 1073 | Kokoro-82M: fonemizacja `espeak-ng --ipa`, tokenizer IPA, inferencja `ort`, fallback `kokoro_worker.py` | Wymaga `espeak-ng` (GPL-3.0) jako proces |
| `whisper_dsp.rs` | 1185 | **DSP szeptu** (macierze transferu 24-pasmowe, LPC) — nie ASR | Nazwa koliduje z Whisper ASR (patrz §5) |
| `style_transfer.rs` | 992 | Rozdzielone macierze stylu (whisper/shout/call/chant) | |
| `presets.rs` | 951 | `PresetRegistry`, `EffectPreset`, `ProjectConfig`, własne emoji z `audioticon.toml` | |
| `verbalizer.rs` | 890 | Normalizacja polskiego (liczby, daty, godziny, kwoty, skróty) | 6 testów |
| `voicelab.rs` | 782 | Profile głosowe, próbki, punch-in, eksport datasetu LJSpeech | |
| `models.rs` | 596 | Katalog + pobieranie głosów Piper, `voices.json` (142 głosy) | |
| `types.rs` | 510 | Kontrakt danych: `ProductionStyle`, `EmojiAction`, `ActorTag`, `TextSegment`, `DirectedItem`, `AudioticonProject` (`.audioticon` JSON) | Format projektu do zachowania |
| `model_hub.rs` | 502 | Katalog huba + kwity EULA (licencje niekomercyjne) | |
| `voucher.rs` / `wallet.rs` | 495 / 432 | AudioCoin$, vouchery VIP | Do usunięcia w 2.0 (model biznesowy, nie lokalne narzędzie) |
| `subtitles.rs` | 473 | SRT/VTT + wyrównanie słów | |
| `cadence.rs` | 463 | Oddechy i frazowanie (sylaby PL, `subtle/natural/off`) | |
| `dialog_master.rs` | 452 | Rozdzielanie narracji/dialogów, casting, płeć mówcy | |
| `sfx.rs` | 413 | Biblioteka SFX/ambient, import własnych | |
| `credentials.rs` | 365 | Klucze BYOK, maskowanie, weryfikacja ElevenLabs | |
| `waveform.rs` | 343 | Piki waveformu + mapowanie segmentów | |
| `cache.rs` | 322 | SHA-256 cache zdań, statystyki, LRU | Klucz: `CACHE_VERSION` + parametry segmentu |
| `voice_design.rs`, `batch_pipeline.rs`, `room_tone.rs`, `studio_leveler.rs`, `ledger.rs`, `pcm_stream.rs`, `estimator.rs`, `updater.rs`, `wsola.rs` | 321–129 | Projektowanie głosu (ElevenLabs), dziennik batch/crash-resume, room tone, leveler/peak-limit, pomiar LUFS/True Peak (ACX), stream PCM, kosztorys, aktualizacje, time-stretch WSOLA | |

**`crates/cli` — powłoka CLI + serwer Studio:**

| Element | LOC | Odpowiedzialność |
|---|---|---|
| `args.rs` | 666 | Kontrakt CLI (clap): `render`, `cache`, `script`, `presets`, `serve`, `models {list,hub,accept,download,remove,update}`, `import`, `providers {list,set,check,remove}`, `estimate`, `voicelab {list,create,add-sample,punch-in,clone-elevenlabs,export-dataset,delete,design}`, `emojis {list,add,remove}`, `direct`, `sfx {list,catalog,import}`, `wallet {balance,redeem,topup}`. Globalne: `--style narrator\|multi-voice\|audio-drama`, `--cadence`, `--breath-intensity`, `--bit-depth`, `-j/--jobs` |
| `commands/*.rs` | 179–449 | Jedna komenda = jeden plik (`render.rs` 449, `models.rs` 204, `script.rs`, `cache.rs`, `sfx.rs`, `voicelab.rs`, `wallet.rs`, `emojis.rs`, `import.rs`, `providers.rs`, `direct.rs`) |
| `server/mod.rs` | **5535** | Wbudowany serwer HTTP (`tiny_http`) + wbudowane HTML (`/index.html`, `/classic.html`, `/studio.html`) — 59 tras `/api/*` |

Serwer to monolit: 5535 linii, w tym zaszyty HTML/JS. **To główne źródło długu i główny argument za 2.0 jako aplikacją natywną.**

### 1.2 Portable-Local-Studio — dawca (9 606 linii JS w `scripts/`, plus frontend)

Brak `package.json` w katalogu głównym. Struktura:

| Warstwa | Pliki | Rola |
|---|---|---|
| Launchery | `linux.sh` (10 068 B), `mac.sh`, `windows.bat` | Setup + start lokalnego serwera Node i frontendu |
| Serwer | `scripts/server/serve.cjs` (**8435**), `model-kind.cjs` (140) | Wszystkie trasy HTTP: LLM, TTS, STT, modele, telemetria, pobrania |
| Rdzeń TTS/dyrygowania | `scripts/server/tts-directives.cjs` (641) | Bez zależności: gramatyka emoji, verbalizer PL, cache kluczy, WAV join, DSP przez ffmpeg |
| Workery | `scripts/workers/tts-kokoro-worker.mjs`, `tts-voiceconv.py`, `coreml_server.py`, `openvino_npu_worker.py` | Mosty do silników (Node/Python) |
| Web-search | `scripts/search/{core,content,providers,cache}.js` (88/159/106/37) | Lokalne wyszukiwanie dla czatu |
| Setup | `scripts/setup/*.sh|*.ps1` (piper, tts, whisper, llama, openvino, coreml) | Instalacja runtime'ów i modeli, wersje per-platforma |
| Frontend | `app/frontend/src` (React 19.1, Vite 8.3, Tauri 2) | `TextToSpeech`, `SpeechTranscriber`, `ModelManager`, `Settings`, `TextChat`, `Generator`, `Sidebar`, `TopStatusBar`, `services/api.js` (~1200 linii klienta HTTP) |
| Testy | `scripts/tests/*.test.cjs` | 31 testów regresyjnych (issue-regressions, model-kind) |

Model danych dawcy jest **katalogowy**, nie projektowy: `app/tts-models/`, `app/tts-cache/segments/`, `app/tts-outputs/`, `app/speech-models/`, `app/llm-models/` — wszystko w `.gitignore`, nic nie jest w repo.

### 1.3 ASTRA-UI-Template `agent-ui` — gospodarz UI

22 pliki, brak `src/core`. Dwa targety qmake: `astra-ui-template.pro` (dashboard `main`) i `agent-console.pro` (`TARGET = astra-agent-console`).

| Plik | Rola |
|---|---|
| `src/agent/main.cpp` | Host: `QQuickStyle("Material")`, kontekst QML `feed` (AgentModel) + `i18n` (Translator), flagi `--screenshot`, `--language`, `--demo` |
| `src/agent/agentmodel.{h,cpp}` | **Kontrakt danych konsoli**: `sessions`, `current`, `messages`, `streaming`, `model/modelLabel`, `tools`, `contextUsed/Max`, `tokensIn/Out`, `tokensPerSecond`, `lastLatency`, `turns`, `toolRuns`, `failures`, `strip`, `hint`, `accent`, `glass`; invokable: `send`, `stop`, `newSession`, `pickSession`, `messageAt` |
| `src/qml/AgentMain.qml` | Układ: szyna sesji (208 px, ukrywana < 1000 px), transkrypt + composer, panel runtime, `StateStrip` na dole; skróty `Q`, `Esc`, `N`, `?`, `Ctrl+L` |
| `src/qml/ChatBubble.qml`, `StateStrip.qml` | Bąbelek wiadomości, pasek stanu |
| `src/translator.*`, `src/i18n/pl.json` | i18n PL/EN, zapis wyboru w `QSettings` |
| `tests/tests.cpp` + `tests.pro` | 15 testów (dashboard + 4 testy konsoli agenta) |

**Kluczowy fakt:** `AgentModel` jest atrapą — `answerFor()` zwraca scenariusz, narzędzia tykają na `QTimer` (24 ms/tick). Kontrakt jest jednak wymienny: komentarz w nagłówku mówi wprost „Replace `answerFor()` and the tool events with a real backend and nothing in the QML has to change”. To jest właściwy punkt integracji.

### 1.4 E0–E1 (druga kopia ASTRA, `~/.config/freebuff-desktop/.../`)

| Plik | Zawartość |
|---|---|
| `src/core/core.pri` | Jedna lista źródeł rdzenia dla GUI i testów |
| `src/core/Paths.{h,cpp}` | XDG: `dataDir()`, `cacheDir()`, `voicesDir()`, `outputsDir()` |
| `src/core/voice/DirectiveParser.{h,cpp}` | `VoiceSegment{text,speed,dsp,pauseMs}`, `Chapter`, `DirectedText`, `parseDirectedText`, `describeDirectives`, `hasSpeakableText`, `cleanupChunk`, `preparePlainText`, `stripDirectives` |
| `src/core/voice/PolishVerbalizer.{h,cpp}` | `audioticon::verbalizePolish` |
| `tests/data/tts-golden.json` + `generate-golden.cjs` | 44 przypadki gramatyki + 11 palety, generowane z dawcy |
| `docs/ARCHITECTURE.md` | Rejestr ustaleń, mapa docelowych modułów, status E0/E1 |

---

## 2. Kontrakty API / IPC

### 2.1 Kontrakt CLI (Audioticon, do zachowania w 2.0)

Jeden binarny `audioticon` z podkomendami. Wartości domyślne, które są już kontraktem (używane w testach e2e): `render --provider edge --voice pl-PL-MarekNeural --lang pl --style audio-drama`, `script -f elevenlabs|chattts|timeline`, `presets`, `cache --stats|--prune|--clear`, `models …`, `import`, `estimate`, `voicelab …`, `emojis …`, `direct`, `sfx …`, `wallet …`.

Zaszyte teksty: `DEMO_SCENE` („Stacja Polarna Aurora”) i `DEMO_EFFECTS_SCENE` — używane przez testy e2e (`script --demo`), więc **nie wolno ich zmieniać bez aktualizacji testów**.

### 2.2 Kontrakt HTTP Studio (legacy, 59 tras) — referencja, nie transport 2.0

| Grupa | Trasy |
|---|---|
| Render/projekt | `/api/render`, `/api/parse`, `/api/export`, `/api/estimate`, `/api/segment/rerender` |
| Cache | `/api/cache`, `/api/cache/status` |
| Modele/licencje | `/api/models`, `/api/models/download`, `/api/models/remove`, `/api/models/update`, `/api/hub`, `/api/hub/models`, `/api/hub/accept`, `/api/license`, `/api/license/activate`, `/api/license/status` |
| Dostawcy | `/api/providers`, `/api/providers/set`, `/api/providers/check` |
| Reżyseria AI | `/api/auto-direct`, `/api/auto-direct/propose`, `/api/auto-direct/approve`, `/api/auto-direct/reject` |
| Dialog/import | `/api/dialog/analyze`, `/api/dialog/apply`, `/api/import` |
| Jakość/dowody | `/api/qc/audit`, `/api/qc/latest`, `/api/ledger/certificate`, `/api/subtitles/export`, `/api/subtitles/latest`, `/api/waveform/analyze`, `/api/waveform/latest` |
| VoiceLab | `/api/voicelab`, `/api/voicelab/profiles`, `…/create`, `…/delete`, `…/sample`, `…/sample/audio`, `…/punch-in`, `…/punch-in/revert`, `…/dataset`, `…/clone-elevenlabs`, `…/design/preview`, `…/design/save` |
| Styl/emoji | `/api/style-transfer/{list,preview,save,extract}`, `/api/custom-emojis{,/add,/remove}` |
| System | `/api/system/health`, `/api/system/network-info`, `/api/system/qr-code`, `/api/system/update-check` |
| Monetyzacja | `/api/wallet`, `/api/wallet/redeem` |

**Wniosek:** w 2.0 te trasy znikają jako transport. Zostaje ich *semantyka* — powinna zostać przeniesiona na metody C++ (jedna klasa/menedżer na grupę) i ewentualnie wystawiona w CLI.

### 2.3 Kontrakt HTTP dawcy (65 tras) — dla porównania zachowań

Najważniejsze dla przeszczepu: `POST /api/tts/speak` (`{text, voice, speed, …}`), `/api/tts/{start,stop,status,models,download-model,import-model,delete-model,outputs,delete-output}`, `/api/speech/transcribe`, `/api/llm/{chat,start,stop,status,models,download-model,stats,benchmark,conversations}`, `/api/{health,diagnostics,telemetry,hardware-specs,backend-status,backend-options,cleanup,cleanup-candidates,web-search,outputs,save-output}`.

`tts-directives.cjs` eksportuje m.in. `parseDirectedText`, `describeDirectives`, `hasSpeakableText`, `cleanupChunk`, `preparePlainText`, `verbalizePolish`, `parseWav`, `pcm16FromWav`, `wavFromPcm`, `silencePcm`, `joinSegments`, `applyDspPreset`, `resolveFfmpeg`, `segmentCacheKey`. To dokładnie ten zestaw, który E1 przeniósł do C++ — kontrakt funkcji jest już ustalony.

### 2.4 Kontrakt QML ↔ C++ w ASTRA (punkt wymiany)

- Dwa konteksty: `feed` (model) i `i18n` (translator). QML nie zna typów spoza tych dwóch.
- QML czyta wyłącznie właściwości z `Q_PROPERTY` i woła `Q_INVOKABLE`. `messageAt(int)` istnieje, bo `modelData` nie działa niezawodnie dla `QVariantList`.
- Sygnały sterują odświeżaniem (`messagesChanged`, `usageChanged`, `stripChanged`, `themeChanged`).
- Skróty i akcent pochodzą z modelu (`feed.accent` czytany z motywu Omarchy), więc nowy model musi je zachować, inaczej UI straci spójność.
- `--screenshot` (offscreen + jeden obrót) i `ASTRA_AGENT_DEMO` to gotowy mechanizm smoke testu UI — warto zachować.

### 2.5 Kontrakt rdzenia E0/E1 (C++)

```cpp
namespace audioticon {
struct VoiceSegment { QString text; double speed; QString dsp; int pauseMs; };
struct Chapter { int index; QString title; };
struct DirectedText { QVector<VoiceSegment> segments; QVector<Chapter> chapters;
                      int leadingPauseMs; QString text; QVariantList directives; };
DirectedText parseDirectedText(const QString &raw, bool polish = true);
}
```
To jest właściwa granica: UI i CLI dostają `DirectedText`, nie emoji. Dalsze etapy (E2/E3) dopisują `PiperEngine`, `SegmentCache`, `WavIo`, `Dsp`, `RenderJob` za tą samą granicą.

---

## 3. Punkty integracji Audioticon × ASTRA UI

| # | Warstwa | Konkretne miejsce | Co trzeba zrobić | Ryzyko |
|---|---|---|---|---|
| I-01 | Build | `agent-console.pro` (kopia robocza) | `QT += multimedia sql network` + `include(src/core/core.pri)` — **już zrobione w E0**; w klonie `.openclaw` tego nie ma | Rozjazd kopii (§6 R-01) |
| I-02 | Kontrakt UI | `src/agent/agentmodel.h` | Zachować wszystkie `Q_PROPERTY`; podmienić `answerFor()`/`tick()` na host LLM; dodać drugi model (Studio) zamiast rozszerzać `AgentModel` | QML ma zaszyte `feed.*` w ~1200 liniach — każda zmiana nazw to edycja QML |
| I-03 | Nawigacja | `AgentMain.qml`, szyna sesji (208 px, `visible: win.width > 1000`) | Zamienić „sesje” na listę modułów (Studio, Czat, VoiceLab, SFX, Eksport) — układ już pasuje 1:1 | Ukrywanie szyny < 1000 px schowa nawigację modułów — próg do zmiany |
| I-04 | Studio głosu | nowy `StudioView.qml` + `studioModel` | Paleta emoji z `describeDirectives()`, composer, lista segmentów, odsłuch | Brak wzorca w ASTRA — trzeba napisać od zera (dawca ma tylko React) |
| I-05 | Audio | `QAudioSink` na PCM (bez GStreamera) | `multimedia` w `.pro`, bufor PCM z `WavIo` | 22 050 Hz (Piper bass) vs 44 100 Hz (mastering) — konwersja w rdzeniu |
| I-06 | Sylabus/CLI | `src/core/cli/` + drugi target qmake | CLI jako drugi `TARGET`, wspólny `core.pri` | Dwa targety w jednym `.pro` albo dwa pliki `.pro` + wspólny `.pri` (rekomendacja) |
| I-07 | Store | `src/core/store/` (SQLite) | Sesje/wątki/wiadomości/wyjścia/ustawienia | `QSqlDatabase` wymaga `sql` + sterownika `QSQLITE` (obecny w systemie) |
| I-08 | Chat/LLM | `src/core/llm/` | `LlamaServer` jako `QProcess`, jedyny loopback w aplikacji | Model 1,3 GB backendu + 4,4 GB modeli — nie wchodzi do repo |
| I-09 | i18n | `translator.*`, `i18n/pl.json` | Dopisać klucze nowych ekranów | Brak |
| I-10 | Testy | `tests/tests.cpp` + `tests.pro` | Golden z dawcy (E1) + smoke renderu bez modeli | Testy wymagające modeli muszą być oznaczone i pomijalne |

**Rekomendowana architektura docelowa (spójna z `docs/ARCHITECTURE.md`):** ASTRA zostaje powłoką (QML + host), cała logika w `libaudioticon_core` (C++), CLI jako drugi target na tym samym `core.pri`, zero HTTP między UI a rdzeniem, jedyny loopback to `llama-server` i pobieranie modeli. Audioticon-legacy (Rust) pozostaje **specyfikacją zachowania i źródłem algorytmów**, nie runtime'em — inaczej mamy dwa silniki.

---

## 4. Elementy Portable Local Studio warte adaptacji

| Element | Ocena | Co dokładnie | Uzasadnienie |
|---|---|---|---|
| `tts-directives.cjs` (gramatyka + verbalizer + cache + WAV) | 🟢 ZIELONY | Cały moduł | 641 linii, zero zależności, **już przeniesiony w E1** i pokryty goldenem 44+11 przypadków |
| `docs/polish-tts-notes.md` | 🟢 ZIELONY | Tabela głosów z licencjami, pomiary `tame_tails()`, odrzucone modele (XTTS CPML, MMS CC-BY-NC, OmniVoice 6,1 GB/RTF≈4) | Gotowa decyzja o głosie `pl_bass` (Apache-2.0, 114 MB, 22 050 Hz) — nie trzeba powtarzać researchu |
| Cache zdań + pomiar | 🟢 ZIELONY | Klucz SHA-256 z (głos, tempo, DSP, tekst, sample rate); 5 segmentów = 5,6 s pierwszy raz, 18 ms powtórka | To są liczby-cel dla E3 (u nas: < 50 ms) |
| Bramka `tame_tails()` (0,20/0,45/0,25) | 🟡 ŻÓŁTY | Wygładzanie końcówek zdań, pomiary metryki ogonków (35,4 → 25,0) | Wymaga portu do C++; wartości kalibracyjne do zachowania |
| Wzorzec nadzoru workerów (`piper-venv`, `.part` → rename, progres pobierania) | 🟡 ŻÓŁTY | `setup-piper.sh`, `POST /api/tts/download-model` | W C++ wypada jako `QProcess` + `QNetworkAccessManager`; wart adaptacji jest **protokół pobierania**, nie Node |
| `model-kind.cjs` (klasyfikacja plików modeli) | 🟡 ŻÓŁTY | 140 linii, 15 testów | Przydatne przy imporcie modeli użytkownika |
| Detekcja backendu / telemetria / hardware-specs | 🟡 ŻÓŁTY | `backends`, `stats`, `benchmark` | W 2.0 uprościć do `doctor` |
| Frontend React (komponenty TTS/ModelManager) | 🟡 ŻÓŁTY | **Tylko jako wzorzec UX** | Nie przenosimy webowego UI (ustalenie produktowe) |
| OpenVoice V2 (`tts-voiceconv.py`, tone_extract/tone_color, refs) | 🔴 CZERWONY | 7,1 GB runtime + modele | Poza zakresem: klonowanie barwy to inna funkcja; licencja modeli z `Hinotsuba/OpenVoice-ONNX-v2` do sprawdzenia |
| Serwer HTTP + Tauri shell | 🔴 CZERWONY | `serve.cjs` 8435 linii, `@tauri-apps/api` | Architektura wykluczona ustaleniem „zero webowego UI i serwera aplikacji” |
| Edge-TTS bridge, portfel/monetyzacja | 🔴 CZERWONY | `edge-tts`, `wallet.rs`/`wallet` w dawcy | ToS/regulaminy + brak wartości w lokalnym narzędziu |

---

## 5. Duplikaty Piper / Kokoro / Whisper / LLM

| Funkcja | Implementacja A | Implementacja B | Implementacja C | Rekomendacja |
|---|---|---|---|---|
| **Piper** | Dawca: `app/tts-runtime/piper-venv/bin/piper` (Python, `setup-piper.sh`), głosy w `app/tts-models/piper` | Legacy: `piper_worker.py` (Python, `piper.voice.PiperVoice`) wołany przez `include_str!` i `python3` | Legacy: kierunek `ort` (`load-dynamic`, `api-31`) — natywna inferencja ONNX w Rust | **Jedna ścieżka: natywna.** W 2.0 C++ (ONNX Runtime) lub Rust `ort`; Python tylko jako spadek po legacy, do usunięcia w E2+ |
| **Kokoro** | Dawca: `tts-kokoro-worker.mjs` (`kokoro-js`, modele zarządzane przez `onnx-community/Kokoro-82M-v1.0-ONNX`, warianty q8/fp32) | Legacy: `kokoro.rs` — fonemizacja `espeak-ng --ipa`, własny tokenizer IPA, inferencja `ort`, fallback `kokoro_worker.py` | — | Legacy jest bogatszy (własna fonemizacja, 6 języków, Apache-2.0). Zostawić legacy; **Kokoro nie obsługuje polskiego** — poza pionowym wycinkiem |
| **Whisper — kolizja nazw** | Dawca: Whisper **ASR** (`whisper-cli`/`whisper-server`, buildy CPU/Vulkan/Metal, `.bin` GGML) do transkrypcji | Legacy: `whisper_dsp.rs` to **DSP szeptu aktora**, nie ASR | Legacy: `model_hub.rs` ma wpis `whisper-tiny-aligner` jako „planowany ASR” | **Zmienić nazwę**: `whisper_dsp` → `whisper_dsp` niejednoznaczne; rozdzielić na `actor_whisper_dsp` i `asr`. ASR to osobna funkcja (napisy/wyrównanie), nieczęść M0–M5 |
| **LLM** | Dawca: host `llama-server` (backend 1,3 GB, modele 4,4 GB, `llm/start|stop|chat|stats|benchmark`, `search/*` jako narzędzie) | Legacy: `auto_director.rs` — HTTP do Gemini/OpenAI/Ollama, bez lokalnego runtime | — | Dawca ma **działający host lokalny**; to on jest dawcą dla „wariantu A” czatu. Legacy zostaje źródłem promptów i walidacji pokrycia tekstu |
| **Głosy PL** | Dawca: 5 głosów (gosia, darkman, bass, mc_speech, mls_6892) | Legacy: `models.rs` + `voices.json` (142 głosy, m.in. te same pl_PL) | — | Jeden katalog z manifestem licencji; `voices.json` z legacy jako baza, wpisy PL z notatek dawcy jako źródło prawdy o licencjach |
| **Cache** | Dawca: `app/tts-cache/segments/`, klucz `(voice,speed,dsp,text,sampleRate)` | Legacy: `cache.rs`, `CACHE_VERSION`, LRU | 2.0 (plan): `v1|voice|speed|dsp|rate|text` | **Ujednolicić przed E3** — inaczej cache 2.0 nie odtworzy trafień dawcy i nie da się porównać czasów |

---

## 6. Ryzyka licencyjne i techniczne

### 6.1 Licencje

| # | Element | Licencja | Ryzyko | Działanie |
|---|---|---|---|---|
| L-01 | Kod: Audioticon MIT/Apache-2.0, PLS MIT, ASTRA MIT | permisywne | Niskie | Zachować `LICENSE` obu projektów i `THIRD_PARTY_NOTICES.md` przy każdym przeszczepie kodu |
| L-02 | Translator ASTRA | MIT, David Heinemeier Hansson (z Omarchy) | Niskie | Nota już jest w `LICENSE` — nie usuwać |
| L-03 | Piper (silnik) | Apache-2.0 | Niskie | — |
| L-04 | Głosy Piper PL | `gosia`/`darkman`/`mc_speech` CC0, `bass` Apache-2.0, `mls_6892` CC BY 4.0 | **Średnie** | `mls_6892` wymaga atrybucji przy redystrybucji; UI musi pokazywać licencję per głos (legacy ma `LicenseType`, dawca ma tabelę w notatkach) |
| L-05 | Łańcuch pochodzenia `pl_bass` | model card: Apache-2.0, fine-tune z `en_US-lessac-high` | **Średnie** — `lessac-medium` bywa opisany jako niekomercyjny (Blizzard 2013); łańcuch `high` nie jest jednoznacznie udokumentowany | Przed publikacją komercyjną: zapisać model card + provenance; jeśli brak pewności, dać `gosia`/`darkman` (CC0) jako domyślny głos komercyjny |
| L-06 | `espeak-ng` (fonemizacja Kokoro) | **GPL-3.0** | **Średnie** | Wołane jako proces zewnętrzny (granica procesu), ale nadal zależność runtime. Udokumentować jako opcjonalną; nie linkować, nie dołączać binarki do paczki bez decyzji |
| L-07 | Kokoro-82M | Apache-2.0 (`CommercialSafe`) | Niskie | — |
| L-08 | Edge TTS | ToS Microsoft (publiczne endpointy Read Aloud), wrapper `edge-tts` LGPL-3.0/MIT | **Wysokie** dla produktu komercyjnego | Nie promować jako ścieżki domyślnej; komunikat w UI, jak w `THIRD_PARTY_NOTICES.md` |
| L-09 | ElevenLabs BYOK | komercyjne API | Niskie (klucz użytkownika) | Klucze tylko lokalnie, nigdy w repo/logach (`credentials.rs` już to robi) |
| L-10 | OpenVoice V2 / modele `Hinotsuba/OpenVoice-ONNX-v2` | notatki dawcy: MIT; licencja wag niepotwierdzona | **Wysokie** | Poza zakresem 2.0 — nie przenosić |
| L-11 | XTTS-v2, MMS-TTS-pol | CPML / CC-BY-NC | Wysokie | Już odrzucone w notatkach dawcy — nie wracać bez decyzji |
| L-12 | `voucher.rs`/`wallet.rs` (AudioCoin$) | kod własny | Niskie prawne, wysokie produktowe | Usunąć z 2.0: lokalna aplikacja nie potrzebuje własnej waluty |

### 6.2 Ryzyka techniczne

| # | Ryzyko | Dowód | Skutek | Mitygacja |
|---|---|---|---|---|
| R-01 | **Dwie kopie roboczych ASTRA** (`~/.openclaw/...` czysta na `4252f4f`, E0–E1 w `~/.config/freebuff-desktop/...` niezacommitowane) | `git status` obu kopii | Praca w złej kopii = „zaginione” etapy; build z `.openclaw` nie zawiera `src/core` | Najpierw commit E0–E1 na `agent-ui` w jednej kopii, potem jedna kopia jako źródło prawdy (albo symlink) |
| R-02 | `cargo test` nie jest zielone | `audio::tests::test_export_to_flac_synthetic` → `ffmpeg` exit 234 | Fałszywy sygnał „legacy jest zepsute”; nie da się użyć testów jako bramki regresji | Nie „naprawiać testem”: to niezgodność ffmpeg 9 (`loudnorm` bez `-ar` po nim); szczegóły w `M0_BUILD_TEST_REPORT.md` |
| R-03 | `ort` `load-dynamic` + `api-31` | `Cargo.toml`; w systemie tylko `/opt/superwhisper/lib/libonnxruntime.so` (1.24.2) | Natywna inferencja nie ruszy bez wskazania `ORT_DYLIB_PATH`; wersja API musi się zgadzać | W E2 zdecydować: dowiązanie systemowe vs własna kopia ORT; test `doctor` na obecność i wersję |
| R-04 | Brak `espeak-ng`, brak pakietów Pythona (`piper`, `onnxruntime`, `kokoro_onnx`), brak `piper` w PATH | inwentarz w raporcie build/test | Żadna ścieżka TTS nie działa „z pudełka” | E2: natywny C++ + jawny komunikat braków; nie polegać na `python3` |
| R-05 | `serve.cjs` 8435 linii jako „oracle” | pomiar | Wszelkie zachowania trzeba wyciągać z monolitu; ryzyko cichej zmiany dawcy | Dawca zamrożony (ustalenie); golden z E1 jest kontraktem, nie kod dawcy |
| R-06 | Monolit serwera legacy 5535 linii w `crates/cli/src/server/mod.rs` | pomiar | Trudny port, kuszące „przenieśmy serwer” | W 2.0 **nie portować** — przenieść semantykę tras do klas C++ |
| R-07 | Rozjazd klucza cache (dawca vs plan 2.0) | §5 | Brak porównywalności czasów i trafień | Ustalić jeden format klucza przed E3 i zapisać w `ARCHITECTURE.md` |
| R-08 | Różne sample rate (Piper 22 050 Hz, mastering 44 100/48 000) | `polish-tts-notes.md`, `export_to_flac` | Trzaski/„chipmunk” przy złej konwersji; różne długości | Konwersja w rdzeniu (jeden `WavIo`), test na długości i pikach |
| R-09 | `Q_PROPERTY` + `QVariantList` jako model listy | `agentmodel.h` (`messageAt` istnieje właśnie z tego powodu) | Przy 1000+ segmentów wydajność i „stale index” | Dla Studio użyć `QAbstractListModel`, nie `QVariantList` |
| R-10 | Testy wymagające modeli/ffmpeg/GPU | build/test | Nietestowalne w CI | Rozdział testów: bez modeli (parser, verbalizer, cache, WAV, DSP) vs z modelami (oznaczone, pomijalne) |
| R-11 | `rust-version = 1.92` + ostrzeżenia „unused field in workspace.package” | `cargo check` | Kosmetyka, ale sygnalizuje, że metadane workspace nie są dziedziczone | Przy okazji E2 ujednolicić `[workspace.package]` |
| R-12 | Nazwa `whisper` w trzech znaczeniach | §5 | Pomyłki w dokumentacji i testach | Zmiana nazw modułów przy przenoszeniu |

---

## 7. Wnioski (M0)

1. **Rdzeń jest w Rust, UI w Qt — i to jest właściwy podział pracy, nie docelowy runtime.** Audioticon-legacy to specyfikacja zachowania + gotowe algorytmy; ASTRA + `libaudioticon_core` to produkt. Dwa runtime'y TTS w jednym produkcie to największe ryzyko projektu.
2. **Punkt integracji jest już czysty:** `feed` + `i18n` w kontekście QML, `AgentModel` jawnie zaprojektowany do wymiany. Nie trzeba przepisywać QML, żeby podłączyć backend.
3. **Z dawcy bierzemy trzy rzeczy:** gramatykę/verbalizer (już zrobione w E1, golden 44+11), decyzję o głosie `pl_bass` z licencjami oraz protokół cache/pobierania. Reszta dawcy to CZERWONY lub tylko wzorzec UX.
4. **Duplikaty trzeba rozstrzygnąć przed E2, nie po:** Piper (3 ścieżki), Kokoro (2), Whisper (ASR vs DSP — kolizja nazw), LLM (host dawcy vs HTTP legacy). Każdy duplikat to dwa zestawy błędów do naprawienia.
5. **Dług do spłaty w legacy:** `wallet`/`voucher`, serwer HTTP 5535 linii, `piper_worker.py`/`kokoro_worker.py` jako most Python, nazwa `whisper_dsp`.
6. **Najpilniejsze działania porządkowe:** commit E0–E1 w jednej kopii ASTRA (R-01), decyzja o ORT (R-03), ujednolicenie klucza cache (R-07).
