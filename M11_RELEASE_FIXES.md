# M11 — Raport z usunięcia technicznych blockerów release

**Data:** 18 listopada 2026  
**Status:** Sukces — wszystkie 4 blockery techniczne (B1, B3, B2, B13) naprawione i zweryfikowane.  
**Środowisko:** Linux (x86_64), GCC 16.2.1, Qt 6.12.0, FFmpeg n9.0.2, Rustc 1.100.0-nightly.

---

## 1. Zakres i cel prac

Zgodnie ze specyfikacją zadania M11 podjęto prace wyłącznie w roboczej kopii projektu:
- `/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/backend`
- `/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/ui`

Nie modyfikowano repozytoriów źródłowych (`Audioticon-legacy`, `ASTRA-UI-Template`), nie commitowano zmian do repozytorium git, a kwestie bundlowania Qt/FFmpeg/ORT i instalatora pozostawiono na kolejne etapy wydania.

---

## 2. Zrealizowane poprawki

### B1 — Eksport FLAC, MP3 i M4B na FFmpeg 9 (`loudnorm` → `aresample` + asercja sample rate)

* **Identyfikacja problemu:**  
  Filtr `loudnorm` w FFmpeg 9 przetwarza dźwięk wewnętrznie w częstotliwości 192 kHz. Przy poprzedniej kolejności łańcucha filtrów (`aresample=...,loudnorm=...`) filtr `loudnorm` generował próbki 192 kHz na wyjściu łańcucha audio. Koder FLAC w FFmpeg odrzucał strumień z błędem `[flac] invalid block size: 192000` i proces kończył się kodem błędu 234.
* **Rozwiązanie w `backend/crates/core/src/audio.rs`:**
  1. Zmieniono kolejność filtrów na `loudnorm` przed `aresample`:
     - FLAC 24-bit: `"loudnorm=I=-19:TP=-3.0:LRA=7,aresample=48000:resampler=soxr:precision=28"` z jawnym parametrem `-ar 48000`.
     - FLAC 16-bit: `"loudnorm=I=-19:TP=-3.0:LRA=7,aresample=44100:resampler=soxr:precision=24"` z jawnym parametrem `-ar 44100`.
     - MP3: `"loudnorm=I=-19:TP=-3.5:LRA=7,aresample=44100,alimiter=limit=0.67:level=false"` z jawnym parametrem `-ar 44100`.
     - M4B: `"loudnorm=I=-19:TP=-3.5:LRA=7,aresample=44100,alimiter=limit=0.67:level=false"` z jawnym parametrem `-ar 44100`.
  2. Zaimplementowano funkcję weryfikacyjną:
     ```rust
     pub fn probe_audio_sample_rate(path: &Path) -> Result<u32>
     ```
     korzystającą z `ffprobe` do odczytu rzeczywistej częstotliwości próbkowania z nagłówka strumienia wyjściowego.
  3. W funkcjach `export_to_flac`, `export_to_mp3` oraz `export_to_m4b` dodano asercje docelowego sample rate po zakończeniu konwersji (48000 Hz dla 24-bit FLAC, 44100 Hz dla 16-bit FLAC, MP3 i M4B).
* **Testy regresyjne:**
  - `audio::tests::test_export_to_flac_synthetic` (24-bit, 48 kHz, soxr precision 28) — **PASS**.
  - `audio::tests::test_export_to_flac_16bit_synthetic` (16-bit, 44.1 kHz, soxr precision 24) — **PASS**.
  - `audio::tests::test_export_to_mp3_synthetic` (libmp3lame, 44.1 kHz, 192 kbps) — **PASS**.
  - `audio::tests::test_export_to_m4b_synthetic` (AAC, 44.1 kHz, 96 kbps) — **PASS**.

---

### B3 — `rustlink.pri`: automatyczne linkowanie staticlibu Release dla Qt Release

* **Identyfikacja problemu:**  
  Plik `ui/src/bridge/rustlink.pri` miał na stałe wpisane `AUDIOTICON_RUST_PROFILE = debug`. W efekcie nawet przy budowaniu aplikacji w trybie Release (`qmake CONFIG+=release`) Qt linkowało debugowy `libaudioticon_bridge.a` (rozmiar 212 MB, brak optymalizacji kompilatora, binarium aplikacji miało 89 MB zamiast kilkunastu).
* **Rozwiązanie w `ui/src/bridge/rustlink.pri`:**
  Wprowadzono automatyczne wykrywanie konfiguracji qmake:
  ```qmake
  isEmpty(AUDIOTICON_RUST_PROFILE) {
      CONFIG(release, debug|release) {
          AUDIOTICON_RUST_PROFILE = release
          CARGO_BUILD_FLAG = --release
      } else {
          AUDIOTICON_RUST_PROFILE = debug
          CARGO_BUILD_FLAG =
      }
  } else {
      equals(AUDIOTICON_RUST_PROFILE, "release") {
          CARGO_BUILD_FLAG = --release
      } else {
          CARGO_BUILD_FLAG =
      }
  }
  AUDIOTICON_RUST_LIBDIR = $$AUDIOTICON_BACKEND/target/$$AUDIOTICON_RUST_PROFILE
  ```
  Jeśli `libaudioticon_bridge.a` dla danego profilu nie istnieje, skrypt samoczynnie uruchamia kompilację z flagą `CARGO_BUILD_FLAG`.
