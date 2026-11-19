# M6 — Raport implementacji UI Audioticon 2.0 (ASTRA Glass UI)

**Data utworzenia:** 2026-11-18  
**Autor:** Antigravity (Architektura UI & Wdrożenie M6)  
**Środowisko robocze:** [`/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/ui`](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui)  
**Stan oryginalnych repozytoriów:** `Audioticon-legacy` oraz `ASTRA-UI-Template` w 100% czyste (brak zmian, brak commitów).

---

## 1. Cel i zakres etapu M6

Celem etapu M6 było przekształcenie powłoki ASTRA w pełnoprawny, natywny interfejs graficzny **Audioticon 2.0**, zgodnie ze specyfikacją [`M5_UI_LAYOUT_SPEC.md`](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/M5_UI_LAYOUT_SPEC.md) oraz planem [`M5_UI_IMPLEMENTATION_PLAN.md`](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/M5_UI_IMPLEMENTATION_PLAN.md).

Wdrożenie zrealizowano bez naruszania dotychczasowych kontraktów testowych i bez wprowadzania technologii webowych (HTTP/WebView).

---

## 2. Zrealizowane komponenty UI

### 2.1 Lewa szyna nawigacyjna: `NavRail.qml`
- **Lokalizacja:** `ui/src/qml/NavRail.qml` (rejestracja w `resources.qrc`).
- **Funkcjonalność:**
  - 6 modułów systemowych:
    1. `studio` — 🎙️ Studio (główny warsztat reżyserii audio),
    2. `agent` — 💬 Czat AI (konsola asystenta ASTRA),
    3. `voicelab` — 👥 VoiceLab (profile głosowe),
    4. `projects` — 📁 Projekty (materiały i struktura książki),
    5. `sfx` — 🔊 Foley/SFX (biblioteka efektów),
    6. `ledger` — 📊 ACX Ledger (pomiary głośności LUFS i True Peak).
  - Płynna responsywność: automatyczne zwijanie do samych ikon (szerokość 58 px) przy oknach `< 1200 px` oraz pełne etykiety (szerokość 176 px) na dużych ekranach.
  - Aktywny moduł oznaczony neonowym akcentem (`accent`) i bocznym wskaźnikiem.
  - Skróty klawiszowe w tooltipach: `Ctrl+1` .. `Ctrl+6`.
  - Wskaźnik stanu silnika w stopce szyny (sygnalizacja renderu żółtą kropką lub gotowości szmaragdową).

### 2.2 Główny warsztat reżyserski: `StudioView.qml`
- **Lokalizacja:** `ui/src/qml/StudioView.qml` (rejestracja w `resources.qrc`).
- **Trójstrefowy układ warsztatu:**
  1. **Directive Composer:**
     - Paleta dyrektyw emoji: `📖 Rozdział`, `🛑 Stop 900ms`, `⏸️ Pauza 400ms`, `😮‍💨 Oddech`, `🐢 Zwolnij`, `🐇 Przyspiesz`, `📻 Radio DSP`, `📱 Telefon`, `🏛️ Katedra`, `🤫 Szept`.
     - Wieloliniowy edytor tekstu ze zliczaniem znaków.
     - Przyciski akcji: `[🎙️ Reżyseruj (C++ E1)]` (natychmiastowe parsowanie przez `DirectiveParser`), `[▶ Syntetyzuj rozdział]` oraz `[🛑 Anuluj]` (widoczny w trakcie renderowania).
  2. **Segment Timeline View:**
     - Tabela wyodrębnionych zdań ze statusem cache (`HIT` / `MISS`).
     - Kolumny: indeks, treść znormalizowana (Verbalized PL), tempo, filtr DSP, pauza, status cache oraz przycisk bezpośredniego odsłuchu `[▶]`.
     - Kliknięcie wiersza zaznacza segment (`selectedIndex`) i automatycznie otwiera prawy panel w zakładce Inspektora.
  3. **Transport Bar:**
     - Kontrolki odtwarzacza: `⏮ Poprzedni`, `▶ Odtwórz / ⏸ Pauza`, `⏭ Następny`.
     - Pasek postępu renderowania powiązany z `studio.taskPercent`.

### 2.3 Prawy panel kontekstowy / drawer: `ContextInspector.qml`
- **Lokalizacja:** `ui/src/qml/ContextInspector.qml` (rejestracja w `resources.qrc`).
- **Funkcjonalność:**
  - Szerokość 320 px, obsługa trybu **Docked** (przypięty na stałe) oraz **Overlay Drawer** (pływająca szuflada nakładana animacją z cieniem scrim dla mniejszych okien lub po odpięciu).
  - Przycisk pinezki `[📌]` oraz przycisk zamknięcia `[✕]`.
  - 7 pełnych zakładek:
    1. **Inspektor:** ID segmentu, tekst, tempo, filtr DSP, pauza, hash SHA-256 oraz przycisk odsłuchu próbki.
    2. **Kolejka:** stan zadania (`studio.taskStatus`), pasek postępu (`studio.taskPercent`), komunikat statusu (`studio.statusMessage`), ścieżka wyjściowa WAV (`studio.outputPath`) i przycisk anulowania.
    3. **Głosy:** aktywny głos Piper ONNX (`pl_PL-bass · 22 050 Hz`, Apache-2.0, 114 MB) oraz lista dostępnych profili lokalnych (`pl_gosia`, `pl_darkman`).
    4. **Projekty:** metadane bieżącego projektu i pokrycie reżyserią.
    5. **Eksport:** opcje masteringu do M4B, MP3 i FLAC.
    6. **Ustawienia:** ścieżka pamięci podręcznej XDG (`~/.cache/audioticon/segments`) i czyszczenie LRU.
    7. **Diagnostyka (Doctor):** status linku Rust FFI (`44 100 Hz OK`), runtime Tokio, silnik C++ E1 i środowisko GUI.

