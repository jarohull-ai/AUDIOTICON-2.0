# Raport M13 — First Run Wizard & Doctor UX (Audioticon 2.0)

**Data wdrożenia:** 18 listopada 2026  
**Środowisko:** Linux (Omarchy / Arch Linux), Qt 6.12 Quick, C++17, Rust Tokio Staticlib  
**Zakres zmian:** Wyłącznie `/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/ui`  
**Referencja UX:** `M12_RELEASE_UX_SPEC.md`  
**Referencja lokalna bundle:** `release/audioticon-local-linux/scripts/doctor.sh`

---

## 1. Cel i architektura rozwiązania

W kamieniu milowym M13 zaimplementowano natywny, lokalny system diagnostyki oraz przewodnika pierwszego uruchomienia dla stacji roboczej Audioticon 2.0. Zgodnie z zasadami projektu:
- **Zero HTTP / Zero WebView:** brak przeglądarek i serwerów sieciowych; aplikacja w 100% działa w środowisku lokalnym.
- **Zero automatycznego pobierania:** aplikacja nie pobiera modeli ani zależności w tle bez wiedzy użytkownika; wskazuje lokalne ścieżki i komendy naprawcze.
- **Brak QMessageBox:** wszystkie komunikaty, kreatory i ostrzeżenia korzystają z architektury kart ASTRA Dark Glass.
- **Pełne wsparcie dwujęzyczne (i18n):** polski i angielski przez `qsTr` i katalog tłumaczeń.
- **Tryb Reżyserski (bez TTS):** pełna gotowość do pracy twórczej (edycja dyrektyw emoji, analiza tekstu, eksport scenariuszy i mastering) nawet w przypadku braku modelu głosu na maszynie.

---

## 2. Zaimplementowane komponenty

### 2.1. Model diagnostyczny C++ (`DoctorModel`)
Pliki: [DoctorModel.h](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/doctor/DoctorModel.h), [DoctorModel.cpp](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/doctor/DoctorModel.cpp)

Model zarządza inspekcją środowiska i udostępnia właściwości reaktywne dla QML:
- **Qt Runtime & Rust FFI:** weryfikacja wersji Qt (6.12), platformy (`xcb`/`wayland`/`offscreen`) oraz próbkowanie `audioticon::ui::coreSampleRate()` (44 100 Hz zlinkowane ze statyczną biblioteką Rust `libaudioticon_bridge.a`).
- **Urządzenie Audio:** badanie `QMediaDevices::audioOutputs()` oraz `QMediaDevices::defaultAudioOutput()` wraz z fallbackiem dla gniazda PipeWire (`/run/user/$UID/pipewire-0`) i ALSA (`/dev/snd`).
- **FFmpeg & Kodery:** badanie ścieżki `ffmpeg`, wersji, filtrów (`loudnorm`) oraz koderów (`flac`, `aac`), a także obecności `ffprobe`.
- **Silnik Piper-TTS:** detekcja lokalnego środowiska wirtualnego `~/.local/share/audioticon/venv`, CLI `piper` oraz modułu Pythona `piper`.
- **Model Głosu:** detekcja modelu przez `AUDIOTICON_PIPER_MODEL`, zapisaną konfigurację `QSettings` lub skanowanie katalogu `~/.local/share/audioticon/models/` (`*.onnx` + `.json`).
- **ONNX Runtime (ORT):** weryfikacja `ORT_DYLIB_PATH` oraz systemowych `libonnxruntime.so` (status: `"optional"` — brak ORT nie blokuje pracy, gdyż Piper korzysta z własnego runtime).
- **Katalogi XDG:** weryfikacja istnienia i uprawnień do zapisu dla `XDG_DATA_HOME` (`data`, `models`, `outputs`) oraz `XDG_CACHE_HOME` (`cache`).
- **Metody i sterowanie:**
  - `generateReport()`: generuje czytelny, sformatowany raport diagnostyczny.
  - `copyReportToClipboard()`: natywny zapis do schowka systemowego przez `QClipboard`.
  - `selectModelFile(filePath)`: bezpieczny wybór i walidacja pliku `.onnx` z dysku.
  - `getRepairCommand(subsystem)`: zwrot dokładnych komend naprawczych dla systemu Pacman/PipeWire/Python.
  - `completeFirstRun()`, `resetFirstRun()`, `setInspectionMode(bool)`: sterowanie stanem first-run zapisanym w `QSettings`.

### 2.2. Modalny kreator First-Run (`FirstRunWizard.qml`)
Plik: [FirstRunWizard.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/FirstRunWizard.qml)
- Centrowana, przyciemniająca tło karta Glass (`z: 100`), wyświetlana automatycznie przy pierwszym uruchomieniu, gdy na dysku brak modelu głosu.
- Interaktywna lista kontrolna 5 podsystemów z diodami LED i stanami `[ OK ]`, `[ OSTRZEŻENIE ]`, `[ BRAK MODELU ]`.
- Przyciski akcji:
  - `[ 📁 Wskaż model z dysku ]`: otwiera natywny `FileDialog` filtrujący pliki `*.onnx`.
  - `[ 🩺 Otwórz Doctor ]`: przełącza moduł na pełną diagnostykę i zamyka modal.
  - `[ ⚙️ Uruchom bez TTS (Tryb Reżyserski) ]`: aktywuje `inspectionMode` i pozwala natychmiast rozpocząć pracę reżyserską.
  - `[ 🚀 Rozpocznij pracę ]`: główny przycisk akceptacji, odblokowany po spełnieniu warunków gotowości.

