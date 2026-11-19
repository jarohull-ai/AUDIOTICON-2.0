# M5 — Job Runtime: model zadań Rust ↔ Qt (bez prawdziwego TTS)

Data: 2026-11-18
Zakres: `AUDIOTICON 2.0/backend`, `AUDIOTICON 2.0/ui` (oryginalne repozytoria `Audioticon-legacy`
i `ASTRA-UI-Template` nietknięte; brak commitów).

---

## 1. Cel i granice

M5 zastępuje makietę M3 prawdziwym modelem zadań: rejestr jobów i jeden runtime Tokio po stronie
Rust, a po stronie Qt — `StudioBridge`, które **odpytuje** ten rejestr co ~50 ms.

Czego M5 świadomie **nie** robi:

* brak modeli (`*.onnx`), brak Piper, brak ONNX Runtime — żaden model nie jest pobierany ani ładowany;
* „synteza" to atrapa: `create_silence_wav(200 ms)` z prawdziwego `audioticon-core`;
* brak zmian w QML (panel `studio` w `AgentMain.qml` nie istnieje — patrz §13);
* brak profilu `--release` (binarki nadal debug: ~92 MB).

Granica FFI pozostaje na **segmencie**, nie na tekście: `DirectiveParser`/`PolishVerbalizer` żyją
w C++ (golden E1), a do Rusta trafia już zwykły tekst. Dzięki temu nie ma dwóch źródeł prawdy
o dyrektywach.

---

## 2. Architektura

```
  wątek GUI (Qt)                                   wątki Rusta
 ┌──────────────────────────────┐          ┌────────────────────────────────────┐
 │ StudioBridge  (QObject)      │          │ BridgeEngine                       │
 │  startRender() ──────────────┼── FFI ──▶│   start_job()  ── rejestracja ──┐  │
 │  QTimer 50 ms ─ pollJob() ───┼── FFI ──▶│   job_state()  ── snapshot ◀────┤  │
 │  cancelRender() ─────────────┼── FFI ──▶│   cancel_job() ── flaga ◀───────┤  │
 └──────────────────────────────┘          │                                 │  │
            ▲                              │   Arc<JobRegistry>              │  │
            │ sygnały Qt                   │   Mutex<HashMap<u64, JobRecord>>│  │
 ┌──────────┴───────────────────┐          └───────────────┬─────────────────┘  │
 │ QML / testy Qt               │                          │ spawn                │
 └──────────────────────────────┘          ┌───────────────▼─────────────────┐  │
                                           │ Runtime Tokio (1 worker,        │  │
                                           │ OnceLock, nigdy nie dropowany)  │◀─┘
                                           │  sleep → progress → WAV 200 ms  │
                                           └─────────────────────────────────┘
```

Brak `block_on` po stronie GUI **i** po stronie Rusta: jedyne wejście do worka zadań to
`Runtime::spawn`. `grep block_on` w `lib.rs` trafia wyłącznie w dwa komentarze dokumentacyjne.

---

## 3. Kontrakt FFI (cxx bridge, namespace `audioticon::bridge`)

| Funkcja | Sygnatura | Zachowanie |
|---|---|---|
| start | `start_job(engine, text, output_dir) -> Result<u64>` | rejestruje job, zwraca id; pusty tekst → `Err`; pusty `output_dir` → katalog domyślny |
| stan | `job_state(engine, job_id) -> JobStateDto` | kopia snapshotu; nieznane id → `Idle`, `noexcept` |
| anulowanie | `cancel_job(engine, job_id) -> bool` | `false`, gdy job nieznany lub już terminalny |
| diagnostyka | `active_jobs(engine) -> u32` | liczba jobów nieterminalnych (używane w testach) |

`JobStateDto` (shared struct): `job_id: u64`, `stage: TaskStage`, `progress_percent: f32`,
`current_segment: u32`, `total_segments: u32`, `status_message: String`, `error_message: String`,
`output_path: String`.

`TaskStage` (już istniejący enum) niesie: `Idle, Queued, Normalizing, Synthesizing, Mastering,
Finished, Failed, Cancelled`. M5 używa z niego pięciu stanów wymaganych w zadaniu
(`Queued`, `Synthesizing`, `Finished`, `Failed`, `Cancelled`); reszta jest zarezerwowana pod M6.

Błędy `start_job` (`BridgeError`) są przez cxx zamieniane na wyjątek C++; `AudioticonCore.cpp`
łapie `std::exception` (z `what()` = komunikat Rusta) i `...` jako backstop. Żadna panic Rusta nie
przechodzi przez granicę — cxx zamienia ją na abort, nie na wyjątek.

---

## 4. Rejestr i maszyna stanów

