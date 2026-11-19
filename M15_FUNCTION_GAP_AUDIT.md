# M15 — Audyt braków funkcjonalnych i architektury (Audioticon 2.0)

**Data:** 19 listopada 2026
**Zakres analizy:** `backend/` (Rust: core / bridge / cli) oraz `ui/` (C++ E1 + QML)
**Tryb pracy:** analiza bez zmian w kodzie — 0 modyfikacji plików źródłowych, 0 zmian QML, 0 commitów, bundle nietknięty, żadnych pobrań modeli ani zależności
**Dokumenty odniesienia:** `M14_RELEASE_BUNDLE_REFRESH.md`, `M14_UX_AUDIT.md`, `M5_UI_LAYOUT_SPEC.md`, `M0_ARCHITECTURE_MAP.md`, `M12_RELEASE_UX_SPEC.md`, `INTEGRATION_ANALYSIS.md`

---

## 0. Werdykt w jednym akapicie

Bundle M14 jest **technicznie poprawny, ale produktowo niekompletny**. Aplikacja jest dziś wiarygodną stacją reżyserii tekstu z jednym realnym silnikiem TTS (Piper) i solidnym rdzeniem domenowym w Rust, natomiast: (a) cztery z siedmiu modułów lewej szyny to placeholdery „w przygotowaniu”, (b) konsola czatu jest w 100% atrapą (`AgentModel::answerFor`), (c) wybór języka jest funkcjonalnie martwy, bo katalogi tłumaczeń są kluczowane tekstem angielskim, a źródła QML są polskie (244 `qsTr`, z tego 198 stringów nie ma w `pl.json`), (d) kafel First Run **przecina stopkę przy 780×560** (zmierzone), (e) katalog modeli nie jest dostępny z poziomu Qt — bridge FFI ma 8 funkcji i **zero** API modeli, głosów, STT i LLM. Warstwa P1 (lokalny LLM, Whisper, VoiceLab, własny głos) **nie istnieje w UI**, a Whisper jest tu wyłącznie DSP szeptu aktora, nie ASR.

Nie należy robić finalnego passu wizualnego przed P0 i przed fundamentem katalogu modeli — inaczej utrwalimy układ dla funkcji, których nie ma.

---

## 1. Metoda, materiał i twarde dowody

### 1.1 Co przeczytano (pełna lista obszarów)

| Warstwa | Pliki |
|---|---|
| Bridge FFI | `backend/crates/bridge/src/lib.rs` (1421 linii) |
| Rdzeń domenowy | `models.rs`, `model_hub.rs`, `audio.rs`, `parser.rs`, `presets.rs`, `cadence.rs`, `voicelab.rs`, `voice_design.rs`, `whisper_dsp.rs`, `kokoro.rs`, `auto_director.rs` |
| CLI | `args.rs`, `commands/{models,providers,voicelab,emojis,render,import,sfx}.rs` |
| C++ E1 | `agent/main.cpp`, `translator.{h,cpp}`, `bridge/{StudioBridge,AudioticonCore}`, `doctor/DoctorModel`, `wallpaper/WallpaperModel`, `agent/agentmodel.cpp`, `core/voice/DirectiveParser` |
| QML | `AgentMain`, `Main`, `StudioView`, `ContextInspector`, `NavRail`, `FirstRunWizard`, `DoctorView`, `WallpaperSettings`, `WallpaperBackground`, `MasteringStrip`, `StateStrip`, `TelemetryTile`, `ChatBubble` |
| Testy | `ui/tests/tests.cpp` (43 326 B), `backend/crates/*/tests`, testy w modułach |

### 1.2 Co zmierzono (nie „na oko”)

Pomiary wykonano na **zbudowanej binarki release** (`ui/build-release/astra-agent-console`), z tymczasowymi katalogami `XDG_CONFIG_HOME=/tmp/m15cfg`, `XDG_DATA_HOME=/tmp/m15data` (bez modelu), więc konfiguracja użytkownika pozostała nietknięta.

| Pomiar | Wynik |
|---|---|
| Zrzut 780×560 | Karta First Run 680×**520**, stopka **przycięta** — przyciski „Otwórz Doctor”, „Uruchom bez TTS”, „Rozpocznij pracę” widoczne tylko częściowo; brak `Flickable`/`ScrollView` |
| Zrzut 1180×850 | Karta 680×640, treść kończy się ~90 px nad stopką (pusty pas); wszystko widoczne |
| Zrzut 1920×1080 | Karta 680×640 — **nie skaluje się** z oknem; ten sam pusty pas, brak wykorzystania przestrzeni |
| Nagłówek „KONFIGURACJA ŚRODOWISKA I MODELI GŁOSU” na 780 px | **Mieści się** (~244 px z 624 px dostępnych) — poziomego overflow tego stringu **nie ma** |
| `qsTr` w QML | **244** wywołania; **46** obecnych w `pl.json`; **198** brakujących; **93** brakujące zawierają polskie diakrytyki |
| `--language en` vs `auto` (ten sam rozmiar) | Zrzuty **identyczne poza zegarem** → wybór języka EN nie tłumaczy ani jednego stringu UI |
| Katalogi i18n w qrc | tylko `i18n/pl.json` — brak `en.json` |
| Kafelki szyny przy 1180 | `NavRail.qml` liczy próg od `parent.width` (body ≈1144 px) → **szyna zwinięta do 56 px**, choć spec M5 §7 przewiduje pełną powyżej 1180 |

### 1.3 Czego nie weryfikowano

Kliknięć myszą, `FileDialog`, schowka, HiDPI, maszyny bez Qt, starszego glibc, realnego odtwarzania wideo jako tapety, jakości transkrypcji (funkcji nie ma). Wszystkie pomiary GUI są **offscreen**; realna sesja Wayland była sprawdzana w M14.

---

## 2. Architektura dziś — stan faktyczny

```
QML (244 qsTr, 7 modułów, 4 placeholdery)
  │  context properties: feed(AgentModel) · studio(StudioBridge) · i18n(Translator)
  │                       wallpaper(WallpaperModel) · doctor(DoctorModel)
  ▼
C++ E1 (Qt 6.12)
  ├─ DirectiveParser.cpp  ← PARSER DYREKTYW UŻYWANY PRZEZ GUI (E1)
  ├─ StudioBridge         → start_job / job_state / cancel_job / parse_directed_text
  ├─ DoctorModel          → własne skanowanie katalogów QDir (duplikat Rust)
  └─ WallpaperModel       → tapety + glass (QSettings)
  ▼
bridge (cxx) — 8 funkcji FFI
  core_version · create_engine · parse_directed_text · create_test_tone_pcm
  get_mock_render_status · notify_progress · start_job · job_state
  cancel_job · active_jobs
  ▼
Rust core — realna wartość: parser.rs, presets, cadence, audio (Piper/Kokoro/Edge/ElevenLabs),
voicelab, whisper_dsp (DSP!), model_hub (licencje), auto_director (HTTP!)
```

**Krytyczne spostrzeżenie:** rdzeń Rust jest znacznie szerszy niż to, co widzi Qt. Bridge jest wąskim pionem „render jednego rozdziału”. Dlatego wszystkie punkty P1 rozbijają się o **jedną** barierę: brak kontraktu FFI dla modeli, głosów i tekstu, a nie brak logiki w Rust.

**Druga obserwacja architektoniczna:** istnieją **dwa parsery dyrektyw** — `ui/src/core/voice/DirectiveParser.cpp` (używany przez `StudioBridge::parseDirectedText`) i `backend/crates/core/src/parser.rs` (używany przez `BridgeEngine::parse_directed_text`). To dług techniczny: każda nowa dyrektywa musi być dodana dwa razy, a rozjazd jest niewykrywalny bez golden-testu porównującego oba wyniki.

### 2.1 Powierzchnia FFI (dowód „bridge nie obsłuży katalogu”)

| Funkcja | Wejście | Wyjście | Czy dotyczy modeli/głosów? |
|---|---|---|---|
| `core_version` | — | `u32` (44100) | nie |
| `create_engine` | — | `Box<BridgeEngine>` | niejawnie wybiera pierwszy model Piper |
| `parse_directed_text` | `&str`, `bool` | `DirectedTextResultDto` | nie |
| `create_test_tone_pcm` | `f32`, `u32` | `AudioBufferDto` | nie |
| `get_mock_render_status` | `u64,u32,u32` | `RenderStatusDto` | nie |
| `notify_progress` | DTO + observer | — | nie |
| `start_job` | `&str`, `&str` | `u64` | **nie — brak wyboru modelu/głosu** |
| `job_state` / `cancel_job` / `active_jobs` | `u64` | DTO / bool / u32 | nie |

