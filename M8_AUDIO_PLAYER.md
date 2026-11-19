# M8 — Raport: Odtwarzanie WAV/PCM przez QAudioSink

**Data ukończenia:** 2026-11-18  
**Środowisko robocze:** `/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/ui`  
**Oryginalne repozytoria:** `Audioticon-legacy` oraz `ASTRA-UI-Template` — nietknięte, czyste, brak commitów.

---

## 1. Cel i zakres etapu M8

Etap M8 dostarcza natywny, nieblokujący odtwarzacz audio dla Audioticon 2.0 oparty na Qt 6 Multimedia (`QAudioSink`), zintegrowany z mostem `StudioBridge` oraz interfejsem użytkownika w QML (`StudioView.qml`).

Zrealizowano wszystkie punkty specyfikacji:
1. **Klasa `AudioPlayer`:** Implementacja w C++ (`src/audio/AudioPlayer.h`, `src/audio/AudioPlayer.cpp`) oparta o `QAudioSink` i in-memory `QBuffer`.
2. **Odtwarzanie plików WAV/PCM:** Bezpośrednie odtwarzanie pliku wynikowego z `outputPath` generowanego przez rdzeń Rust (M5/M7 Piper).
3. **Pełna kontrola transportu:** Metody `play(filePath)`, `pause()`, `resume()`, `stop()`, `togglePlayPause()`, `seek(positionMs)` oraz `setVolume(volume)`.
4. **Kompletny zestaw sygnałów:**
   - `playingChanged(bool isPlaying)`
   - `pausedChanged(bool isPaused)`
   - `positionChanged(qint64 positionMs)`
   - `durationChanged(qint64 durationMs)`
   - `playbackFinished()`
   - `errorOccurred(const QString &error)`
5. **Podpięcie do `TransportBar` w `StudioView.qml`:**
   - Przycisk Play/Pause/Resume reagujący na stan odtwarzania.
   - Przycisk Stop (`⏹`).
   - Przyciski nawigacji `⏮` (przewijanie do 0:00 / poprzedni segment) i `⏭` (następny segment).
   - Suwak pozycji (`Slider` / scrubber) z obsługą przeciągania (`seekPlayback`).
   - Licznik czasu w formacie `mm:ss / mm:ss`.
6. **Przyciski `▶` przy segmentach:**
   - Przycisk `▶` w wierszu każdego segmentu uruchamia syntezę tego segmentu, a po jej zakończeniu automatycznie rozpoczyna odtwarzanie (`m_autoPlayOnFinish`).
   - W trakcie odtwarzania ikona aktywnego segmentu zmienia się na `⏸`, umożliwiając natychmiastowe zatrzymanie.
7. **Obsługa błędów urządzenia audio:**
   - Wykrywanie braku urządzenia wyjściowego (`QMediaDevices::defaultAudioOutput().isNull()`).
   - Reakcja na błędy inicjalizacji strumienia i zgłaszanie ich przez `errorOccurred` i `playbackError` bez zamrażania aplikacji.
8. **Brak blokowania GUI:**
   - Dekodowanie nagłówka RIFF/WAVE do bufora pamięciowego następuje synchronicznie w kilkadziesiąt mikrosekund.
   - Odtwarzanie strumieniowe i polling pozycji (25 ms) działają asynchronicznie przez pętlę zdarzeń Qt.

---

## 2. Architektura i implementacja

### 2.1. Klasa `AudioPlayer` (`src/audio/AudioPlayer.h`, `src/audio/AudioPlayer.cpp`)
- **Parser formatu RIFF/WAVE:**
  - Weryfikacja sygnatury `RIFF....WAVE`.
  - Odczyt chunka `fmt `: wsparcie dla 16-bit PCM integer (`QAudioFormat::Int16`), 8-bit PCM, 32-bit PCM oraz 32-bit IEEE Float (`QAudioFormat::Float`), dowolne częstotliwości próbkowania (44.1 kHz, 24 kHz, 16 kHz), mono i stereo.
  - Odczyt chunka `data`: wyodrębnienie surowych bajtów PCM do `QByteArray m_pcmData`.
  - Obliczenie dokładnego czasu trwania: `durationMs = (pcmData.size() * 1000) / bytesPerSec`.
- **Zarządzanie `QAudioSink`:**
  - Utworzenie instancji `QAudioSink` powiązanej z domyślnym urządzeniem wyjściowym.
  - Strumieniowanie z `QBuffer` otwartego w trybie `ReadOnly`.
  - Obsługa przejść stanów w `handleStateChanged(QAudio::State)`:
    - `ActiveState`: start timera pozycji, emisja `playingChanged(true)`.
    - `SuspendedState`: zatrzymanie timera pozycji, emisja `pausedChanged(true)`.
    - `StoppedState`: obsługa ewentualnego błędu, reset flag.
    - `IdleState`: sygnał dotarcia do końca danych PCM — automatyczne zatrzymanie, ustawienie pozycji na `durationMs` i emisja `playbackFinished()`.