### 2.3. Pełny widok diagnostyki (`DoctorView.qml`)
Plik: [DoctorView.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/DoctorView.qml)
- Nowy pełnoekranowy moduł roboczy dostępny z lewej szyny nawigacyjnej oraz pod skrótem klawiszowym `Ctrl+7`.
- **Karta głównego statusu:**
  - Duża dioda i nagłówek informujący o gotowości: `ŚRODOWISKO GOTOWE DO PRACY`, `TRYB REŻYSERSKI (BEZ TTS)` lub `WYKRYTO BRAKI W ŚRODOWISKU`.
  - Przyciski: `Kopiuj pełny raport` (z animowanym potwierdzeniem skopiowania), `Odśwież stan`, `Wskaż model z dysku...`, `Przełącz tryb bez TTS`, `Resetuj kreator (First-Run)`.
- **Siatka 6 kafelków LED podsystemów:**
  1. Qt 6 & Core C++/Rust FFI (44.1 kHz, platforma, ld.so)
  2. Urządzenie Audio (QAudioSink / PipeWire / ALSA)
  3. FFmpeg & Narzędzia Eksportu (loudnorm, flac, aac, ffprobe)
  4. Silnik Syntezy Piper-TTS (Python / CLI / venv)
  5. Aktywny Model Głosu (.onnx + .json, rozmiar w MB, liczba zainstalowanych modeli)
  6. ONNX Runtime (libonnxruntime — oznaczony jako opcjonalny)
- **Karta katalogów XDG:** zestawienie ścieżek do danych, modeli, cache i wyników renderu z potwierdzeniem praw zapisu.

### 2.4. Integracja z powłoką aplikacji
- [NavRail.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/NavRail.qml): dodano moduł `{ id: "doctor", icon: "🩺", label: qsTr("Doctor"), key: "Ctrl+7" }` oraz zintegrowano stopkę informującą o stanie środowiska (`ŚRODOWISKO OK`, `BEZ TTS`, `NAPRAWA`).
- [AgentMain.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/AgentMain.qml):
  - Rejestracja skrótu `Ctrl+7`.
  - Wskaźnik trybu reżyserskiego `[ TRYB REŻYSERSKI ]` w nagłówku aplikacji (obok badge PRO STUDIO).
  - Integracja dynamicznego wskaźnika aktywnego modelu / braku modelu w plakietce transportu.
  - Osadzenie `DoctorView` w centralnym obszarze roboczym.
  - Wyświetlanie `FirstRunWizard` jako modalnej nakładki.
- [ContextInspector.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/ContextInspector.qml):
  - Zastąpiono statyczny tekst w zakładce `diagnostics` żywym mini-panelem Doctora (wskaźniki diodowe 6 podsystemów, aktywny model, przycisk `Otwórz pełny Doctor (Ctrl+7)`, przycisk szybkiego kopiowania raportu oraz przełącznik trybu bez TTS).
- [StudioView.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/StudioView.qml):
  - Dodano pasek informacyjny trybu reżyserskiego: `[ 🎭 TRYB REŻYSERSKI AKTYWNY — Analiza tekstu, reżyseria emoji i mastering bez syntezy TTS ]`.
  - W przypadku błędu syntezy spowodowanego brakiem modelu (`brak modelu głosu`) baner błędu natychmiast oferuje kontekstowe przyciski: `[ 📁 Wskaż model... ]`, `[ 🩺 Doctor ]` oraz `[ ⚙️ Bez TTS ]`.

---

## 3. Wyniki testów i weryfikacja

Wszystkie testy wykonano natywnie w trybie offscreen:
```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./tests
```

### Podsumowanie wykonania zestawu testów:
```
********* Start testing of Tests *********
Config: Using QtTest library 6.12.0, Qt 6.12.0, omarchy 4.5.0
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
PASS   : Tests::firstRunWizardFlow()
PASS   : Tests::doctorMissingModelReportsStatus()
PASS   : Tests::doctorCompleteEnvironmentWithModel()
PASS   : Tests::doctorInspectionModeWithoutTts()
PASS   : Tests::doctorUiModuleAndOffscreenScreenshot()
PASS   : Tests::cleanupTestCase()
Totals: 32 passed, 0 failed, 0 skipped, 0 blacklisted, 9695ms
********* Finished testing of Tests *********
```

### Wygenerowane zrzuty ekranu:
- [audioticon-m13-doctor-preview.png](file:///tmp/audioticon-m13-doctor-preview.png) (169 KB, 1180×850) — pełny widok Doctora z kafelkami LED i ścieżkami XDG.
- [audioticon-m13-console-preview.png](file:///tmp/audioticon-m13-console-preview.png) (145 KB, 1180×850) — konsola agenta z zintegrowanym modelem diagnostycznym.

---

## 4. Wnioski i status wydania

1. **Kompletność:** Wdrożono wszystkie wymagania M13 zgodnie ze specyfikacją UX.
2. **Estetyka:** Interfejs FirstRunWizard i DoctorView zachowuje pełną spójność wizualną z ASTRA Glass, wzbogaconą o diody statusu LED i wyraźne stany OK/WARN/FAIL/OPCJONALNY.
3. **Brak regresji:** Wszystkie dotychczasowe 27 testów zachowały 100% zgodności, a 5 nowych testów zabezpiecza ścieżki diagnostyki, First-Run, braku modelu i trybu inspekcji.
4. **Czystość kodu:** Brak modyfikacji rdzenia Rust i oryginalnych repozytoriów zewnętrznych. Brak commitów git.