Wniosek: **obecny bridge nie może obsłużyć katalogu modeli.** Może co najwyżej wyrenderować rozdział pierwszym znalezionym plikiem `.onnx` (`resolve_piper_model`).

---

## 3. P0 — UX i stabilność (punkty 1–8)

> Uwaga metodyczna: dla każdego punktu podano dziewięć pól wymaganych w zleceniu. Ocena „stan” jest wynikiem lektury kodu **oraz** pomiaru tam, gdzie to było możliwe.

### 3.1 Overflow tekstu w kaflu „Konfiguracja środowiska i modeli głosu”

| Pole | Treść |
|---|---|
| **Stan obecny** | Karta First Run (`FirstRunWizard.qml`) ma **sztywną wysokość** `height: Math.min(parent.height - 40, 640)` i `clip: true`, a jej zawartość to `ColumnLayout` z `Item { Layout.fillHeight: true }` przed stopką — bez `Flickable`. Przy 780×560 karta ma 520 px, treść ~590 px → **stopka jest przycięta** (zmierzone). Poziomo nagłówek się mieści (244 px z 624). Podtytuł modelu ma `elide: Text.ElideRight` (dobrze), ale nagłówek nie ma `elide` ani `wrapMode` — przy dłuższym tłumaczeniu (np. „ENVIRONMENT AND VOICE MODEL SETUP” + inne języki) overflow wróci. |
| **Istniejące moduły/API** | `FirstRunWizard.qml` (22 `qsTr`), `DoctorModel.firstRunCompleted / isEnvironmentReady / completeFirstRun() / setInspectionMode()`, `FileDialog` z `selectModelFile`. |
| **Brakujące elementy** | Responsywna wysokość (`implicitHeight` + `Layout.maximumHeight`), przewijana treść (`Flickable`/`ScrollView`) poniżej ~620 px, kompaktowy wariant listy (mniejszy `implicitHeight` wierszy) dla 780×560, `elide`/`wrapMode` na nagłówku, brak martwego pasa `fillHeight` przy dużych oknach. |
| **Miejsce integracji** | `ui/src/qml/FirstRunWizard.qml` — karta i stopka; opcjonalnie wspólny komponent `GlassDialog` dla Doctor/FirstRun. |
| **Zależności** | Brak (czysty QML). Nie dotyka Rust ani FFI. |
| **Ryzyko** | Niskie. Regresja w `firstRunWizardFlow` (tests.cpp:857) możliwa, jeśli zmienimy `objectName`/strukturę; test offscreen używa 1280×800, więc nie wykryje problemu 780 — **trzeba dodać test na minimum**. |
| **Wpływ na brak HTTP/WebView** | Zerowy. |
| **Testy akceptacyjne** | (1) 780×560: wszystkie 3 przyciski stopki w pełni widoczne i `enabled` zgodnie z logiką; (2) `firstRunCard.childrenRect.height <= firstRunCard.height` dla 780/1180/1920; (3) 1180/1920: brak pasa > 24 px między treścią a stopką; (4) nagłówek `implicitWidth <= width` dla `pl`, `en`, `de` (najdłuższy testowy). |
| **Priorytet** | **P0** — blocker pierwszego kontaktu z produktem. |

### 3.2 Wybór języka aplikacji

| Pole | Treść |
|---|---|
| **Stan obecny** | Mechanizm **istnieje**: `ComboBox` w `AgentMain.qml` (kody `["auto","en","pl"]`) i w `Main.qml`, właściwość `i18n.choice` (`Translator::setChoice`, walidacja regexem), `QTranslator::translate` z obsługą liczby mnogiej (CLDR dla ~25 języków). **Ale wybór jest funkcjonalnie martwy:** katalog `pl.json` mapuje **angielskie** teksty źródłowe → polskie, podczas gdy QML ma źródła **polskie**. Zmierzone: 244 `qsTr`, tylko 46 kluczy trafia, 198 nie trafia (93 z diakrytykami). Zrzut z `--language en` jest identyczny z polskim. |
| **Istniejące moduły/API** | `translator.{h,cpp}`, `pl.json`, `resources.qrc` (tylko `pl.json`), `engine.retranslate` w `main.cpp`, `qmlRegisterType` nie dotyczy. |
| **Brakujące elementy** | Jednoznaczny kierunek katalogów: **źródła EN + `pl.json` + `en.json`** (rekomendacja) albo katalogi per-locale z polskim źródłem i `en.json`; pokrycie 100% stringów; skrypt CI wykrywający brakujące klucze; `qsTr` dla stringów nadal hardcoded (np. „VoiceLab”, „PRO STUDIO”, „FIRST-RUN”, „Brak modelu głosu” w badge’ach C++/QML); obsługa RTL (jest `setLayoutDirection`, brak testu). |
| **Miejsce integracji** | `ui/src/translator.cpp` (walidacja + fallback), `ui/src/i18n/*.json`, `ui/src/resources.qrc` (dodać `en.json`), skrypt `ui/scripts/check-i18n.*`, `AgentMain.qml`/`Main.qml` (jedno wspólne źródło listy języków zamiast dwóch kopii `["auto","en","pl"]`). |
| **Zależności** | P0-3 (wybór w First Run), P1 (nowe stringi modułów). |
| **Ryzyko** | **Średnie/wysokie organizacyjnie** — 198 stringów do przeniesienia; ryzyko rozjazdu PL/EN przy każdej zmianie UI; test `languageSwitch` (tests.cpp:223) tego nie łapie, bo sprawdza stringi EN („Edit layout”), których UI nie używa. |
| **Wpływ na brak HTTP/WebView** | Zerowy (katalogi w qrc, brak `QtNetwork`). |
| **Testy akceptacyjne** | (1) test pokrycia: każdy `qsTr` z qrc ma klucz w `en.json` i `pl.json`; (2) w trybie `en` na 3 modułach (Studio, Doctor, First Run) **zero** stringów źródłowych z polskimi diakrytykami; (3) `en` ≠ `pl` na zrzucie (hash różny poza zegarem); (4) po `retranslate` layout się nie psuje (brak `childrenRect.width > width`). |
| **Priorytet** | **P0** — funkcja istnieje pozornie, więc jest bardziej szkodliwa niż brak. |

### 3.3 Język powitalny (First Run)

| Pole | Treść |
|---|---|
| **Stan obecny** | **Brak wyboru języka w kreatorze.** Język wynika z `QSettings("language")` lub pliku Omarchy, zanim użytkownik zobaczy pierwszy ekran. Kreator jest w całości po polsku (22 `qsTr`, ale źródła PL). Użytkownik EN-first nie ma żadnej ścieżki. |
| **Istniejące moduły/API** | `i18n.choice`, `Translator::readLanguage`, `firstRunWizard` w `AgentMain.qml`, `DoctorModel.completeFirstRun()`. |
| **Brakujące elementy** | Selektor języka w nagłówku kreatora (3 segmenty: Auto / English / Polski), zapis do QSettings **przed** `completeFirstRun`, ponowne `retranslate` bez zamykania kreatora, zapamiętanie wyboru jako języka aplikacji (a nie tylko ekranu). |
| **Miejsce integracji** | `FirstRunWizard.qml` — rząd nagłówka obok badge „FIRST-RUN”; zapis przez `i18n.choice` (istniejący setter + `connect` w `main.cpp`). |
| **Zależności** | P0-2 (bez naprawy katalogów wybór nadal nic nie zmieni). |
| **Ryzyko** | Niskie technicznie, średnie UX: zmiana języka w trakcie kreatora zmienia szerokości tekstów → trzeba przetestować 780×560 po `retranslate`. |
| **Wpływ na brak HTTP/WebView** | Zerowy. |
| **Testy akceptacyjne** | (1) puste QSettings + brak modelu → kreator pokazuje wybór języka; (2) wybór EN → cały kreator po angielsku (włącznie z listą kontrolną i stopką); (3) restart → język zachowany; (4) wybór języka nie ustawia `first_run_completed`; (5) 780×560 po przełączeniu nadal bez overflow. |
| **Priorytet** | **P0**. |

### 3.4 Persystencja języka