* **Wynik:**  
  Kompilacja aplikacji Release tworzy zoptymalizowany `libaudioticon_bridge.a` (46 MB) i finalny plik wykonywalny `astra-agent-console` o rozmiarze zaledwie **16 MB** (spadek z 89 MB!).

---

### B2 — Preflight wykrywający pusty lub uszkodzony `cxx.h`

* **Identyfikacja problemu:**  
  W przypadku uszkodzenia lokalnego rejestru cargo lub przerwanej kopii, nagłówek `cxx.h` mógł zostać utworzony jako plik 0-bajtowy, powodując trudne w diagnostyce błędy kompilacji C++ (`namespace rust undeclared`).
* **Rozwiązanie wielopoziomowe:**
  1. **Na etapie publikacji glue (`backend/crates/bridge/build.rs`):**  
     Dodano asercję sprawdzającą, czy wygenerowany `dest/rust/cxx.h` istnieje, ma rozmiar > 100 bajtów oraz zawiera frazę `"namespace rust"`. W przeciwnym wypadku build script przerywa działanie z czytelnym błędem `Preflight B2 FAILED`.
  2. **Na etapie konfiguracji qmake (`ui/src/bridge/rustlink.pri`):**  
     Dodano test:
     ```qmake
     !system("test -s $$shell_quote($$AUDIOTICON_CXX_H) && grep -q 'namespace rust' $$shell_quote($$AUDIOTICON_CXX_H)") {
         error("B2 Preflight FAILED: naglowek $${AUDIOTICON_CXX_H} jest pusty lub uszkodzony...")
     }
     ```
  3. **W testach jednostkowych Rust (`backend/crates/bridge/src/lib.rs`):**  
     Dodano test `test_preflight_cxx_header_validity`.
  4. **W testach QtTest (`ui/tests/tests.cpp`):**  
     Dodano test `preflightCxxHeaderIsValid()`.

---

### B13 — Fallback bez modelu raportuje „Brak modelu głosu” i stan Failed (koniec z fałszywym „Finished”)

* **Identyfikacja problemu:**  
  Gdy aplikacja nie miała skonfigurowanego ani zainstalowanego modelu Piper (`resolve_piper_model() == None`), mechanizm fallbacku generował 200 ms ciszy i raportował `TaskStage::Finished` z komunikatem `Gotowe: .../silence_200ms.wav`. Wprowadzało to użytkownika w błąd, sugerując poprawną syntezę tekstu.
* **Rozwiązanie w `backend/crates/bridge/src/lib.rs`:**
  1. Dodano funkcję `spawn_fallback_no_model`:
     ```rust
     fn spawn_fallback_no_model(
         registry: Arc<JobRegistry>,
         job_id: u64,
         cancel: Arc<AtomicBool>,
     ) {
         job_runtime().spawn(async move {
             tokio::time::sleep(JOB_STEP_DELAY).await;
             if cancel.load(Ordering::SeqCst) {
                 return;
             }
             registry.finish(
                 job_id,
                 TaskStage::Failed,
                 0.0,
                 "Brak modelu głosu".to_string(),
                 "Brak modelu głosu (ustaw AUDIOTICON_PIPER_MODEL)".to_string(),
                 String::new(),
             );
         });
     }
     ```
  2. W `BridgeEngine::start_job`:
     - Jeśli model Piper jest dostępny (`Some(piper)`) → pełna synteza `spawn_chapter_render`.
     - Jeśli włączony jest jawny tryb mock (`self.mock_enabled`, np. w testach mock flow) → `spawn_mock_render`.
     - W standardowym trybie produkcyjnym bez modelu → `spawn_fallback_no_model`.
  3. UI (`StudioView.qml`) natychmiast wyświetla czerwony baner błędu: *"Brak modelu głosu (ustaw AUDIOTICON_PIPER_MODEL)"*, a `taskStatus` przyjmuje stan `Failed`. Nie jest tworzony żaden fałszywy plik audio.
* **Testy regresyjne:**
  - W teście Rust: `test_fallback_without_model_reports_missing_voice_model` — **PASS**.
  - W teście QtTest: `fallbackWithoutModelReportsMissingVoiceModel()` — **PASS**.

---

## 3. Zestawienie wyników testów