### 2.4 Integracja powłoki aplikacji: `AgentMain.qml`
- **Przełączanie modułów:** centralny kontener przełącza widoki `StudioView` oraz `AgentView` w zależności od `win.activeModule`.
- **Gwarancja kompatybilności wstecznej:** `transcriptView` (`ListView` z modelem `feed.messages`) pozostaje trwale obecny w drzewie Qt Quick, dzięki czemu istniejące testy konsoli agenta przechodzą w 100%.
- **Globalne skróty klawiszowe:**
  - `Ctrl+1` .. `Ctrl+6` — przełączanie modułów NavRail,
  - `Ctrl+I` — otwarcie/zamknięcie panelu bocznego,
  - `Ctrl+J` — natychmiastowy skok do zakładki Kolejki zadań,
  - `Space` — zatrzymanie / kontrola renderu,
  - `Escape` — zamknięcie pływającego panelu lub przerwanie streamingu,
  - `Ctrl+L` — powrót do pola edycyjnego,
  - `Q` — wyjście z aplikacji.

### 2.5 Dedykowana akcja w `ChatBubble.qml`
- W dymkach odpowiedzi asystenta (`!isUser && !streaming`) dodano przycisk:
  **`[🎙️ Przeczytaj w studiu]`** (`objectName: "readInStudioBtn"`).
- Kliknięcie natychmiast:
  1. Wywołuje `studio.parseDirectedText(message.text)` z wykorzystaniem parsera C++ E1,
  2. Przełącza aktywny widok okna na `win.activeModule = "studio"`,
  3. Emituje sygnał `readInStudioRequested(string)`.

---

## 3. Zgodność z zasadami architektonicznymi

| Zasada | Status | Weryfikacja |
|---|:--:|---|
| **Zachowanie `transcriptView`** | ✅ | Obiekt obecny pod `objectName: "transcriptView"`, test `agentConsoleRenders` przechodzi w 100% |
| **Zachowanie `feed` i `i18n`** | ✅ | Wszystkie bindingi do `feed.accent`, `feed.glass`, `feed.messages`, `feed.strip` zachowane |
| **Brak drugiego parsera w Ruście** | ✅ | Parsowanie tekstu i normalizacja odbywa się wyłącznie w C++ E1 (`DirectiveParser.h`, `PolishVerbalizer.h`) |
| **Brak HTTP, WebView i przeglądarki** | ✅ | 100% czyste komponenty natywne Qt 6 Quick / Material Dark |
| **Brak pobierania modeli** | ✅ | Domyślne wskaźniki na lokalne zasoby offline |
| **Brak zmian w repozytoriach oryginalnych** | ✅ | `Audioticon-legacy` i `ASTRA-UI-Template` pozostały nienaruszone |

---

## 4. Wyniki kompilacji i testów

### 4.1 Kompilacja aplikacji `astra-agent-console`
```bash
cd "/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/ui/build-console"
qmake6 ../agent-console.pro && make -j$(nproc)
```
**Wynik:** Kompilacja zakończona kodem **0** (0 błędów, 0 ostrzeżeń). Wygenerowano plik binarny `astra-agent-console` zlinkowany statycznie z Rust FFI.

### 4.2 Testy jednostkowe QtTest (23 testy)
Wzbogacono plik `ui/tests/tests.cpp` o 3 nowe testy powłoki graficznej:
- `Tests::uiModuleSwitching()` — weryfikuje przełączanie modułów przez `NavRail` i zachowanie widoku transkryptu,
- `Tests::uiDrawerVisibility()` — weryfikuje otwieranie, zamykanie, dokowanie i przełączanie 7 zakładek prawego panelu,
- `Tests::uiScreenshotSmokeTest()` — wykonuje zrzuty ekranu okna offscreen w rozdzielczościach 1180x850 oraz 800x600.

Uruchomienie testów:
```bash
cd "/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/ui/build-tests"
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./tests
```

**Pełny raport wykonania:**
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
PASS   : Tests::cleanupTestCase()
Totals: 23 passed, 0 failed, 0 skipped, 0 blacklisted, 5377ms
********* Finished testing of Tests *********
```
**Podsumowanie:** Wszystkie 20 dotychczasowych testów oraz 3 nowe testy powłoki M6 przeszły w 100% (**23 passed, 0 failed**).

### 4.3 Screenshot Smoke Test Offscreen
Wygenerowano zrzut ekranu działającej aplikacji w trybie bezgłowym (offscreen):
- **Polecenie:** `QT_QPA_PLATFORM=offscreen ./astra-agent-console --screenshot /tmp/audioticon-m6-preview.png`
- **Plik wynikowy:** `/tmp/audioticon-m6-preview.png`
- **Format:** PNG image data, `1180 x 850`, 8-bit/color RGBA, rozmiar: **135 KB**.
- **Weryfikacja wizualna:** Okno renderuje pełny układ: lewa szyna NavRail, centralne StudioView z edytorem dyrektyw i listą segmentów, prawy panel ContextInspector w trybie zadokowanym oraz dolny pasek StateStrip.

---

## 5. Podsumowanie gotowości

Etap M6 został zrealizowany w pełnym zakresie. Aplikacja posiada kompletny, responsywny interfejs graficzny ASTRA Glass przygotowany pod podpięcie docelowego silnika syntezy Piper ONNX w kolejnym kroku, bez długu technologicznego i z pełną automatyczną regresją testową.