| Pole | Treść |
|---|---|
| **Stan obecny** | **Zrobione.** `main.cpp`: odczyt `QSettings("astra-ui-agent","astra-ui-agent").value("language","auto")` → `translator.setChoice(...)`; zapis w `connect(&Translator::changed, ...)`. `Translator` przełącza `QLocale::setDefault` i `setLayoutDirection`. Wartość `auto` śledzi `~/.config/omarchy/language` przez `QFileSystemWatcher` z debounce 80 ms. |
| **Istniejące moduły/API** | `main.cpp`, `translator.cpp` (`reload`, `readLanguage`, `setChoice`), `QSettings`. |
| **Brakujące elementy** | Test restartu (obecny test nie dotyka QSettings); walidacja przy starcie jest, ale brak logowania odrzuconej wartości; brak migracji, gdy Omarchy nie istnieje (`readLanguage` → `en`, poprawne); brak zapisu języka do manifestu projektu (język jest globalny — dziś OK, ale przy projektach wielojęzycznych trzeba rozstrzygnąć). |
| **Miejsce integracji** | `ui/src/agent/main.cpp` + `translator.cpp`; ewentualnie `Paths.cpp` jeśli klucz ma trafić do wspólnej konfiguracji. |
| **Zależności** | Brak. |
| **Ryzyko** | Niskie (funkcja działa; ryzykiem jest brak testu i cicha zmiana zachowania przy refaktorze `main.cpp`). |
| **Wpływ na brak HTTP/WebView** | Zerowy. |
| **Testy akceptacyjne** | (1) `language=pl` → UI PL po restarcie; (2) `language=auto` + Omarchy `pl` → PL; (3) `language=auto` bez pliku → EN; (4) `language="xx!!"` → `auto`; (5) zmiana w trakcie działania → natychmiastowy `retranslate` bez restartu. |
| **Priorytet** | **P0 — domknięcie testem** (funkcja gotowa, dowód brak). |

### 3.5 Przezroczystość kafli

| Pole | Treść |
|---|---|
| **Stan obecny** | **Dwie niezależne osie przezroczystości.** `feed.glass` (DashboardModel, QSettings) i `wallpaper.glassOpacity` (suwak „Przezroczystość szkła”, 0.30–0.98). `AgentMain.qml` liczy `effectiveGlass = wallpaper.glassOpacity` i przekazuje je do `NavRail`, `StudioView`, `ContextInspector`, `StateStrip`, `MasteringStrip`, `ChatBubble` — **ale cztery powierzchnie modułu agenta nadal używają `feed.glass`**: lista sesji, karta transkryptu, composer, kafel placeholdera. `Main.qml` (dashboard) jest w całości na `feed.glass`. `DoctorView` ma zaszyte `Qt.rgba(..., 0.7)`. Efekt: suwak zmienia część kafli, nie zmienia innych — użytkownik widzi „niedziałającą” przezroczystość. |
| **Istniejące moduły/API** | `WallpaperModel.glassOpacity` (qBound, `saveSettings`), `WallpaperSettings.qml` (repeater suwaków, 6 parametrów), `WallpaperBackground` (MultiEffect: blur, saturation, vignette — **blur działa**), `glass` w każdym komponencie QML. |
| **Brakujące elementy** | Jedno źródło prawdy dla alfy powierzchni (usunąć `feed.glass` z powierzchni lub zbindować), spójne alpha w `DoctorView`/`FirstRunWizard`/`ContextInspector`, minimalna alpha gwarantująca kontrast tekstu, etykieta „dotyczy wszystkich paneli”. |
| **Miejsce integracji** | `AgentMain.qml` (4 miejsca `feed.glass`), `Main.qml`, `DoctorView.qml` (hardcoded 0.7), docelowo wydzielony `GlassTheme` (singleton QML) zamiast przekazywania `glass` przez 6 poziomów. |
| **Zależności** | Brak. |
| **Ryzyko** | Niskie technicznie; **średnie dla czytelności** — przy 0.30 tekst 9–10 px na jasnej tapecie może zejść poniżej WCAG AA. |
| **Wpływ na brak HTTP/WebView** | Zerowy. |
| **Testy akceptacyjne** | (1) `glassOpacity` 0.30 i 0.98 → alfa **każdej** powierzchni się zmienia (próbka piksela / `color.a` w drzewie QML); (2) brak w QML odwołań do `feed.glass` w powierzchniach; (3) kontrast tekstu ≥ 4.5:1 przy 0.30 na najjaśniejszej wpisanej tapecie; (4) wartość przeżywa restart. |
| **Priorytet** | **P0**. |

### 3.6 Zmiana tapety

| Pole | Treść |
|---|---|
| **Stan obecny** | **Rdzeń zrobiony szeroko.** `WallpaperModel`: tryby `none/static/video/rotate`, `source`, `wallpaperOpacity`, `blurRadius`, `darknessOverlay`, `saturation`, `vignette`, `pauseMotion`, `reducedMotion`, `rotationIntervalSec` (5–3600 s), `scanAssets(extraDir)`, `selectWallpaper(path, kind)`, `nextWallpaper()`, `resetDefaults()`, zapis/odczyt QSettings. `WallpaperBackground`: `Image` (PreserveAspectCrop), `MediaPlayer`+`VideoOutput` (Qt Multimedia, bez WebView), `MultiEffect` (blur/saturacja/vignette). `WallpaperSettings.qml`: przełącznik, 4 tryby, 6 suwaków, rotacja. |
| **Brakujące elementy** | **Brak UI wyboru konkretnej tapety** — `availableWallpapers` jest wyliczane, ale panel go nie renderuje; brak `FileDialog` do wskazania własnego pliku (metoda `scanAssets(extraDir)` istnieje, ale nieużywana z UI — użytkownik nie ma jak dodać swojej tapety); brak miniatur; `MediaPlayer.onErrorOccurred` tylko `console.warn` (użytkownik nie wie, że kodek nie działa); brak walidacji przy starcie, gdy `source` z QSettings już nie istnieje ( brak fallbacku → czarne tło); `scanAssets` skanuje cały `~/Downloads` (prywatność + koszt przy tysiącach plików). |
| **Miejsce integracji** | `WallpaperSettings.qml` (sekcja „Tapeta”: grid miniatur + „Wskaż plik…”), `WallpaperModel.cpp` (fallback na qrc, whitelist rozszerzeń, limit skanowania, `QFileSystemWatcher` na katalogu). |
| **Zależności** | QtMultimedia + QtQuick.Effects (już w projekcie), brak nowych pakietów. |
| **Ryzyko** | Średnie: dekodery wideo (kodek/VAAPI), koszt CPU przy 4K/60fps, prywatność skanowania Downloads, rozmiar tapet w qrc. |
| **Wpływ na brak HTTP/WebView** | Zgodne z zasadą — wszystko z plików lokalnych; **uwaga**: nie dodawać URL-i sieciowych do `source`. |
| **Testy akceptacyjne** | (1) wskazanie JPG → natychmiastowa zmiana + zapis; (2) restart → ta sama tapeta; (3) usunięcie pliku → powrót do domyślnej bez crasha; (4) MP4 → odtwarza i zapętla; (5) plik nieobsługiwany → widoczny komunikat, nie ciche `warn`; (6) rotacja 5 s przy 3 tapetach → zmiana; (7) `reducedMotion` → brak rotacji i brak animacji; (8) `scanAssets` nie skanuje Downloads, gdy wyłączone w UI. |
| **Priorytet** | **P0** (rdzeń gotowy; **brak wyboru z UI = realny brak funkcji**). |

### 3.7 Zachowanie layoutu przy 780, 1180 i 1920 px

| Pole | Treść |
|---|---|
| **Stan obecny** | Minimum 780×560. `NavRail.width` = `parent.width > 1200 ? 176 : 56` — próg liczony od **body**, nie okna → przy oknie 1180 szyna ma 56 px (spec M5 §7: >1180 → pełna 180). `ContextInspector` 320 px, dokowany gdy `win.width > 920` (spec: 900–1180 zwinięty/overlay) → przy 1180 dokowany, zawęża Studio do ~610 px i ucina przyciski (potwierdzone w M14 jako P2). `StateStrip` tylko w module `agent`, `MasteringStrip` w pozostałych (spec zakłada wspólny dolny pasek). Kolumna sesji `visible > 1060`, zegar `> 940`. Zmierzone: 780 → rail 56, inspektor overlay, karta First Run ucięta; 1180 → rail 56 (nie 176), inspektor dokowany; 1920 → rail 176, dużo pustej przestrzeni (treść nie skaluje). |
| **Istniejące moduły/API** | `AgentMain.qml` (progi), `NavRail`, `ContextInspector` (320), `StudioView` (progi 680/760), `MasteringStrip` (640/740/820), `StateStrip` (760), `Main.qml` (850/950/1040). |
| **Brakujące elementy** | Progi liczone od `win.width` (jedna macierz breakpointów: 780 / 900 / 1180 / 1600 / 1920); `Layout.minimumWidth` na paskach akcji; zawijanie (`Flow`) zamiast ucinania przy <1300; inspektor jako overlay w 900–1180; skalowanie treści powyżej 1600 (max-width kolumny tekstu); testy szerokości w CI. |
| **Miejsce integracji** | `AgentMain.qml` (źródło progów, np. `readonly property int bp: width<900?0:width<1180?1:width<1600?2:3`), `NavRail.qml`, `ContextInspector.qml`, `StudioView.qml` (pasek akcji). |
| **Zależności** | P0-1 (karta First Run), P0-5 (spójność powierzchni przy zmianie szerokości). |
| **Ryzyko** | Średnie: 32 testy Qt (część mierzy offscreen 1280×800), golden screenshoty; ryzyko regresji przy każdej zmianie progów bez macierzy. |
| **Wpływ na brak HTTP/WebView** | Zerowy. |
| **Testy akceptacyjne** | Dla 780/1180/1920 (i 2560 jako kontrola): (1) szerokość szyny zgodna z macierzą; (2) inspektor w trybie zgodnym z macierzą; (3) dla każdego paska akcji `childrenRect.width <= width` (wykrywanie ucinania); (4) wszystkie przyciski `visible && width>0`; (5) brak pustych pasów > 24 px w karcie First Run; (6) teksty nie nachodzą na siebie (porównanie `implicitWidth` etykiet). |
| **Priorytet** | **P0**. |