`Arc<JobRegistry>` jest współdzielony przez `BridgeEngine` i zadania runtime, więc job przeżyje
zniszczenie obiektu po stronie Qt (np. zamknięcie okna w trakcie renderu).

```
register ──▶ Queued ──▶ Synthesizing ──▶ Finished
                 │            │
                 └────────────┴──▶ Failed      (błąd I/O przy create_silence_wav)
                              └──▶ Cancelled   (cancel_job, natychmiast, ze strony GUI)
```

* `update_running()` **ignoruje** joby terminalne — spóźniony krok workera nie wskrzesi
  anulowanego zadania (to jest dokładnie ten wyścig, który test `jobCancelStopsTheJob` sprawdza
  po 400 ms ciszy).
* `cancel()` ustawia `AtomicBool` i od razu stan `Cancelled`; worker tylko sprawdza flagę między
  krokami i wychodzi.
* Zatruty `Mutex` (panic w jobie) nie psuje rejestru: `unwrap_or_else(|p| p.into_inner())`.
* Identyfikatory: `AtomicU64`, start od 1; `0` w C++ znaczy „nie udało się".

---

## 5. Runtime Tokio

```rust
fn job_runtime() -> &'static tokio::runtime::Runtime   // OnceLock<Runtime>
```

* **jeden** runtime na proces, `worker_threads(1)`, `thread_name("audioticon-jobs")`,
* tworzony leniwie przy pierwszym `start_job`,
* **nigdy nie dropowany**: `Runtime::drop` zamyka workery i blokuje wątek, który zwalnia ostatni
  `BridgeEngine` — a tym wątkiem może być GUI. `OnceLock` czyni to niemożliwym.
* `enable_all()` daje `tokio::time::sleep` (używane zamiast `std::thread::sleep`, żeby krok nie
  zajmował wątku).

---

## 6. Strona Qt

### `AudioticonCore.{h,cpp}` — jedyny plik dotykający cxx

Nowa fasada `audioticon::ui::CoreSession` (pimpl, `std::unique_ptr<Impl>`), która trzyma
`rust::Box<BridgeEngine>` w `std::optional` (nieudane `create_engine` nie wywala aplikacji).
Do świata Qt wychodzą wyłącznie typy Qt:

```cpp
enum class JobStage { Idle, Queued, Normalizing, Synthesizing, Mastering, Finished, Failed, Cancelled };
struct JobSnapshot { quint64 jobId; JobStage stage; double percent; int currentSegment;
                     int totalSegments; QString statusMessage, errorMessage, outputPath; };
class CoreSession { quint64 startJob(text, outputDir, QString *err);
                    JobSnapshot jobState(quint64); bool cancelJob(quint64); };
```

`coreSampleRate()` z M4 zostaje (test linkowania).

### `StudioBridge` — prawdziwy bridge zamiast mocka

* `m_mockTimer` i `m_observer` zniknęły; jest `QTimer m_pollTimer` (50 ms) → `pollJob()`.
* `startRender(rawText, outputDir = {})`:
  1. `parseDirectedText()` (C++ parser + verbalizer) → `plain_text` i lista segmentów dla UI,
  2. `CoreSession::startJob(plainText, outputDir)` → id,
  3. ustawia `Queued`, `isBusy = true`, startuje timer.
* `pollJob()` czyta `jobState()` i emituje sygnały **tylko przy zmianie** (stage, komunikat,
  procent, ścieżka). Na stanie terminalnym zatrzymuje timer, `isBusy = false`, `renderFinished(ok)`.
* `cancelRender()` woła `cancel_job()` i **natychmiast** `pollJob()`, więc `Cancelled` jest
  widoczne od razu, a nie do 50 ms później.
* `setErrorFromWorker()` (wejście dla przyszłego `BridgeObserver`) zatrzymuje timer i ustawia
  `Failed` + `errorMessage`.
* Nowe property: `statusMessage`, `outputPath`, `jobId`; nowy sygnał `renderFinished(bool)`.
* Stare `updateProgressFromWorker()` zostaje — `BridgeObserver` (szew M3/M6 na powiadomienia
  push) nadal się kompiluje, ale M5 go nie używa.

### Postęp i komunikaty

| moment | `stage` | `progress` | `status_message` (przykład) |
|---|---|---|---|
| rejestracja | `Queued` | 0 % | `W kolejce (0/3 segmentów)` |
| start pracy | `Synthesizing` | 0 % | `Synteza 0/3` |
| krok n | `Synthesizing` | `n/total × 90 %` | `Synteza segmentu 2/3` |
| zapis pliku | — | 90 % | (bez zmiany) |
| sukces | `Finished` | 100 % | `Gotowe: /tmp/…/job_1_silence_200ms.wav` |
| błąd I/O | `Failed` | ostatni | `Renderowanie nie powiodło się` + `error_message` |
| anulowanie | `Cancelled` | zamrożony | `Anulowano przez użytkownika` |

