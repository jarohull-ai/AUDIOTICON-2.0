# Raport M16 — Implementacja P0 UX i powrót do Director Workstation (Audioticon 2.0)

**Data wdrożenia:** 19 listopada 2026  
**Zakres:** Wyłącznie etap **P0** w [`ui/`](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui) (bez zmian w rdzeniu Rust, bez commitów, bez P1 i bez końcowego visual passu Fable/Hydrogen/Carbon).  
**Raporty referencyjne:**
- [M15_PRODUCT_DIRECTION_RESET.md](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/M15_PRODUCT_DIRECTION_RESET.md)
- [M15_FUNCTION_GAP_AUDIT.md](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/M15_FUNCTION_GAP_AUDIT.md)

---

## 1. Zrealizowane wymagania P0

### 1.1 Naprawa pionowego overflow First Run (780×560, 1180×850, 1920×1080)
Plik: [FirstRunWizard.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/FirstRunWizard.qml)
- Wprowadzono układ trójsekcyjny wewnątrz modalnej karty Glass (`objectName: "firstRunCard"`):
  1. **Przypięty nagłówek** (`headerRow`) z tytułem, plakietką i przełącznikiem języka startowego.
  2. **Przewijalna treść** (`ScrollView` z `Layout.fillHeight: true`, `clip: true`) zawierająca opis, 5 kafli listy kontrolnej i ramkę prywatności.
  3. **Przypięta stopka akcji** (`objectName: "firstRunFooter"`) — przyciski *Otwórz Doctor*, *Uruchom bez TTS* i *Rozpocznij pracę* są **zawsze dostępne i nieobcięte** nawet przy minimalnym oknie `780×560`.
- Usunięto sztywne wysokości kafli (`implicitHeight: 46`) na rzecz dynamicznego `Math.max(58, modelRow.implicitHeight + 18)` — podtytuł kafla „Model głosu Piper” już nie wychodzi poza dolną ramkę.
- Wysokość karty dopasowuje się do zawartości (`Math.min(parent.height - 28, Math.max(420, idealHeight))`), eliminując martwy pusty pas w `1180×850` i `1920×1080`.

### 1.2 Kompletny system i18n (PL ↔ EN, język na pierwszym ekranie, persystencja, Doctor/C++)
Pliki: [en.json](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/i18n/en.json), [pl.json](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/i18n/pl.json), [resources.qrc](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/resources.qrc), [DoctorModel.cpp](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/doctor/DoctorModel.cpp), [main.cpp](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/agent/main.cpp) oraz wszystkie pliki `ui/src/qml/*.qml`.
- **Ujednolicenie kluczy źródłowych:** 100% wywołań `qsTr(...)` w QML oraz `QCoreApplication::translate(...)` w C++ używa stabilnych kluczy angielskich (**0 kluczy źródłowych z polskimi diakrytykami**).
- **Pełne katalogi:** dodano `i18n/en.json` (374 klucze) oraz uzupełniono `i18n/pl.json` (374 klucze) — **0 brakujących kluczy** w obu katalogach.
- **Wybór języka na pierwszym ekranie:** w nagłówku `FirstRunWizard.qml` dodano segmentowany przełącznik `[ Auto | EN | PL ]`, który natychmiast przelicza cały interfejs (`QQmlEngine::retranslate()`) bez zamykania kreatora i zapisuje wybór w `QSettings`.
- **Tłumaczenie komunikatów C++ / Doctor / Studio:** `DoctorModel` kieruje wszystkie szczegóły podsystemów i raport tekstowy przez `QCoreApplication::translate("DoctorModel", ...)`, a `Translator::changed` w `main.cpp` automatycznie odświeża `DoctorModel` i `WallpaperModel`. Baner błędu braku modelu w `StudioView.qml` mapuje komunikat błędu Rust/bridge na klucz `qsTr(...)`.

### 1.3 Przywrócenie prawdziwego szkła (Glass) i spójnego `glassOpacity`
Pliki: [WallpaperModel.h](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/wallpaper/WallpaperModel.h), [WallpaperModel.cpp](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/wallpaper/WallpaperModel.cpp), [WallpaperBackground.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/WallpaperBackground.qml), [AgentMain.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/AgentMain.qml), [DoctorView.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/DoctorView.qml)
- Usunięto nieprzezroczyste `#080c11` z głównego kontenera `AgentMain.qml` (`color: "transparent"`) oraz z `WallpaperBackground.qml` (tryb bez tapety używa `Qt.rgba(0.025, 0.042, 0.060, wpGlass)`).
- Skalibrowano domyślne wartości dla czytelnego, widocznego szkła (`wallpaperOpacity: 0.35`, `darknessOverlay: 0.50`, `glassOpacity: 0.72`, `blurRadius: 14.0`, `saturation: 0.85`).
- Usunięto wszystkie pozostałe odwołania do `feed.glass` na powierzchniach w `AgentMain.qml` (sesje, transkrypt, composer, placeholder) oraz zahardkodowane `0.7` w `DoctorView.qml` — **jeden spójny parametr `wallpaper.glassOpacity` steruje wszystkimi panelami**.