### 3.8 Zgodność z wcześniejszym layoutem Audioticon

| Pole | Treść |
|---|---|
| **Stan obecny** | Układ zrealizowany = 3 strefy + 7 modułów (dodany `doctor` względem specyfikacji 6). Zgodne: lewa szyna, centrum (Studio/Czat), prawy inspektor 320 px z 7 zakładkami, dolny pasek telemetrii, język ASTRA Glass, transport z realnym `playbackPosition/Duration` (pusty stan `00:00.00 / 00:00.00` — poprawny). Niezgodne: (a) szyna nie ma pełnej szerokości przy 1180 (błąd progu), (b) inspektor dokowany tam, gdzie spec chce overlay, (c) `StateStrip` zastąpiony `MasteringStrip` poza modułem agenta, (d) brak „PROJEKT: …” w nagłówku (brak modelu projektu), (e) **4 z 7 modułów to placeholdery** — layout istnieje, funkcji brak, (f) inspektor „Głosy” pokazuje zaszyte „Apache-2.0 · 114 MB” i statyczną listę profili zamiast danych z katalogu. |
| **Istniejące moduły/API** | Cała warstwa QML układu; `M5_UI_LAYOUT_SPEC.md` jako kontrakt; `M14_UX_AUDIT.md` jako ocena wizualna. |
| **Brakujące elementy** | Rozstrzygnięcie: czy `M5_UI_LAYOUT_SPEC` jest nadal wiążący (jeśli tak — naprawa progów i paska dolnego; jeśli nie — aktualizacja specyfikacji, aby uniknąć „audytu widm”). Model projektu w nagłówku (nazwa książki/rozdziału, liczba segmentów, źródło). Realne dane w inspektorze zamiast zaszytych wartości. |
| **Miejsce integracji** | `AgentMain.qml` (nagłówek), `ContextInspector.qml` (zakładki `models`, `projects`), docelowo `ProjectModel` w C++/Rust. |
| **Zależności** | P0-7, P1-11 (katalog), P1-13 (profile głosów). |
| **Ryzyko** | Średnie: „zgodność z wcześniejszym layoutem” jest dziś **niedefiniowalna bez decyzji**, czy odniesieniem jest spec M5, snapshot ASTRA E0–E1, czy donor `Portable-Local-Studio` (3 różne układy). |
| **Wpływ na brak HTTP/WebView** | Zerowy. |
| **Testy akceptacyjne** | (1) macierz zgodności: dla każdej pozycji spec M5 → `zgodne / świadomie zmienione / brak` z uzasadnieniem w repo; (2) nagłówek pokazuje realny projekt lub jawnie „Brak projektu”; (3) inspektor nie zawiera zaszytych liczb (grep w CI na `114 MB`, `100%`); (4) 7 modułów: każdy ma realny widok albo jawny, oznaczony placeholder (nie mylący). |
| **Priorytet** | **P0 — decyzja i macierz**, implementacja częściowo P1. |

---

## 4. P1 — lokalne LLM / STT / głosy / TTS / ikony (punkty 9–16)

### 4.1 Lokalne modele LLM do tekstu (reżyseria skryptu)

| Pole | Treść |
|---|---|
| **Stan obecny** | Logika reżyserii AI **istnieje i jest dobra**: `auto_director.rs` ma pełny prompt systemowy (alfabet emoji, zakaz zmiany słów, przykłady) oraz walidację pokrycia tekstu. **Transport jest jednak sieciowy**: `call_gemini_director` (`curl` → `generativelanguage.googleapis.com`), `call_openai_director` (`curl`), `call_ollama_director` (`curl -X POST http://127.0.0.1:11434/v1/chat/completions`). Brak jakiegokolwiek lokalnego runtime w procesie. |
| **Istniejące moduły/API** | `AUDIOTICON_DIRECTOR_SYSTEM_PROMPT`, `call_*_director`, `parse_gemini_response`, walidacja integralności tekstu, CLI (`auto-director` przez warstwę commands), `credentials.rs` (klucze), `wallet.rs` (koszty). |
| **Brakujące elementy** | Lokalny backend (llama.cpp przez FFI do staticlib albo `llama-cli` jako proces potomny ze stdin/stdout), loader GGUF, katalog modeli LLM, tokenizacja/limit kontekstu, szablony promptów per rodzina (ChatML, Llama-3, Qwen, Mistral), streaming tokenów, anulowanie, mapowanie błędów (brak modelu ≠ błąd HTTP), cache wyników reżyserii po hashu tekstu. |
| **Miejsce integracji** | Nowy moduł `backend/crates/core/src/llm/` (`backend.rs`, `prompt.rs`, `session.rs`), nowy namespace FFI `llm_*`, UI: `StudioView` (przycisk „Reżyseruj” z realnym postępem) + ustawienia modelu w inspektorze. |
| **Zależności** | `llama-cpp-sys`/`llama-rs` (MIT) + cmake/clang w buildzie **lub** `llama-cli` w PATH; model GGUF 1–5 GB (poza repo i poza bundle). |
| **Ryzyko** | **Wysokie**: +30–60 MB binarki, czas linkowania, ABI, brak GPU = wolno (7B q4 na CPU ~3–8 tok/s), halucynacje (mitygowane istniejącą walidacją pokrycia), licencje modeli (Llama Community, Qwen Apache-2.0), RAM. |
| **Wpływ na brak HTTP/WebView** | **Dziś łamie zasadę** (3 wywołania `curl`, w tym do zewnętrznych API). Docelowo: 0 HTTP, model w pamięci procesu lub proces potomny bez gniazd. Gemini/OpenAI wydzielić jako jawnie „wymaga sieci”, domyślnie wyłączone. |
| **Testy akceptacyjne** | (1) brak modelu → terminalny stan `LlmModelMissing`, **żadnego** wywołania `curl` (asercja na `FakeTransport`); (2) `FakeBackend` zwraca tekst → reżyseria przechodzi walidację pokrycia 100% słów; (3) model zwraca skrót → odrzucenie z `CoverageError`; (4) anulowanie w trakcie → `Cancelled` bez pliku wyjściowego; (5) test offline `unshare -n` → funkcja działa bez sieci; (6) 2 modele (3B/7B) → wybór w UI zmienia backend. |
| **Priorytet** | **P1** (po P1-11 katalog). |

### 4.2 Lokalne modele LLM do konwersacji