Liczba kroków: `planned_steps(text) = (words / 8).clamp(3, 6)`, opóźnienie 80 ms na krok —
job trwa ~240–480 ms, więc da się go złapać w połowie (test anulowania) i nie blokuje testów.

---

## 7. Atrapowy render

`audioticon_core::create_silence_wav(&path, 200, 44_100)` — prawdziwa funkcja rdzenia
(room-tone ~-62 dBFS, nie cyfrowe zero). Plik: `<output_dir>/job_<id>_silence_200ms.wav`,
domyślnie `$TMPDIR/audioticon-jobs/`. Testy weryfikują nagłówek RIFF/WAVE, 1 kanał,
44100 Hz i rozmiar dokładnie `44 + 44100×200/1000×2 = 17 684 B`.

---

## 8. Pliki zmienione

| plik | linie | rola |
|---|---|---|
| `backend/crates/bridge/src/lib.rs` | 985 | `JobStateDto`, `start_job/job_state/cancel_job/active_jobs`, `JobRegistry`, runtime Tokio, `spawn_mock_render`, 5 nowych testów |
| `backend/crates/bridge/Cargo.toml` | +1 | `tokio` (już w `[workspace.dependencies]`) |
| `ui/src/bridge/AudioticonCore.h` | 77 | fasada `CoreSession`, `JobStage`, `JobSnapshot` (bez cxx w nagłówku) |
| `ui/src/bridge/AudioticonCore.cpp` | 154 | mapowanie `TaskStage`→`JobStage`, `rust::String`→`QString`, obsługa wyjątków cxx |
| `ui/src/bridge/StudioBridge.h` | 78 | polling zamiast mocka: `startRender/cancelRender`, `jobId/outputPath/statusMessage`, `renderFinished` |
| `ui/src/bridge/StudioBridge.cpp` | 218 | `QTimer` 50 ms → `pollJob()`, anulowanie, mapowanie stanów |
| `ui/tests/tests.cpp` | 449 | `jobFlowStartProgressFinish()`, `jobCancelStopsTheJob()` |
| `ui/tests/tests.pro` | 19 | testy budują prawdziwy `StudioBridge.cpp` + `BridgeObserver.cpp` |

Nietknięte: `backend/crates/bridge/build.rs`, `ui/src/bridge/rustlink.pri`, `ui/bridge.pri`,
`ui/src/agent/main.cpp`, QML, `crates/core`.

---

## 9. Testy

### Rust — `cargo test -p audioticon-bridge`: **11 passed, 0 failed** (0,49 s)

Było 6, doszły 5:

* `test_job_flow_queued_progress_finished` — start → (Queued|Synthesizing) → postęp > 0 →
  `Finished` 100 %, `current_segment == total_segments`, plik WAV poprawny (nagłówek, 44,1 kHz,
  17 684 B), `active_jobs() == 0`;
* `test_job_cancel_stops_the_worker` — czeka na postęp, anuluje, sprawdza `Cancelled` < 100 %,
  brak ścieżki wyjściowej; po 400 ms stan i procent **niezmienione**, drugi `cancel` → `false`;
* `test_job_failure_reports_error_message` — `output_dir = /dev/null/audioticon` (prawdziwy błąd
  `ENOTDIR`) → `Failed` + niepusty `error_message`, `active_jobs() == 0`;
* `test_start_job_rejects_empty_text` — `Err`, rejestr pusty;
* `test_job_state_of_unknown_id_is_idle` — `Idle`, `cancel` → `false`.

### Qt — `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./tests`: **20 passed, 0 failed** (4961 ms)

18 istniejących testów zachowanych (16 slotów + `initTestCase`/`cleanupTestCase`), plus 2 nowe:

* `jobFlowStartProgressFinish()` — prawdziwy job przez FFI, `QTRY_VERIFY` obserwuje
  `Synthesizing` i rosnący procent, potem `Finished` 100 %, `renderFinished(true)` dokładnie raz,
  plik istnieje i ma > 8 kB;
* `jobCancelStopsTheJob()` — czeka na `percent > 0`, `cancelRender()`, natychmiast `Cancelled`,
  `isBusy == false`, `renderFinished(false)` raz, po 400 ms nadal `Cancelled` z tym samym
  procentem (worker nie wskrzesza joba).

To, że `QTRY_VERIFY` w ogóle widzi kroki pośrednie, jest dowodem, że pętla zdarzeń Qt żyje
w trakcie renderu — nie ma zamrożenia GUI.

---

## 10. Wyniki komend (stan końcowy, po wszystkich zmianach)