### A. Testy Rust (`cargo test --workspace`)
```
running 15 tests (audioticon-bridge)
test tests::test_core_version_reports_core_sample_rate ... ok
test tests::test_preflight_cxx_header_validity ... ok
test tests::test_empty_input_handling ... ok
test tests::test_create_engine ... ok
test tests::test_audio_buffer_pcm_generation ... ok
test tests::test_render_status_progression ... ok
test tests::test_start_job_rejects_empty_text ... ok
test tests::test_job_state_of_unknown_id_is_idle ... ok
test tests::test_parse_directed_text_basic ... ok
test tests::test_fallback_without_model_reports_missing_voice_model ... ok
test tests::test_job_cancel_stops_the_worker ... ok
test tests::test_job_failure_reports_error_message ... ok
test tests::test_job_flow_queued_progress_finished ... ok
test tests::test_real_piper_synthesis_one_segment ... ok
test tests::test_real_piper_chapter_pipeline_cache ... ok
test result: ok. 15 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out

running 34 tests (audioticon-cli)
test result: ok. 34 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out

running 130 tests (audioticon-core)
test audio::tests::test_create_silence_wav ... ok
test audio::tests::test_create_breath_wav ... ok
test audio::tests::test_create_sfx_and_ambient_synthesis ... ok
test audio::tests::test_export_to_m4b_synthetic ... ok
test audio::tests::test_export_to_mp3_synthetic ... ok
test audio::tests::test_export_to_flac_synthetic ... ok
test audio::tests::test_export_to_flac_16bit_synthetic ... ok
...
test result: ok. 130 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out

Łącznie Rust: 179 passed, 0 failed.
```

### B. Testy QtTest (`QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./tests`)
```
********* Start testing of Tests *********
Config: Using QtTest library 6.12.0, Qt 6.12.0 (x86_64-little_endian-lp64 shared (dynamic) release build; by GCC 16.2.1 20260810), omarchy 4.5.0
PASS   : Tests::initTestCase()
PASS   : Tests::ffiLinksRustCore()
PASS   : Tests::jobFlowStartProgressFinish()
PASS   : Tests::jobCancelStopsTheJob()
PASS   : Tests::catalogAndDefaults()
PASS   : Tests::toggleMoveReset()
PASS   : Tests::chartsAxes()
PASS   : Tests::historyIsBounded()
PASS   : Tests::unavailableGreysOut()
PASS   : Tests::persistence()
PASS   : Tests::stripRoundTrip()
PASS   : Tests::languageSwitch()
PASS   : Tests::rendersAndResponds()
PASS   : Tests::directiveGolden()
PASS   : Tests::directivePaletteMatchesDonor()
PASS   : Tests::agentTurnStreamsAndShowsTools()
PASS   : Tests::agentStopHaltsTheStream()
PASS   : Tests::agentSessionsKeepTheirOwnTranscript()
PASS   : Tests::agentConsoleRenders()
PASS   : Tests::uiModuleSwitching()
PASS   : Tests::uiDrawerVisibility()
PASS   : Tests::uiScreenshotSmokeTest()
PASS   : Tests::audioPlayerPlaybackCycle()
PASS   : Tests::wallpaperSystemAndReadability()
PASS   : Tests::preflightCxxHeaderIsValid()
PASS   : Tests::fallbackWithoutModelReportsMissingVoiceModel()
PASS   : Tests::cleanupTestCase()
Totals: 27 passed, 0 failed, 0 skipped, 0 blacklisted, 6964ms
********* Finished testing of Tests *********
```

---

## 4. Wskaźniki jakościowe

| Metryka | Stan przed M11 | Stan po M11 |
|---|---|---|
| Błędy eksportu FLAC (FFmpeg 9) | 1 fail (kod wyjścia 234) | **0 fail (100% poprawnych eksportów z asercją sample rate)** |
| Poprawność sample rate FLAC 24-bit | 192 000 Hz (nieobsługiwane) | **48 000 Hz (zweryfikowane ffprobe)** |
| Poprawność sample rate FLAC 16-bit | 192 000 Hz (nieobsługiwane) | **44 100 Hz (zweryfikowane ffprobe)** |
| Poprawność sample rate MP3 / M4B | Zależne od FFmpeg | **44 100 Hz (z asercją ffprobe)** |
| Profil staticlibu w Qt Release | Debug (212 MB) | **Release (46 MB)** |
| Rozmiar binarium konsoli Qt Release | 89 MB (niezoptymalizowany) | **16 MB (pełna optymalizacja -O2)** |
| Preflight uszkodzonego nagłówka `cxx.h` | Brak (padanie kompilacji) | **Fail-fast z natychmiastowym komunikatem B2** |
| Fallback bez modelu głosu | Fałszywe `Finished` + 200 ms ciszy | **Jawne `Failed` + komunikat „Brak modelu głosu”** |
| Wynik `cargo test --workspace` | 1 fail / 175 testów | **179 passed / 0 failed** |
| Wynik QtTest offscreen | 25 passed | **27 passed / 0 failed** |