| Pole | Treść |
|---|---|
| **Stan obecny** | Konsola czatu jest **atrapą**: `AgentModel::answerFor()` (agentmodel.cpp:329) zwraca gotowe odpowiedzi, a `queueText` udaje streaming; komentarz w kodzie wprost mówi „in the demo this is a canned answer”. Brak historii do modelu, brak tokenów, brak narzędzi („tools” to wiersze demonstracyjne). |
| **Istniejące moduły/API** | `AgentModel` (sesje, wiadomości, `streaming`, `stop`, `newSession`, `pickSession`), `ChatBubble`, composer, skróty, `StateStrip` (kontekst/tokeny — wartości demonstracyjne). |
| **Brakujące elementy** | Backend LLM (z 4.1), zarządzanie kontekstem (system prompt + historia + budżet tokenów), streaming do QML (sygnał per chunk), anulowanie, zapis sesji na dysk (`~/.local/share/audioticon/sessions/*.json`), narzędzia lokalne (odczyt pliku projektu, lista modeli, doctor) bez HTTP, licznik kosztu/tokenów, obsługa braku modelu. |
| **Miejsce integracji** | `AgentModel` → wstrzyknięcie `LlmService` (fasada C++ nad FFI `llm_*`), zachowanie **wszystkich** `Q_PROPERTY` (kontrakt ~1200 linii QML — nie zmieniać nazw). Widok: moduł `agent`. |
| **Zależności** | 4.1, 4.3 (katalog), 4.15 (parametry generacji: temperatura, max tokenów). |
| **Ryzyko** | Średnie/wysokie: regresja 4 testów agenta (`agentTurnStreamsAndShowsTools`, `agentStopHaltsTheStream`, `agentSessionsKeepTheirOwnTranscript`, `agentConsoleRenders`); UX przy 3 tok/s (potrzebny wskaźnik tempa); kontekst rośnie → kompaktowanie. |
| **Wpływ na brak HTTP/WebView** | Zgodne, jeśli backend jest lokalny; **zakaz** podpinania Ollamy przez `curl` do UI (dziś to jedyna „działająca” ścieżka i kusi). |
| **Testy akceptacyjne** | (1) `FakeBackend` streamuje 3 chunki → 3 aktualizacje UI, `streaming=true` do końca; (2) Stop przerywa w <100 ms i zachowuje częściowy tekst; (3) brak modelu → komunikat + przycisk „Wybierz model”, brak atrapy odpowiedzi; (4) restart → sesje odtworzone z dysku; (5) 2 sesje nie mieszają historii; (6) offline test przechodzi. |
| **Priorytet** | **P1** (po 4.1; to największy „demo-smell” produktu). |

### 4.3 Katalog, wykrywanie i wybór modeli

| Pole | Treść |
|---|---|
| **Stan obecny** | Trzy równoległe światy: (a) `models.rs` — **tylko Piper**: `voices.json` z HF, `list_models()` skanuje katalog i oznacza `installed`, `download_model()` (HTTP), `remove_model()`; (b) `model_hub.rs` — katalog z licencjami (`LicenseType`, `ModelCategory`: NeuralTts/SpeechAligner/VoiceCloning/Diarization), HMAC-owe kwity akceptacji EULA; (c) `DoctorModel.cpp` — **własne** skanowanie `QDir` po `*.onnx` z heurystyką „pl_PL-bass” (duplikat logiki Rust). Do tego `ContextInspector` „Głosy” pokazuje zaszyte „Apache-2.0 · 114 MB”. **Bridge nie ma żadnego API modeli.** |
| **Istniejące moduły/API** | `models_dir()`, `list_models()`, `find_model()`, `download_model()`, `remove_model()`, `update_voices_catalog()`, `ModelHubEntry`, `ReceiptStore`, `DoctorModel.installedModelsCount/activeModelName/modelDetails`. |
| **Brakujące elementy** | `ModelKind { Piper, Kokoro, Whisper, Llm }`; DTO `ModelEntryDto { id, kind, path, size_bytes, sha256, lang, license_spdx, license_type, installed, valid, missing_companions }`; FFI: `list_models()`, `scan_models()`, `import_model(path, kind)`, `validate_model(id)`, `remove_model(id)`, `select_model(id)`; rozdzielenie katalogów `models/{piper,kokoro,asr,llm}/`; walidacja par (`.onnx` + `.json` dla Piper; `.bin` + opcjonalny `.json` dla Whisper; `.gguf` dla LLM); manifest `manifest.json` per katalog z licencją i źródłem; deduplikacja między katalogami; `ModelCategory` rozszerzone o `Asr` i `Llm`. |
| **Miejsce integracji** | `core/src/models/` (split: `catalog.rs`, `scan.rs`, `registry.rs`), `bridge/src/lib.rs` (nowe DTO + funkcje), `DoctorModel.cpp` **deleguje** do bridge (usuwa duplikat), UI: `ContextInspector > Głosy` + nowy widok „Modele”. |
| **Zależności** | `sha2`, `serde`, `walkdir` (dodać) — bez nowych ciężkich bibliotek. |
| **Ryzyko** | Średnie: migracja istniejących instalacji (pliki leżą dziś płasko w `models/`), rozjazd C++/Rust (rozwiązany delegacją), wydajność skanowania dużych katalogów. |
| **Wpływ na brak HTTP/WebView** | Katalog **offline-first**: skanowanie dysku, brak sieci w UI. `download_model()` zostaje **tylko w CLI** i musi być jawnie oznaczone jako operacja sieciowa. |
| **Testy akceptacyjne** | (1) fixture: 1 Piper + 1 Whisper + 1 LLM → `list_models` zwraca 3 z poprawnym `kind`; (2) `.onnx` bez `.json` → `valid=false, missing_companions=["json"]`; (3) plik 0 B → `valid=false`; (4) duplikat w dwóch katalogach → jeden wpis (lub dwa z ostrzeżeniem — decyzja); (5) brak katalogu → pusta lista, nie błąd; (6) `DoctorModel.installedModelsCount` == liczba z Rust (test zgodności); (7) UI pokazuje liczniki per typ. |
| **Priorytet** | **P1.0 — fundament** (blokuje 4.1, 4.2, 4.4, 4.5). |

### 4.4 Whisper speech-to-text

| Pole | Treść |
|---|---|
| **Stan obecny** | **Nie istnieje.** `whisper_dsp.rs` to DSP **szeptu aktora** (macierze transferu 24-pasmowe, LPC, tryby narracyjny/akcja/romantyczny/close-mic) — nazwa koliduje z ASR i już raz wprowadziła w błąd (`M0` §5). Brak whisper.cpp, brak `transcribe`, brak komendy CLI, brak modelu `.bin`. |
| **Istniejące moduły/API** | `audio.rs` (ffmpeg, resampling, `read_wav_samples`, `create_silence_wav`), `hound`, `importer.rs` (EPUB/DOCX/MD — import **tekstu**, nie audio), `model_hub` (wpis „whisper-tiny-aligner” jako planowany), `subtitles.rs` (formaty napisów). |
| **Brakujące elementy** | Silnik ASR, wykrywanie modeli GGML (`.bin`) + `kind=Whisper`, chunking >30 s z zachowaniem czasów, VAD (pomijanie ciszy), mapowanie segmentów ASR → segmenty scenariusza Audioticon (z opcją „zachowaj pauzy”), wybór języka (`pl`/`en`/auto), timestamps do napisów, postęp, anulowanie. |
| **Miejsce integracji** | `core/src/asr/` (`whisper.rs`, `chunk.rs`, `vad.rs`) + FFI `transcribe_file(path, model_id, lang) -> TranscriptDto` + UI: moduł `projects` („Wczytaj nagranie lektora”) i `StudioView` („Zamień nagranie na scenariusz”). |
| **Zależności** | `whisper-cli` (MIT) jako proces potomny z `-oj` (JSON) **albo** `whisper-rs` (FFI, bindgen+cmake); model 75 MB (tiny) … 1.5 GB (large-v3); `ffmpeg` do 16 kHz mono (już jest). Rekomendacja na pierwszy pion: **proces `whisper-cli`** — zero HTTP, zero nowego linkowania, łatwe mockowanie. |
| **Ryzyko** | **Wysokie**: RTF 0.3–2 na CPU, jakość PL (large-v3 wymagany dla sensownej jakości), licencje modeli, rozmiar, mylenie z `whisper_dsp` w nazwach → wymagany rename `whisper_dsp` → `actor_whisper_dsp` (lub dokumentacja). |
| **Wpływ na brak HTTP/WebView** | Zgodne: proces lokalny + pliki; **żadnego** `whisper-server` (HTTP) w aplikacji. |
| **Testy akceptacyjne** | (1) brak modelu → `AsrModelMissing`, **nie** pusty transkrypt i nie „sukces”; (2) 3 s ciszy → tekst pusty, `status=ok`, czas 3.0 s ±0.2; (3) fixture 10 s mowy → tekst zawiera znane słowo, czasy ±200 ms; (4) anulowanie → `Cancelled`, brak pliku; (5) 70 s audio → chunking, brak utraty słów na granicach (join z zakładką 0.5 s); (6) dwa modele (tiny/base) → wybór działa; (7) brak `whisper-cli` w PATH → jawny błąd instalacji z komendą. |
| **Priorytet** | **P1** (po 4.3); pierwszy pion tylko plik, bez mikrofonu. |

### 4.5 Import i rejestracja głosu lektora