| komenda | wynik |
|---|---|
| `cargo check --workspace` (backend) | `0` błędów, exit **0** |
| `cargo build -p audioticon-bridge` | exit **0**, `target/debug/libaudioticon_bridge.a` 220 896 530 B |
| `cargo test -p audioticon-bridge` | **11 passed, 0 failed** |
| `qmake6 ../agent-console.pro && make -j$(nproc)` | exit **0**, `build-console/astra-agent-console` 92 225 272 B |
| `qmake6 ../tests/tests.pro && make -j$(nproc)` | exit **0**, `build-tests/tests` 92 368 360 B |
| `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./tests` | **20 passed, 0 failed**, 4961 ms |
| `./astra-agent-console --probe-ffi` | `audioticon core sample rate: 44100 Hz`, exit **0** |

Symbole nowego kontraktu są w binarce Qt (linkowane z `.a`, nie z nagłówków) — potwierdzone
linkowaniem `tests`, które wywołuje `start_job`/`job_state`/`cancel_job`.

---

## 11. Defekty i pułapki napotkane w M5

1. **Cargo nie przebudowuje staticlib po zmianie `lib.rs` w niektórych ścieżkach.**
   `cargo build -p audioticon-bridge` potrafi skończyć w 0,05 s i odtworzyć z cache ostrzeżenie
   „cxx headers published", nie publikując nowych nagłówków. Obejście (już w `rustlink.pri`):
   `cargo clean -p audioticon-bridge && cargo build -p audioticon-bridge`. W M5 wymagane po
   dodaniu `start_job`: bez tego Qt linkowałby stary `.a` i padł na niezdefiniowanych symbolach.
2. **`target/cxxbridge/rust/cxx.h` ma 0 bajtów.** `cxx-build` publikuje ten nagłówek jako
   symlink do `~/.cargo/registry/.../cxx-1.0.205/include/cxx.h`, a ten plik w lokalnym rejestrze
   jest pusty. **Nie jest to blokada**: cxx 1.0.205 *inline'uje* treść `rust/cxx.h` w generowanym
   `lib.rs.h` (dyrektywa jest zakomentowana), więc Qt nie potrzebuje tego pliku. Sprawdzone
   `g++ -fsyntax-only` na `lib.rs.h`: 0 błędów. Warto to wiedzieć, zanim ktoś „naprawi" pusty
   plik i wprowadzi rozjazd wersji.
3. **`Result<u64>` przez cxx działa** (generuje funkcję rzucającą wyjątek, bez `noexcept`) —
   dlatego `AudioticonCore.cpp` łapie `std::exception`; typ wyjątku (`rust::Error`) nie jest
   nazwany w nagłówku, więc nie wolno go wymieniać w `catch`.
4. **`Q_PROPERTY(quint64 …)`** wymaga `quint64` z `QtGlobal` — ciągnięte przez `QString`, ale
   warto o tym pamiętać przy przenoszeniu nagłówka.

---

## 12. Czego M5 nie obejmuje (świadomie)

* **Brak modeli**: żaden `*.onnx`, brak `ORT_DYLIB_PATH`, brak Piper — `create_silence_wav`
  jest jedynym „renderem".
* **Brak QML**: `StudioBridge` jest wystawione jako `studio` w `main.cpp`, ale `AgentMain.qml`
  go nie używa; nie dodano panelu postępu (nie było w zakresie M5).
* **Brak profilu release**: binarki ~92 MB (debug + statyczny Rust). Przed dystrybucją:
  `cargo build --release`, `strip`, `-O2` bez `-g`.
* **Brak historii jobów**: rejestr trzyma wszystkie wpisy do końca życia procesu (rośnie
  liniowo). Dla audiobooka z tysiącami segmentów M6 powinien dodać eviction albo trzymać tylko
  N ostatnich.
* **Brak testu dwóch równoległych jobów** — `startRender` blokuje drugi job po stronie Qt
  (`m_isBusy`), a Rust dopuszcza wiele; nie było w wymaganiach.

---

## 13. Następny krok (M6)

1. Podmienić `spawn_mock_render` na wywołanie `PiperBridge`/`SpeechSynthesizer` z
   `audioticon-core` (nadmodele `*.onnx`, `ORT_DYLIB_PATH` — patrz pułapka ORT 1.24 vs 1.31
   z `M2_TOOLCHAIN_FFI_AUDIT.md` §4).
2. Per-segment progress z prawdziwej syntezy (`total_segments` już jest w DTO).
3. Panel postępu w QML (`studio.taskStatus/taskPercent/renderFinished`) + przycisk anulowania.
4. `eviction` rejestru i `--job-report` w CLI (diagnostyka bez GUI).
5. Profil `--release` + `strip` przed dystrybucją.