### 1.4 Uporządkowanie Ustawień (jeden panel pod `⚙`, miniatury tapet, fallback po usunięciu pliku)
Pliki: [AgentMain.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/AgentMain.qml), [WallpaperSettings.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/WallpaperSettings.qml), [WallpaperModel.cpp](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/wallpaper/WallpaperModel.cpp)
- Przycisk `⚙` otwiera teraz **jeden spójny, przewijalny panel Ustawień** (`unifiedSettingsDrawer`) zawierający wybór **Języka** oraz pełny panel **Tapety i Szkła (`WallpaperSettings`)**.
- Dodano siatkę **miniatur lokalnych tapet** (`GridView`, `objectName: "wallpaperThumbnailGrid"`) z podglądem obrazów, kartami wideo `MP4`, aktywną ramką, przyciskiem odświeżania (`↻`) i natywnym `FileDialog`.
- W `WallpaperModel` dodano `validateSource()` oraz `fallbackNotice`: po usunięciu wybranego pliku tapety z dysku aplikacja bez crasha i bez czarnego ekranu wraca do wbudowanej tapety `qrc:/assets/wallpapers/pexels-artyusufpatel-8998289.jpg` i wyświetla czytelny komunikat.

### 1.5 Powrót do charakteru Director Workstation
Pliki: [NavRail.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/NavRail.qml), [ContextInspector.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/ContextInspector.qml), [StudioView.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/StudioView.qml), [DoctorView.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/DoctorView.qml)
- `NavRail` liczy próg zwijania od szerokości okna (`win.width >= 1180 ? 176 : 56`) z jawnym `Layout.preferredWidth` — pełna szyna przy `1180×850` i zwięzła `56 px` przy `780×560`.
- `ContextInspector`: pasek zakładek zmieniono z ukrytego scrolla na zawijany `Flow`, dzięki czemu wszystkie 7 zakładek są od razu widoczne i klikalne. Usunięto zahardkodowane makiety głosów (`Apache-2.0 · 114 MB`) — zakładki `Voices`, `Projects` i `Export` uczciwie pokazują realny stan `DoctorModel` oraz informację o planowanych funkcjach P1.
- `StudioView`: baner błędu braku modelu i jego 3 przyciski akcji przeniesiono do osobnego wiersza `Flow` pod kompozytorem (zero ucinania przy `1180×850` i `780×560`).
- `DoctorView`: pasek 5 przycisków akcji zmieniono na `Flow` (zero ucinania przy otwartym inspektorze).
- Stopka skrótów w `AgentMain.qml` zaktualizowana do `Ctrl+1..7`.

---

## 2. Wyniki testów (QtTest offscreen)

Polecenie:
```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./tests
```

Wynik: **36 passed, 0 failed, 0 skipped, 0 blacklisted (13 719 ms)** (wszystkie 32 dotychczasowe testy zachowane + 4 nowe testy regresji M16 P0):
- `Tests::firstRunNoOverflowAt780x560()` — **PASS** (zweryfikowana geometria karty, stopki i podtytułu modelu w `780×560`, `1180×850` i `1920×1080`)
- `Tests::i18nSwitchPlEnAndRestartPersistence()` — **PASS** (przełączenie PL ↔ EN na żywo w QML + `DoctorModel` + persystencja w `QSettings` po symulowanym restarcie)
- `Tests::glassOpacityUnifiedAndWallpaperVisibility()` — **PASS** (spójna zmiana `glassOpacity` na `NavRail`, `StudioView`, `DoctorView`, `ContextInspector`, widoczność tapety oraz przezroczystość tła bez `#080c11`)
- `Tests::wallpaperSelectionThumbnailsAndDeletedAssetFallback()` — **PASS** (wybór lokalnego pliku, fallback po usunięciu assetu z dysku, obecność miniatur i języka w `unifiedSettingsDrawer`)

Kompilacja binarki standalone `astra-agent-console` (`ui/build-console`): **exit 0, 0 błędów, 0 ostrzeżeń**.

---

## 3. Wygenerowane zrzuty ekranu

- First Run `780×560`: [`/tmp/audioticon-m16-firstrun-780x560.png`](file:///tmp/audioticon-m16-firstrun-780x560.png)
- First Run `1180×850`: [`/tmp/audioticon-m16-firstrun-1180x850.png`](file:///tmp/audioticon-m16-firstrun-1180x850.png)
- First Run `1920×1080`: [`/tmp/audioticon-m16-firstrun-1920x1080.png`](file:///tmp/audioticon-m16-firstrun-1920x1080.png)
- Director Workstation `780×560`: [`/tmp/audioticon-m16-780x560.png`](file:///tmp/audioticon-m16-780x560.png)
- Director Workstation `1180×850`: [`/tmp/audioticon-m16-1180x850.png`](file:///tmp/audioticon-m16-1180x850.png)
- Director Workstation `1920×1080`: [`/tmp/audioticon-m16-1920x1080.png`](file:///tmp/audioticon-m16-1920x1080.png)

---

## 4. Zakres świadomie niewdrożony (zgodnie z ograniczeniami M16)
- Brak implementacji P1: lokalne LLM, Whisper STT, pełny VoiceLab, własne głosy, edytor własnych ikon.
- Brak finalnego visual passu (Fable / Hydrogen / Carbon).
- Brak zmian w rdzeniu Rust (`backend/`), brak commitów git, zero HTTP i zero WebView.