| Pole | Treść |
|---|---|
| **Stan obecny** | **Backend gotowy, UI nie istnieje.** `voicelab.rs`: `VoiceProfile` (id, name, gender, language, description, samples, embedding, consent), `VoiceSampleMeta` (czas, SR, kanały, RMS, peak, SNR, clipping, data), `SampleQualityReport` (status + tips), `convert_to_standard_wav`, `analyze_wav_samples`, `add_sample_to_profile`, `punch_in_sentence`, `export_dataset_zip`, `delete_voice_profile`. CLI `voicelab` obsługuje list/create/add-sample/punch-in/clone/export/delete/design. W GUI: moduł `voicelab` = „Moduł w przygotowaniu”. Brak FFI. |
| **Istniejące moduły/API** | `voicelab.rs` (782 linie), `voices_dir()` = `~/.local/share/audioticon/voices`, `profile_dir(id)`, `sanitize_slug`, CLI `voicelab`. |
| **Brakujące elementy** | FFI: `list_voice_profiles`, `create_voice_profile`, `add_sample(id, path)`, `analyze_sample(path) -> QualityDto`, `delete_voice_profile`, `export_dataset(id)`; UI `VoiceLab.qml` (lista profili, drag&drop WAV, waveform, ocena jakości z tipami, przycisk „Nagraj”); powiązanie profilu z silnikiem TTS (Piper **nie klonuje** — profil to metadane + referencja); UI zgody (`consent` istnieje w modelu, brak ekranu); **licencja próbek** (patrz 4.13/6.4). |
| **Miejsce integracji** | `bridge` (DTO `VoiceProfileDto`, `VoiceSampleDto`, `QualityReportDto`), nowy `ui/src/qml/VoiceLab.qml` + wpis w `NavRail` (istnieje), `ContextInspector > Głosy` (realna lista zamiast statycznej). |
| **Zależności** | 4.3 (katalog), `hound`/`zip` (są), `ffmpeg` (konwersja), opcjonalnie QtMultimedia do nagrywania (jest). |
| **Ryzyko** | Średnie: RODO/zgoda, duże pliki (dataset), konwersja 48k stereo → 44.1k mono bez utraty jakości, brak UI do nagrywania (tylko import). |
| **Wpływ na brak HTTP/WebView** | Zgodne: wszystko lokalne; **klonowanie ElevenLabs** (patrz 4.6) to jedyna ścieżka sieciowa — musi być jawnie oznaczona i domyślnie wyłączona. |
| **Testy akceptacyjne** | (1) import WAV 48 kHz stereo → profil z próbką 44.1 kHz mono; (2) 16 s → `is_too_short`, status `warning` z tipami; (3) clipping → `status=error` + tip; (4) usunięcie profilu → kaskadowe usunięcie katalogu próbek; (5) restart → profile widoczne; (6) brak katalogu → utworzony automatycznie; (7) export ZIP → zawiera WAV + metadane JSON; (8) UI: 0 profili → stan pusty z akcją, nie błąd. |
| **Priorytet** | **P1** (po 4.3; najtańsza duża funkcja — backend już jest). |

### 4.6 Własny głos użytkownika

| Pole | Treść |
|---|---|
| **Stan obecny** | `VoiceProfile.embedding: Option<Vec<f32>>` (256-wymiarowy) **istnieje w strukturze, ale nigdy nie jest wyliczany** — brak jakiegokolwiek ekstraktora. Klonowanie dostępne wyłącznie przez ElevenLabs: `voicelab::clone_to_elevenlabs` i `voice_design::create_voice_design_previews` — oba `curl` do `api.elevenlabs.io`, wymagają klucza i sieci. |
| **Istniejące moduły/API** | `VoiceProfile.embedding`, `clone_to_elevenlabs(profile_id, api_key)`, `voice_design` (preview/design), `credentials.rs`, `wallet.rs` (spend 50). |
| **Brakujące elementy** | Lokalny ekstraktor embeddingu (speaker-encoder ONNX, np. ECAPA/WavLM), lokalny zero-shot TTS (XTTS/Piper finetune/Kokoro voice pack), mapowanie embedding → warunki syntezy, cache embeddingu, znakowanie „głos syntetyczny”, consent + dowód zgody, fallback do Piper. |
| **Miejsce integracji** | `core/src/voice/embedding.rs`, `core/src/voice/clone.rs`, integracja z `PiperBridge` (post-processing barwy) lub osobny silnik; UI: `VoiceLab` („Utwórz mój głos z 3 próbek”). |
| **Zależności** | speaker-encoder 20–100 MB (licencja!), model zero-shot (ciężki, często **non-commercial** → `LicenseType::NonCommercialOnly` i bramka EULA z `model_hub`), ewentualnie `ort` (już używany przez Kokoro). |
| **Ryzyko** | **Bardzo wysokie**: jakość PL, licencje (większość zero-shot TTS jest NC), etyka (deepfake), rozmiar, czas inferencji, oczekiwania użytkownika („mój głos” ≠ „mój głos brzmi jak ja”). |
| **Wpływ na brak HTTP/WebView** | ElevenLabs **łamie offline** — oznaczyć jako opcję sieciową, domyślnie wyłączoną, z ostrzeżeniem; ścieżka lokalna bez HTTP. |
| **Testy akceptacyjne** | (1) brak modelu → `CloneUnavailable` (nie pusty plik); (2) 2 próbki tego samego mówcy → cosine ≥ 0.8; (3) różnych mówców → ≤ 0.6; (4) brak `consent` → operacja zablokowana; (5) wyjście oznaczone metadanymi „synthetic voice”; (6) offline test: funkcja lokalna działa bez sieci, ElevenLabs zwraca jawny błąd sieci. |
| **Priorytet** | **PÓŹNIEJ** (po wdrożeniu P1 i visual passie). Wyjątek: „profil lektora = metadane + referencja” należy do 4.5. |

### 4.7 Tempo, pauzy, wysokość i parametry TTS

| Pole | Treść |
|---|---|
| **Stan obecny** | **Rdzeń bogaty, UI prawie nie istnieje.** Parser: `EmojiAction::{Pause, Breath, RateMod, PitchMod, VolumeMod, Emotion, Filter}`, `CadenceConfig` (clause 160 ms, sentence 350 ms, oddechy), style produkcji. `PiperBridge`: `length_scale` z `rate_mod`, `volume`, pitch realizowany przez `ffmpeg asetrate+atempo` (półtony = `pitch_mod/3`). CLI: `--style`, `--cadence`, `--breath-intensity`, `--bit-depth`. GUI: `ContextInspector` pokazuje Tempo/Filtr DSP/Pauzę/SHA-256 **tylko do odczytu** dla wybranego segmentu; brak kontrolek globalnych; brak edycji parametrów per segment; brak podglądu „na sucho”. |
| **Istniejące moduły/API** | `parser.rs`, `cadence.rs`, `presets.rs`, `audio.rs` (`PiperBridge` parametry), `ContextInspector.segmentData`, `StudioBridge` (brak API parametrów). |
| **Brakujące elementy** | FFI: `set_render_params(params)`, `preview_segment(index, params)`; DTO `RenderParamsDto { rate, pitch, volume, clause_pause_ms, sentence_pause_ms, breath, style }`; UI: panel „Głos i tempo” (4 suwaki + preset), edycja per segment w inspektorze, reset, zapis presetów użytkownika; **hash cache musi uwzględniać parametry** inaczej dostaniemy fałszywy HIT. |
| **Miejsce integracji** | `StudioView.qml` (panel nad composerem), `ContextInspector.qml` (zakładka Inspektor → edycja), `bridge` (nowe funkcje), `cache.rs` (klucz hasha). |
| **Zależności** | Zakres parametrów zależy od silnika (Piper: `length_scale`; Kokoro: własne; Edge: SSML; ElevenLabs: `stability`) → potrzebny `capabilities()` w traitcie. |
| **Ryzyko** | Średnie: artefakty przy dużym pitch (ffmpeg), cache HIT z błędnym kluczem = najbardziej podstępny błąd produktu, rozjazd C++/Rust parsera. |
| **Wpływ na brak HTTP/WebView** | Zerowy. |
| **Testy akceptacyjne** | (1) rate 0.8 → czas ×1.25 ±5%; (2) pitch +6 → F0 +~2 semitony, długość ±2%; (3) pauza 900 ms → cisza w WAV (analiza amplitudy); (4) zmiana parametru → **cache MISS** (nowy hash); (5) identyczne parametry → HIT; (6) UI → render używa wartości z panelu (porównanie DTO); (7) reset → wartości domyślne; (8) preset użytkownika przeżywa restart. |
| **Priorytet** | **P0/P1 granica — rekomendacja: P0.5**, bo to nie brak funkcji, a brak UI nad gotowym rdzeniem; tanie i widoczne dla użytkownika. Pełny zakres (presety, per-segment) w P1. |

### 4.8 Projektowanie zachowań własnych ikon