- **Precyzyjny tracking pozycji:**
  - Timer 25 ms pobiera `m_audioSink->processedUSecs() / 1000` i sumuje z przesunięciem `m_seekOffsetMs`.
  - Natychmiastowe uaktualnienie po wywołaniu `seek(positionMs)` z wyrównaniem do granic ramki audio (`m_frameSize`).

### 2.2. Rozszerzenie `StudioBridge` (`src/bridge/StudioBridge.h`, `src/bridge/StudioBridge.cpp`)
- Most zyskał instancję `AudioPlayer *m_player`.
- Udostępniono właściwości dla QML:
  - `Q_PROPERTY(bool isPlaying ...)`
  - `Q_PROPERTY(bool isPaused ...)`
  - `Q_PROPERTY(qint64 playbackPosition ...)`
  - `Q_PROPERTY(qint64 playbackDuration ...)`
  - `Q_PROPERTY(QString playbackError ...)`
  - `Q_PROPERTY(AudioPlayer* player READ player CONSTANT)`
- Metody pomocnicze QML:
  - `playFile(filePath)`, `playOutput()` — odtwarzanie pliku z zadania.
  - `pausePlayback()`, `resumePlayback()`, `stopPlayback()`, `togglePlayback()`.
  - `seekPlayback(positionMs)`.
  - `playSegment(index)` — reżyserski odsłuch pojedynczego zdania (synteza + auto-play).

### 2.3. Dostosowanie UI (`src/qml/StudioView.qml`)
- Funkcja pomocnicza `formatTimeMs(ms)` do konwersji milisekund na format `mm:ss`.
- Wzbogacony `TransportBar`:
  - Przycisk Play: `"▶ Odtwórz"` gdy idle, `"⏸ Pauza"` podczas grania, `"▶ Wznów"` po zapauzowaniu.
  - Przycisk Stop (`⏹`): resetuje pozycję i zatrzymuje odtwarzanie.
  - `Slider` pozycji: powiązany z `studio.playbackPosition` i `studio.playbackDuration`, reaguje na przeciąganie użytkownika (`onMoved`).
  - Etykieta czasu: wyświetla aktualny czas i łączną długość pliku WAV.
- W liście segmentów (`ListView`):
  - Przycisk `▶` uruchamia `studio.playSegment(index)`.
  - Ikona zmienia się dynamicznie na `⏸` dla aktualnie odtwarzanego segmentu.

---

## 3. Zmiany w plikach projektu

1. **`ui/bridge.pri`:**
   - Dodano `$$PWD/src/audio` do `INCLUDEPATH`.
   - Dodano `AudioPlayer.h` do `HEADERS` i `AudioPlayer.cpp` do `SOURCES`.
2. **`ui/tests/tests.pro`:**
   - Dodano moduł `multimedia` do `QT`.
   - Dodano `../src/audio` do `INCLUDEPATH`.
   - Dodano `AudioPlayer.h` i `AudioPlayer.cpp` do kompilacji testowej.
3. **`ui/tests/tests.cpp`:**
   - Dodano nagłówki `<QtEndian>`, `<cmath>` oraz `"AudioPlayer.h"`.
   - Dodano funkcję pomocniczą `createTestWav(...)` generującą poprawny plik WAV z tonem sinusoidalnym 440 Hz.
   - Dodano test `audioPlayerPlaybackCycle()` weryfikujący:
     - Parsowanie nagłówka RIFF/WAVE i parametry formatu audio.
     - Pełny cykl `play() -> pause() -> resume() -> stop()`.
     - Odtworzenie do końca i emisję sygnału `playbackFinished()`.
     - Integrację ze `StudioBridge`: renderowanie zadania i odtworzenie przez `playOutput()`.

---

## 4. Wyniki testów i weryfikacja

Uruchomiono pełny zestaw testów w wymaganym trybie headless:
```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./tests
```

**Wynik uruchomienia:**
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
PASS   : Tests::cleanupTestCase()
Totals: 24 passed, 0 failed, 0 skipped, 0 blacklisted, 6051ms
********* Finished testing of Tests *********
```

**Kompilacja aplikacji produkcyjnej:**
```bash
cd ui/build-console && qmake6 ../agent-console.pro && make -j$(nproc)
```
- Status: **0 błędów, exit code 0**.
- Wygenerowano binarkę `astra-agent-console` powiązaną z `libQt6Multimedia.so` oraz biblioteką Rust `libaudioticon_bridge.a`.

**Screenshot smoke test:**
- Wygenerowano zrzut ekranu okna aplikacji: `/tmp/audioticon-m8-preview.png` (138 141 B).

---

## 5. Podsumowanie zgodności z regułami

- Praca wykonana wyłącznie w katalogu: `/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/ui`.
- Oryginalne repozytoria `Audioticon-legacy` i `ASTRA-UI-Template` nie były modyfikowane.
- Rdzeń Rust nie był modyfikowany.
- Brak commitów w git.
- Wszystkie 24 testy (23 istniejące + 1 nowy) przechodzą bez błędów.