| Pole | Treść |
|---|---|
| **Stan obecny** | Rdzeń **gotowy**: `CustomEmojiDef { name, alias, action_type (filter/voice/modifier), filter, voice, pitch, rate }`, `EmojiDirector::register_custom_emoji_def`, `ProjectConfig.emojis` z `audioticon.toml`, CLI `emojis add/list/remove`. GUI: **zero** — brak edytora, brak podglądu, a `parseDirectedText` w GUI używa **C++** `DirectiveParser`, który nie zna definicji użytkownika → własne ikony nie działają w aplikacji mimo działającego CLI. |
| **Istniejące moduły/API** | `presets.rs` (`CustomEmojiDef`, rejestr), `parser.rs` (`register_custom_emoji_def`), `emojis` CLI, `DirectiveParser.cpp/.h` (E1), `ProjectConfig`. |
| **Brakujące elementy** | FFI: `list_custom_emojis()`, `add_custom_emoji(def)`, `remove_custom_emoji(emoji)`; **przekazanie definicji do C++ E1** (albo C++ czyta `audioticon.toml` sam — rekomendacja: czyta, bo parser zostaje po stronie E1); UI edytor (picker emoji, nazwa, alias, akcja, parametry, podgląd syntezy); zapis do projektu vs globalnie; wykrywanie konfliktów z wbudowanymi; dokumentacja alfabetu. |
| **Miejsce integracji** | `ui/src/core/voice/DirectiveParser.cpp` (ładowanie `audioticon.toml`), nowy widok „Dyrektywy” lub rozszerzenie `StudioView`, `ContextInspector`. |
| **Zależności** | 4.7 (parametry akcji), 4.5 (głos jako cel akcji `voice`), 4.3 (lista modeli dla `voice`). |
| **Ryzyko** | Średnie: **dwa parsery** (C++ i Rust) — definicja dodana w jednym może nie działać w drugim; golden-test zgodności jest obowiązkowy. |
| **Wpływ na brak HTTP/WebView** | Zerowy. |
| **Testy akceptacyjne** | (1) dodanie 🧙→`cathedral`+pitch −4 w UI → render stosuje filtr (analiza widma); (2) alias `[czarodziej]` działa; (3) usunięcie → przestaje działać; (4) konflikt z wbudowanym → ostrzeżenie, nie cicha nadpiska; (5) restart → definicje zachowane; (6) **golden**: dla 10 skryptów `DirectiveParser` (C++) i `parser.rs` dają identyczny `DirectedScript`. |
| **Priorytet** | **P1**. |

---

## 5. Odpowiedzi na pytania szczegółowe zlecenia

### 5.1 Czy obecny Rust bridge może obsłużyć katalog lokalnych modeli?

**Nie.** Bridge eksponuje 10 sygnatur (8 funkcji + `notify_progress` + `get_mock_render_status`), z czego żadna nie dotyczy modeli: `start_job(text, output_dir)` nie przyjmuje modelu ani głosu, a `create_engine()` wybiera model niejawnie przez `resolve_piper_model()` (env → katalog XDG → trzy znane nazwy). `JobStateDto` nie ma pola modelu. Rdzeń **ma** logikę (`models.rs`, `model_hub.rs`), więc praca to **owinięcie, nie przepisanie**: nowe DTO (`ModelEntryDto`, `ModelKindDto`) + 6 funkcji + zmiana `start_job` na przyjmowanie `model_id` (z zachowaniem kompatybilności: puste = dotychczasowe zachowanie).

### 5.2 Jak dodać Whisper bez serwera?

Trzy warianty, w kolejności rekomendacji:

1. **`whisper-cli` (GGML) jako proces potomny** — `Command::new("whisper-cli").args(["-m", model, "-f", wav16k, "-oj", "-of", out, "-l", lang])`, parsowanie JSON-a, postęp z `stderr`. Zero HTTP, zero nowego linkowania, trywialne mockowanie (podstawiany skrypt w testach), ale wymaga binarki w PATH (doctor check).
2. **`whisper-rs` / `whisper.cpp` przez FFI do staticlib** — najlepsza integracja i wydajność, ale +~5–15 MB, bindgen/cmake w buildzie, komplikuje `cargo build --release --locked`.
3. **`whisper-server`** — **odrzucone**: to HTTP, łamie zasadę „brak HTTP”.

Niezależnie od wariantu: wejście zawsze przez `ffmpeg -ar 16000 -ac 1`, chunking 30 s z zakładką 0,5 s, wynik jako `TranscriptDto { segments: [{start_ms, end_ms, text}], language, model_id }`.

### 5.3 Jak rozdzielić modele Piper, Whisper i LLM?

- `ModelKind` w rdzeniu i w DTO; katalogi `models/piper/`, `models/kokoro/`, `models/asr/`, `models/llm/` (dziś wszystko płasko w `models/` — wymaga migracji z wykrywaniem starych ścieżek).
- Osobne moduły Rust: `synthesis/` (istniejące `audio.rs` + `kokoro.rs`), `asr/` (nowe), `llm/` (nowe). **Zakaz** wspólnego pola `model_path` w DTO bez `kind`.
- Osobne namespace’y FFI: `synth_*`, `asr_*`, `llm_*` — dzięki temu zmiana w LLM nie dotyka sygnatur renderu.
- Wspólny tylko `ModelEntryDto` i rejestr licencji (`model_hub`), bo to metadane, nie inferencja.

### 5.4 Jak przechowywać metadane głosów i licencji?

- **Model**: rozszerzyć `ModelHubEntry` o `kind` i `installed_path`; licencja zostaje `{spdx, type, summary, evidence_url, terms_fingerprint}` + kwit HMAC (`LicenseReceipt`) — wzorzec już działa i jest bezpieczny.
- **Głos**: `VoiceProfile` + nowe `license: VoiceLicense { spdx, source, evidence_url, consent_at, consent_scope }`; próbki: `VoiceSampleMeta` + `sha256` + `captured_at` + `consent`.
- **Dysk**: `voices/<id>/profile.json` + `samples/*.wav` + `LICENSE.txt` wygenerowany z metadanych; `models/<kind>/manifest.json` z listą plików i sum.
- **Bramka**: każda operacja „użyj do syntezy komercyjnej” sprawdza `LicenseType` i kwit — inaczej UI musi pokazać ostrzeżenie.

### 5.5 Jak zachować tryb offline?

1. Zero `QtNetwork` w UI; brak nowych zależności sieciowych w QML/C++.
2. Wszystkie ścieżki sieciowe w rdzeniu **za cechą/flagą** (`features = ["online"]` lub funkcje `*_online`), domyślnie wyłączone w buildzie release.
3. Pobieranie modeli tylko CLI (`audioticon models download`) — jawnie, z potwierdzeniem; UI tylko import z pliku.
4. Test CI: `unshare -n` (lub `firejail --net=none`) uruchamia ścieżki UI; każdy test kończy się sukcesem bez sieci.
5. Doctor: sekcja „tryb offline” weryfikująca brak otwartych gniazd (`ss -tp` dla PID) — opcjonalnie, ale mocno dowodowe.

### 5.6 Jak nie mieszać funkcji Piper, Kokoro, Whisper i LLM?

- `trait SpeechSynthesizer` już istnieje — rozszerzyć o `capabilities() -> SynthCaps { pitch, rate, volume, styles, languages }`, a UI renderuje tylko wspierane kontrolki.
- Osobne `enum` błędów per domena (`SynthesisError`, `AsrError`, `LlmError`) mapowane na jeden `BridgeError` z kodem — żeby „brak modelu Whisper” nie był raportowany jako „brak modelu głosu Piper”.
- Doctor: 4 niezależne kafle (Piper, Kokoro/ORT, ASR, LLM) z własnymi statusami; dziś kafel 6 mówi o Kokoro, ale nie ma kafla ASR/LLM.
- Nazewnictwo: `whisper_dsp.rs` → `actor_whisper_dsp.rs` (albo `style/whisper.rs`), żeby „whisper” znaczyło wyłącznie ASR w nowym kodzie.

### 5.7 Jak testować brakujące modele bez fałszywego sukcesu?

Wzorzec **B13** już istnieje w bridge: gdy nie ma modelu, `start_job` uruchamia `spawn_fallback_no_model` i kończy stanem błędu, a **nie** `Finished` z plikiem ciszy (placeholder `MOCK_RENDER_MS` jest osiągalny tylko z `AUDIOTICON_MOCK_RENDER=1` albo `new_mock()`). To jest wzorzec do skopiowania 1:1:

1. Każdy nowy backend ma **trzy** stany terminalne: `Finished`, `Failed`, `ModelMissing` — `ModelMissing` nigdy nie tworzy artefaktu.
2. Fixture „missing” w testach: pusty katalog modeli + asercja `stage == Failed` i `output_path.is_empty()`.
3. **Zakaz** generowania plików zastępczych w ścieżce release (mock tylko za env/feature).
4. Testy negatywne w CI dla każdej funkcji: STT bez modelu, LLM bez modelu, klonowanie bez embeddingu, render bez `.json`.
5. Doctor pokazuje dokładnie ten sam powód, który zwraca rdzeń (wspólny kod błędu, nie dwa niezależne skany).
6. Asercja „brak sieci” przy błędzie modelu: `ModelMissing` nie może wywołać `curl`.

---

## 6. Proponowana architektura docelowa (bez implementacji)

```
models/
  piper/   <name>.onnx + <name>.onnx.json + manifest.json
  kokoro/  model.onnx + voices.bin      + manifest.json
  asr/     ggml-*.bin                   + manifest.json
  llm/     *.gguf                       + manifest.json
voices/
  <id>/profile.json + samples/*.wav + LICENSE.txt + embedding.bin
sessions/ <id>.json
```

- **Rdzeń domenowy zostaje w Rust** — C++/QML tylko woła FFI; parser dyrektyw zostaje w E1, ale **definicje** (custom emoji) są wspólnym plikiem `audioticon.toml`, czytanym przez oba parsery.
- **Bridge**: 3 nowe namespace’y (`models_*`, `asr_*`, `llm_*`) + rozszerzone `start_job(text, output_dir, model_id, params)`.
- **UI**: `GlassTheme` (jedno źródło alphy), `ModelBrowser` (współdzielony dla 4 typów), `VoiceLab`, `DirectiveEditor`, `LlmSettings`.
- **Zakaz**: nowych serwerów HTTP, WebView, `QProcess` z `curl` w ścieżkach produktowych.

---

## 7. Macierz priorytetów (zbiorczo)

| # | Punkt | Priorytet | Stan | Główna praca |
|:--:|---|:--:|---|---|
| 1 | Overflow kafla First Run | **P0** | zepsute (zmierzone) | QML: wysokość + scroll |
| 2 | Wybór języka | **P0** | pozornie działa | i18n: kierunek katalogów, 198 stringów |
| 3 | Język powitalny | **P0** | brak | QML: selektor w kreatorze |
| 4 | Persystencja języka | **P0** | **działa** | testy restartu |
| 5 | Przezroczystość kafli | **P0** | częściowo (2 osie) | QML: jedno źródło alphy |
| 6 | Zmiana tapety | **P0** | rdzeń OK, brak UI wyboru | QML + model: lista/FileDialog |
| 7 | Layout 780/1180/1920 | **P0** | progi błędne | QML: macierz breakpointów |
| 8 | Zgodność z poprzednim layoutem | **P0** | częściowo | decyzja + macierz zgodności |
| 15 | Parametry TTS | **P0.5 → P1** | rdzeń OK, brak UI | FFI params + panel + hash cache |
| 11 | Katalog modeli | **P1.0** | rozproszony, brak FFI | rdzeń + FFI + delegacja Doctor |
| 16 | Własne ikony | **P1** | rdzeń + CLI, brak GUI | FFI/TOML + edytor + golden |
| 13 | Import głosu | **P1** | backend gotowy | FFI + `VoiceLab.qml` |
| 12 | Whisper STT | **P1** | brak | `asr/` + `whisper-cli` |
| 9 | LLM tekst | **P1** | HTTP tylko | `llm/` + llama.cpp FFI |
| 10 | LLM konwersacja | **P1** | atrapa | `LlmService` + AgentModel |
| 14 | Własny głos | **Później** | embedding martwy | modele lokalne, licencje |

---

## 8. Kolejność wdrożenia (wymagana sekwencja)

### Etap 1 — P0 UX i stabilność (M16)
1. First Run: responsywna karta + scroll + wybór języka (1, 3).
2. i18n: decyzja o kierunku katalogów, migracja stringów, pokrycie 100%, test CI (2), testy persystencji (4).
3. Jedno źródło alphy szkła + usunięcie `feed.glass` z powierzchni (5).
4. Tapeta: grid miniatur + „Wskaż plik…” + fallback przy braku pliku (6).
5. Macierz breakpointów liczona od `win.width` + zawijanie pasków akcji (7).
6. Macierz zgodności ze spec M5 i decyzja o zakresie odchyleń (8).
**Bramka:** zrzuty 780/1180/1920 bez przycięć; test pokrycia i18n zielony; brak `feed.glass`; 0 regresji w 32 testach Qt.

### Etap 2 — P1 lokalne LLM / STT / głosy / TTS / ikony (M17–M19)
Kolejność wymuszona zależnościami:
1. **11 katalog modeli** (`ModelKind`, DTO, FFI, delegacja Doctora, migracja katalogów) — fundament.
2. **15 parametry TTS** (DTO + panel + hash cache) — szybki zysk, potrzebne przez 16.
3. **16 własne ikony** (TOML + edytor + golden C++/Rust).
4. **13 import głosu** (`VoiceLab.qml` + FFI; backend gotowy).
5. **12 Whisper STT** (`asr/` + `whisper-cli`, tylko plik).
6. **9 LLM tekst** (`llm/` + llama.cpp, reżyseria).
7. **10 LLM konwersacja** (`LlmService`, zastąpienie `answerFor`).
8. **14 własny głos** — dopiero po decyzji o modelu i licencji (poza P1).
**Bramka etapu:** każda funkcja ma stan „model missing” bez artefaktu; doctor ma 4 niezależne kafle; test offline (`unshare -n`) zielony.

### Etap 3 — testy integracyjne (M20)
- Macierz: 4 typy modeli × 3 stany (brak / uszkodzony / OK) × 3 rozdzielczości.
- End-to-end: tekst → reżyseria LLM → render Piper → eksport M4B; nagranie → Whisper → scenariusz → render.
- Cache: HIT/MISS z parametrami; brak fałszywych HIT-ów.
- Offline: pełny przepływ bez sieci; próba funkcji sieciowej → jawny komunikat.
- Wydajność: 10 min tekstu, RAM, czas startu z 100 modelami w katalogu.
- Regresja: 179 testów Rust + 32 Qt + nowe.

### Etap 4 — finalny visual pass (Fable / Hydrogen / Carbon)
Dopiero tutaj: typografia, hierarchia, dopracowanie Glass, mikroruch, ikonografia, spójność 3 motywów. **Warunek:** brak otwartych P0 i zamknięty etap 3 — inaczej pass utrwali układ dla nieistniejących funkcji (dokładnie to, przed czym ostrzega zlecenie).

---

## 9. Ryzyka przekrojowe

| ID | Ryzyko | Wpływ | Mitygacja |
|---|---|---|---|
| R1 | **Dwa parsery dyrektyw** (C++ E1 i Rust) | rozjazd zachowania, własne ikony działają w CLI, nie w GUI | golden-test zgodności na 10 skryptach; definicje w jednym TOML |
| R2 | i18n pozornie działa (testy na stringach EN) | produkt „polsko-only” mimo przełącznika | test pokrycia + zrzuty per język |
| R3 | Katalog modeli w C++ (Doctor) i Rust (models.rs) | różne liczby/statusy w UI i CLI | delegacja Doctora do FFI |
| R4 | LLM przez `curl` do Ollamy | jedyna „działająca” ścieżka łamie offline | flaga `online`, domyślnie off; FFI jako ścieżka produktowa |
| R5 | `whisper_dsp` vs Whisper ASR | mylenie funkcji, złe nazwy w API | rename przed dodaniem ASR |
| R6 | Brak modeli w bundle | każda funkcja P1 „nie działa” na czystej maszynie | jawny stan `ModelMissing` + doctor per typ |
| R7 | Cache bez parametrów w hashu | fałszywe HIT-y po zmianie tempa/pitchu | parametr w kluczu + test MISS |
| R8 | Licencje modeli (NC, Llama Community) | ryzyko prawne przy komercji | kwity HMAC + bramka `LicenseType` |

---

## 10. Ograniczenia tego audytu

- Wszystkie zrzuty wykonano **offscreen**; realna sesja Wayland nie była powtarzana (była w M14).
- Nie testowano kliknięć, `FileDialog`, schowka, HiDPI, wideo-tapety, maszyny bez Qt.
- Nie weryfikowano jakości Whisper/LLM (funkcji nie ma) — oceny ryzyka oparte na rozmiarze modeli i architekturze.
- Ocena „zgodności z wcześniejszym layoutem” wymaga decyzji, które z trzech odniesień (spec M5, snapshot E0–E1, donor Portable-Local-Studio) jest wiążące — bez tego punkt 8 pozostaje częściowo nierozstrzygalny.
- Nie zmieniono żadnego pliku źródłowego, QML, bundle ani manifestu; nie wykonano commitów; nie pobrano modeli ani zależności.
