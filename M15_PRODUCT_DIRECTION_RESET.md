# AUDIOTICON M15 — Reset kierunku produktu i audyt odstępstw

**Data:** 19 listopada 2026  
**Status:** Dokument kierunkowy i audyt odstępstw (bez zmian w kodzie i QML)  
**Ocena bazowa produktu przez użytkownika:** **6 / 10**  
**Zastrzeżenie formalne:** Raport [M14_UX_AUDIT.md](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/M14_UX_AUDIT.md) weryfikował wyłącznie techniczne działanie *First Run* oraz *Doctor* w izolacji. Jego wynik `PASS` **nie oznacza akceptacji całego produktu** ani zatwierdzenia obecnego wyglądu jako finalnego.

**Analizowane materiały źródłowe:**
- [M5_UI_LAYOUT_SPEC.md](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/M5_UI_LAYOUT_SPEC.md)
- [M9_UI_VISUAL_DIRECTION.md](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/M9_UI_VISUAL_DIRECTION.md)
- [M10_UI_WALLPAPER_PREMIUM.md](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/M10_UI_WALLPAPER_PREMIUM.md)
- [M12_RELEASE_UX_SPEC.md](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/M12_RELEASE_UX_SPEC.md)
- [M14_UX_AUDIT.md](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/M14_UX_AUDIT.md)
- [M14_RELEASE_BUNDLE_REFRESH.md](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/M14_RELEASE_BUNDLE_REFRESH.md)
- Kod bieżący w [ui/src/](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src)

---

## Diagnoza główna: dlaczego obecny stan to 6/10

Aplikacja posiada solidny fundament inżynieryjny (natywny Rust + FFI cxx 44 100 Hz, C++17 E1 `DirectiveParser`, `QAudioSink`, brak HTTP/WebView, 179 testów Rust i 32 testy Qt), ale na poziomie **produktu i UX** zeszła z kursu:

1. **Dryf tożsamości:** Zamiast wyspecjalizowanej, lokalnej stacji roboczej do reżyserii dźwięku (*Director Workstation*), aplikacja zaczęła przypominać generyczną demo-aplikację w stylu Google/Gemini Studio (sztywne ciemne karty, rozproszone badge'e, placeholderowe moduły i odziedziczony demo-czat ASTRA).
2. **Niespełnione obietnice P0 (jakość podstawowa):** Przepełnienie tekstu poza ramkę w kreatorze startowym, zepsuta zmiana języka (polskie stringi źródłowe w `qsTr` bez katalogu `en.json`), brak realnej przezroczystości kafli (efektywna widoczność tła ~1,26%) i ukryty panel tapet, którego nie ma pod przyciskiem `⚙`.
3. **Brak kluczowych funkcji P1 (rdzeń wartości produktu):** Brak lokalnego wyboru modeli LLM, brak Whisper STT, brak importu głosów i obsługi własnego głosu, szczątkowa regulacja parametrów TTS oraz brak projektowania własnych zachowań ikon.

**Wniosek:** Zanim wykonamy jakikolwiek końcowy szlif wizualny (*last touch*), musimy **zamknąć wszystkie błędy P0** oraz **zbudować realne funkcje P1**.

---

## A. Macierz odstępstw (Założenie vs Stan obecny vs Odstępstwo)

| # | Obszar | Oryginalne założenie Audioticon | Stan obecny (fakty z kodu i UI) | Odstępstwo (Root Cause) | Priorytet | Kryterium akceptacji |
|---|---|---|---|---|:---:|---|
| 1 | **First Run — kafel modelu** | Czyste, nieobcięte karty Glass z wyraźnymi marginesami w każdej rozdzielczości. | W [FirstRunWizard.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/FirstRunWizard.qml#L290-L341) dolny kafel „Model Głosu Piper” ma sztywne `implicitHeight: 46` przy `margins: 10`; dwuwierszowy tekst i przycisk Material wypychają czerwony podtytuł poza dolny obrys kafla. | Zbyt mała sztywna wysokość kontenera zamiast dynamicznego `Math.max(..., row.implicitHeight + 20)`. | **P0** | Zero wychodzenia tekstu poza obrys w 780×560, 1180×850 i 1920×1080 (weryfikacja screenshotem i testem geometrii). |
| 2 | **Zmiana języka (i18n)** | Pełne, działające w locie przełączanie języka aplikacji (PL ↔ EN) w całym UI i modelach C++. | [translator.cpp](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/translator.cpp#L82-L120) zakłada angielskie klucze źródłowe i ma tylko `pl.json`. Tymczasem QML (M6–M13) ma polskie teksty w `qsTr(...)`, a `DoctorModel.cpp` używa surowego polskiego `QStringLiteral`. Brak `en.json`. | Przełączenie na *English* zmienia ~10% starych napisów ASTRA, a ~90% aplikacji (Studio, Doctor, First Run, Inspector) pozostaje po polsku. | **P0** | 100% napisów QML i C++ przełącza się natychmiast między PL i EN bez restartu; test automatyczny weryfikuje oba języki. |
| 3 | **Język powitalny (First Run)** | Użytkownik świadomie wybiera język na samym początku lub aplikacja respektuje wybrany język powitalny. | [FirstRunWizard.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/FirstRunWizard.qml) natychmiast wita użytkownika po polsku i nie posiada przełącznika języka na ekranie startowym. | Brak selektora języka na górze ekranu powitalnego i polskie stringi bazowe. | **P0** | Wyraźny wybór języka (`Polski / English / Auto`) na górze ekranu powitalnego przed pierwszym krokiem konfiguracji; natychmiastowe przeliczenie UI. |
| 4 | **Przezroczystość kafli (Glass)** | Prawdziwe, kontrolowane szkło ASTRA Glass (widoczna głębia i subtelna przezroczystość kafli, opcjonalnie przezroczystość okna pod Hyprland). | [AgentMain.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/AgentMain.qml#L90) i [WallpaperBackground.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/WallpaperBackground.qml#L28) używają w 100% nieprzezroczystego `#080c11`, a domyślne parametry (`wallpaperOpacity: 0.24`, `darknessOverlay: 0.62`, `glassOpacity: 0.86`) dają ~1,26% widoczności tła pod kaflami. | Kafle wyglądają jak całkowicie płaskie, nieprzezroczyste ciemne prostokąty; szkło jest niewidoczne. | **P0** | Regulacja przezroczystości kafli jest wyraźnie widoczna na żywo (od czytelnego szkła do pełnego matu) przy zachowaniu kontrastu tekstu ≥ WCAG AA. |
| 5 | **Zmiana tapety i dostęp do ustawień** | Łatwa, natychmiastowa zmiana tapety (brak / obraz / wideo / rotacja) i parametrów szkła z głównego przycisku ustawień. | Przycisk `⚙` w [AgentMain.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/AgentMain.qml#L202-L205) otwiera stary drawer ASTRA z samym wyborem języka. [WallpaperSettings.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/WallpaperSettings.qml) jest ukryte w 6. zakładce prawego panelu, a pasek zakładek ma `ScrollBar.horizontal.policy: ScrollBar.AlwaysOff` ([ContextInspector.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/ContextInspector.qml#L86)). | Użytkownik nie ma jak odkryć ani zmienić tapety i szkła z poziomu `⚙`, a w prawym panelu zakładki 5–7 są niewidoczne bez poziomego scrolla. | **P0** | Jedne, spójne Ustawienia (`⚙` i zakładka) z czytelnym wyborem tapety, trybu, przezroczystości i podglądem na żywo; brak ukrytych zakładek. |
| 6 | **Tożsamość Director Workstation** | Profesjonalna, mroczna stacja reżyserii dźwięku (DAW / sprzęt audio), zero generycznego „AI Studio”. | 4 z 7 modułów (`voicelab`, `projects`, `sfx`, `ledger`) to placeholdery; `Czat AI` to stary demo-skrypt ASTRA; płaskie karty i rozproszone komunikaty przypominają generyczny dashboard AI. | Dryf w stronę „AI demo template” zamiast narzędzia inżyniera dźwięku i lektora. | **P0** | Usunięcie resztek demo-ASTRA ze Studia, spójna hierarchia racków audio, brak obcinania kontrolek w 1180×850 (Studio i Doctor). |
| 7 | **Status „finalnego wyglądu”** | Wygląd zatwierdzany dopiero po zamknięciu funkcji i realnym przeglądzie użytkownika. | Raport M14 dał ocenę `PASS`, co może sugerować gotowość wizualną. | Błędne utożsamienie testu technicznego First Run/Doctor z akceptacją całego produktu. | **P0** | Formalne zamrożenie „last touch” do czasu zaliczenia bramki P0 + P1 i jawnej akceptacji użytkownika. |
| 8 | **Lokalne modele LLM** | Wybór lokalnych modeli LLM do pracy z tekstem, redakcji i konwersacji bez chmury i HTTP. | [AgentModel](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/agent/agentmodel.cpp) zwraca statyczne odpowiedzi ze skryptu demo (`answerFor()`). Brak katalogu i wyboru modeli LLM. | Brak realnego LLM i brak katalogu lokalnych modeli. | **P1** | Katalog lokalnych modeli LLM (np. GGUF), wybór aktywnego modelu, parametry kontekstu i prawdziwa konwersacja lokalna bez HTTP. |
| 9 | **Whisper Speech-to-Text (STT)** | Lokalna transkrypcja mowy na tekst (dyktowanie, import nagrań, pętla głosowa). | Brak jakiejkolwiek integracji Whisper/STT w UI i warstwie bridge. | Całkowity brak ścieżki wejścia głosowego (audio → tekst). | **P1** | Lokalny moduł Whisper STT (dyktowanie z mikrofonu + transkrypcja plików audio → segmenty scenariusza), bez HTTP i chmury. |
| 10 | **Import / dodawanie głosu lektora** | Proste dodawanie nowych głosów lektorskich do lokalnej biblioteki z weryfikacją i odsłuchem. | Zakładka `Głosy` w [ContextInspector.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/ContextInspector.qml#L326-L371) ma zahardkodowaną listę makiet (`gosia`, `darkman`) i martwy przycisk „Wybierz”; moduł `VoiceLab` to placeholder. | Brak prawdziwego katalogu i mechanizmu importu głosów. | **P1** | Działający `VoiceLab` i panel głosów: skanowanie XDG, import `.onnx` + `.json`, metadane, licencja, odsłuch próbki, wybór aktywnego lektora. |
| 11 | **Obsługa własnego głosu** | Możliwość pracy z własnym głosem użytkownika (własny model / profil / próbki referencyjne). | Brak profili własnych głosów i brak ścieżki importu/nagrywania próbek referencyjnych. | Produkt ograniczony wyłącznie do gotowych zewnętrznych modeli Piper. | **P1** | Dedykowany przepływ „Własny głos”: import własnego modelu lub rejestracja zestawu lokalnych próbek WAV z kontrolą jakości (SNR/LUFS). |
| 12 | **Regulacja parametrów syntezy TTS** | Precyzyjna reżyseria tempa, pauz, wysokości, intonacji i DSP (globalnie i per segment). | W [StudioView.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/StudioView.qml#L80-L91) dostępnych jest tylko 10 sztywnych żetonów emoji bez suwaków i bez edycji wartości liczbowych. | Zbyt mała kontrola nad brzmieniem mowy — brak precyzji wymaganej od stacji reżyserskiej. | **P1** | Suwaki/kontrolki tempa (`0.5x–2.0x`), pauz (zdanie/akapit/rozdział w ms), wysokości/intonacji (`pitch`, `noise_scale`, `noise_w`) oraz gain/DSP per segment. |
| 13 | **Projektowanie zachowań własnych ikon** | Użytkownik może tworzyć własne ikony/dyrektywy i przypisywać im akcje reżyserskie (głos, tempo, pauza, DSP, SFX). | Lista 10 emoji jest twardo wpisana w QML i C++ bez edytora użytkownika. | Brak możliwości personalizacji języka reżyserskiego pod własne książki, role postaci i efekty. | **P1** | Edytor własnych ikon/dyrektyw zapisywany w `$XDG_CONFIG_HOME/audioticon/directives.json`, wczytywany przez C++ E1 `DirectiveParser` z natychmiastowym podglądem. |

---

## B. Nowy kierunek produktu (Reset do Director Workstation)

### 1. Powrót do tożsamości: Audioticon Director Workstation
Audioticon 2.0 to **lokalna stacja reżyserii dźwięku, głosu i tekstu** (hybryda lekkiego DAW + warsztatu lektorskiego + lokalnego asystenta tekstowo-głosowego).
- **Czym JEST:** Narzędziem precyzyjnym — inspirowanym ergonomią sprzętu studyjnego (racki, szyny sygnałowe, mierniki LUFS/Peak, przejrzyste przełączniki stanu, osie czasu i kontrolki parametrów).
- **Czym NIE JEST:** Generycznym „AI Studio / Gemini dashboardem” z dużymi kartami powitalnymi, marketingowymi plakietkami, nieaktywnymi kafelkami i chatem jako głównym ekranem.

### 2. Architektura trójstrefowa (przywrócenie dyscypliny z M5/M9)
- **Lewa szyna (`NavRail`):** Zwarty, przewidywalny przełącznik realnych modułów produkcyjnych (`Studio`, `VoiceLab`, `Rozmowa lokalna / LLM`, `Whisper STT`, `Projekty`, `ACX Ledger`, `Doctor`). Koniec z pustymi ekranami „w przygotowaniu” dla kluczowych funkcji.
- **Centralna przestrzeń pracy:** 100% skupienia na aktywnym zadaniu (np. w Studio: *Kompozytor reżyserii + Edytor parametrów → Oś segmentów z waveformami → Transport*). Zero kolidujących pasków i zero ucinania przycisków w standardowym oknie 1180×850.
- **Prawy kontekstowy drawer (`ContextInspector`):** Prawdziwie kontekstowy panel narzędziowy (pokazuje narzędzia adekwatne do zaznaczonego segmentu, głosu, kolejki lub diagnostyki) z **widoczną nawigacją zakładek** (ikonowy pasek lub zawijane zakładki — nigdy ukryty scroll bez paska).
- **Dolny pasek masteringu (`MasteringStrip`):** Kompaktowa telemetria audio (aktywny silnik/głos, sample rate, cache, LUFS, True Peak, status ACX).

### 3. Szkło (Glass) jako narzędzie czytelności, nie dekoracja
- Szkło służy do budowania **hierarchii głębi** (tło aplikacji / tapeta → główne panele robocze → aktywne karty/modale), a nie do efektownego rozmazywania tekstu.
- **Naprawa matematyki szkła:**
  - Usunięcie sztywnego, nieprzezroczystego `#080c11` tam, gdzie blokuje przezroczystość kompozytora lub tapety.
  - Sensowne wartości domyślne: tapeta dyskretnie widoczna (np. efektywne 18–25% przez szkło przy zachowaniu kontrastu tekstu ≥ 7:1), a suwak **Przezroczystość kafli** realnie zmienia prześwit między `0.45` a `0.98`.
  - Tryb **Bez tapety / Czyste szkło systemowe** pozwala na autentyczną przezroczystość okna pod Hyprland/Wayland.

### 4. Kontrolowane tapety i spójne Ustawienia
- Jedno źródło prawdy: przycisk `⚙` otwiera **pełne Ustawienia Audioticon** (Język, Wygląd/Tapeta/Szkło, Audio/Urządzenia, Ścieżki XDG, Skróty), zamiast starego jednoelementowego drawera z ASTRA.
- Zmiana tapety (brak / obraz / wideo MP4 / rotacja / wybór własnego pliku) działa natychmiastowo z podglądem na żywo i bezpiecznym trybem *Reduced Motion*.

### 5. Zero generycznych wzorców „AI dashboard”
- Usunięcie pozostałości po szablonie demonstracyjnym ASTRA (canned script odpowiedzi, napisy typu „Console settings / demo script”, zbędne puste placeholdery).
- Wszystkie komunikaty, stany i etykiety mówią językiem **produkcji audio i lokalnego warsztatu**.

---

## C. Specyfikacja UX brakujących funkcji (P0 & P1)

### 1. Wybór języka aplikacji i języka powitalnego (P0)
- **Ekran powitalny (First Run):** Na samej górze karty — jeszcze nad opisem — znajduje się wyraźny segmentowany przełącznik: `[ Polski | English | Auto ]`. Kliknięcie natychmiast przeładowuje teksty kreatora i całej aplikacji.
- **Architektura i18n:**
  - Ujednolicenie wszystkich kluczy `qsTr(...)` i `tr(...)` na spójny język bazowy (EN) + kompletne katalogi `src/i18n/en.json` i `src/i18n/pl.json`.
  - Usunięcie nieprzetłumaczalnych polskich `QStringLiteral(...)` z `DoctorModel.cpp` i `StudioBridge.cpp` (zastąpienie kluczami tłumaczonymi lub kodami stanu mapowanymi w QML przez `qsTr`).
  - Persystencja wyboru w `QSettings` i natychmiastowe `QQmlEngine::retranslate()`.

### 2. Katalog lokalnych modeli LLM (P1)
- **Lokalizacja:** `$XDG_DATA_HOME/audioticon/llm/` + możliwość wskazania dodatkowego katalogu lokalnego (np. istniejąca biblioteka plików `.gguf`).
- **Widok katalogu (w module Rozmowa / Ustawienia LLM):**
  - Lista wykrytych modeli z metadanymi: nazwa, format/kwantyzacja (`Q4_K_M`, `Q8_0`), rozmiar na dysku, okno kontekstu, szacowany RAM/VRAM, licencja.
  - Przycisk `[ + Dodaj model z dysku ]` (bez automatycznego pobierania z sieci).
  - Wybór roli modelu: *Redakcja / Reżyseria tekstu* vs *Konwersacja ogólna*.

### 3. Rozmowa lokalna bez HTTP (P1)
- **Architektura bez HTTP:** Natywny worker lokalny komunikujący się przez FFI (Rust/C++ `llama.cpp`) lub dedykowany proces potomny po lokalnym IPC (`stdin/stdout` JSON-lines). **Zero portów TCP, zero serwera HTTP, zero WebView.**
- **Przepływ produkcyjny:**
  - Czat wspiera pracę nad książką/scenariuszem (np. streszczenie, podział na dialogi, propozycja dyrektyw emocjonalnych).
  - Przycisk **„🎙️ Przeczytaj w studiu”** faktycznie przekazuje wybraną treść (lub zaznaczony fragment) do edytora `StudioView`, parsuje go przez C++ E1 i przygotowuje do odsłuchu.
  - Jawny status runtime w nagłówku modułu: aktywny model GGUF, zużycie kontekstu, prędkość generowania (tok/s), przycisk `Stop`.

### 4. Whisper / Speech-to-Text (STT) (P1)
- **Architektura bez HTTP:** Lokalny silnik `whisper.cpp` (FFI lub lokalny worker IPC) korzystający z lokalnych modeli `ggml-*.bin` w `$XDG_DATA_HOME/audioticon/whisper/`.
- **3 scenariusze UX:**
  1. **Dyktowanie w Studio:** Przycisk mikrofonu `[ 🎤 Dyktuj ]` w pasku kompozytora — nagrywa głos przez natywne `QAudioSource`, transkrybuje lokalnie i wstawia tekst w pozycję kursora.
  2. **Import nagrania do scenariusza:** Wybór pliku WAV/FLAC/MP3 → lokalna transkrypcja z podziałem na zdania i znacznikami czasu → konwersja bezpośrednio na listę segmentów w Studio.
  3. **Pętla głosowa offline (opcjonalna w module Rozmowa):** Mikrofon → Whisper STT → lokalny LLM → Piper TTS → `QAudioSink`.

### 5. Katalog i import głosów lektorskich (P1)
- **Prawdziwy `VoiceLab` (zastępuje placeholder) + zsynchronizowana zakładka `Głosy`:**
  - Automatyczne skanowanie `$XDG_DATA_HOME/audioticon/models/` oraz `$XDG_DATA_HOME/audioticon/voices/`.
  - Karta głosu pokazuje: nazwę, język, silnik (Piper ONNX / inne lokalne), częstotliwość próbkowania, rozmiar, obecność pliku `.onnx.json`, licencję oraz przycisk **▶ Odsłuch próbki**.
  - **Kreator importu głosu (`+ Importuj głos`):**
    1. Wskazanie pliku `.onnx` (automatyczne wykrycie towarzyszącego `.onnx.json` lub prośba o jego wskazanie).
    2. Podgląd metadanych i nadanie etykiety / roli (np. *Narrator*, *Postać A*).
    3. Kopiowanie lub dowiązanie symboliczne do katalogu XDG + natychmiastowy test syntezy 1 zdania.

### 6. Obsługa własnego głosu (Custom Voice) (P1)
- **Sekcja „Własny głos” w `VoiceLab`:**
  - **Ścieżka A (Gotowy własny model):** Import własnego, lokalnie wytrenowanego modelu głosu (`.onnx` + `.json`) z oznaczeniem `USER VOICE / PRIVATE`.
  - **Ścieżka B (Profil próbek referencyjnych):** Nagranie (przez `QAudioSource`) lub import lokalnych próbek WAV (10–60 s) własnego głosu z automatyczną, lokalną analizą jakości (poziom głośności LUFS, szum tła, clipping, sample rate) i zapisem profilu w `$XDG_DATA_HOME/audioticon/voices/<nazwa>/`. Profil służy jako gotowy zestaw wejściowy dla lokalnych silników klonowania/adaptacji głosu bez wysyłania głosu do chmury.

### 7. Strojenie parametrów TTS (P1)
- **Dwa poziomy kontroli (Globalny profil głosu + Nadpisanie per-segment w Inspektorze Segmentu):**
  - **Tempo mowy:** suwak `0.50x – 2.00x` (krok `0.05x`, mapowany na `length_scale`).
  - **Pauzy:** suwaki w ms — *Pauza po zdaniu* (`0–2000 ms`), *Pauza przed segmentem*, *Pauza po akapicie/rozdziale*.
  - **Wysokość i ekspresja:**
    - *Wysokość głosu (Pitch):* `-6` do `+6` półtonów (lokalny DSP pitch-shift bez zmiany tempa),
    - *Ekspresyjność / wariancja (`noise_scale`):* `0.0 – 1.2`,
    - *Zróżnicowanie rytmu (`noise_w`):* `0.0 – 1.2`.
  - **Głośność i DSP segmentu:** gain (`-12 dB` do `+6 dB`), wybór filtra (`Brak`, `Radio`, `Telefon`, `Katedra`, `Szept`) + suwak intensywności (*Wet/Dry*).
  - Przycisk `[ ▶ Odsłuchaj segment z nowymi parametrami ]` i `[ ↺ Resetuj do domyślnych głosu ]`.

### 8. System projektowania zachowań własnych ikon (P1)
- **Cel:** Zastąpienie sztywnych 10 emoji programowalnym, lokalnym systemem reżyserii.
- **Konfiguracja:** Plik `$XDG_CONFIG_HOME/audioticon/directives.json` współdzielony między UI a natywnym parserem **C++ E1 `DirectiveParser`** (nadal jeden parser w C++, bez duplikacji w Ruście).
- **Edytor zachowań ikon (w Studio / Ustawieniach reżyserii):**
  - Lista wszystkich ikon (wbudowane presety + własne ikony użytkownika).
  - Formularz tworzenia/edycji ikony:
    - **Symbol / Ikona** (wybór emoji lub glifu),
    - **Nazwa i podpowiedź** (np. *„Krótkie zawahanie”*, *„Głos z krótkofalówki”*, *„Postać: Detektyw”*),
    - **Zakres działania:** punktowy (wstawia pauzę/oddech/SFX) lub blokowy (zmienia parametry kolejnych zdań do następnej dyrektywy),
    - **Akcje składowe (można łączyć):** zmiana tempa, zmiana pitch, pauza przed/po (ms), filtr DSP + Wet/Dry, zmiana aktywnego głosu lektora, wstawienie lokalnego pliku SFX,
    - **Skrót klawiszowy** (opcjonalny) i przycisk **▶ Testuj ikonę na zdaniu próbnym**.

---

## D. Plan realizacji (P0 → P1 → P2)

### Faza P0 — Naprawa fundamentów, jakości i tożsamości (KRYTYCZNA)
*Cel: usunąć wszystkie defekty psujące pierwsze wrażenie i przywrócić dyscyplinę Director Workstation.*
1. **Layout & zero overflow:**
   - Naprawa wysokości dolnego kafla w `FirstRunWizard.qml` (dynamiczna wysokość z marginesem).
   - Przeniesienie banera błędu braku modelu i jego 3 przycisków w `StudioView.qml` do osobnego, nieobcinanego wiersza.
   - Rozwiązanie ciasnoty paska zakładek w `ContextInspector.qml` (widoczne wszystkie zakładki bez ukrytego scrolla) oraz paska akcji w `DoctorView.qml`.
   - Korekta stopki skrótów `Ctrl+1..7`.
2. **Kompletny system językowy (i18n) + język powitalny:**
   - Przełącznik języka na górze `FirstRunWizard.qml`.
   - Ujednolicenie wszystkich kluczy `qsTr`/`tr` w QML i C++ (`DoctorModel`, `StudioBridge`, `StudioView`, `DoctorView`, `ContextInspector`, `NavRail`) + kompletne `en.json` i `pl.json`.
   - Test automatyczny sprawdzający przełączenie PL ↔ EN na żywo.
3. **Prawdziwa przezroczystość kafli Glass i spójny panel tapet:**
   - Odblokowanie przezroczystości bazowej okna/kafli i kalibracja domyślnych parametrów szkła (wyraźnie odczuwalna regulacja przezroczystości przy zachowaniu kontrastu).
   - Scalenie ustawień pod `⚙` (Język + Tapeta i Szkło + Audio + XDG w jednym spójnym miejscu).
4. **Reset tożsamości Director Workstation:**
   - Usunięcie artefaktów „ASTRA demo console” z głównego strumienia pracy.
   - Wyraźne oddzielenie funkcji aktywnych od modułów planowanych bez udawania gotowych ekranów makietami.

### Faza P1 — Wdrożenie rdzenia funkcji produktu Audioticon
*Cel: dostarczyć realną wartość produkcyjną, której dziś brakuje.*
1. **Katalog i import głosów + `VoiceLab`:** dynamiczne skanowanie XDG, import `.onnx` + `.json`, odsłuch próbki, usunięcie zahardkodowanych makiet.
2. **Strojenie parametrów TTS:** globalne i per-segment kontrolki tempa, pauz, pitch, `noise_scale`, `noise_w`, gain i DSP (przekazywane przez `StudioBridge` do silnika Rust/Piper).
3. **System własnych zachowań ikon:** `directives.json` + rozszerzenie C++ E1 `DirectiveParser` + edytor ikon w UI z podglądem na żywo.
4. **Obsługa własnego głosu:** import własnego modelu użytkownika + rejestrator/importer próbek referencyjnych WAV z lokalną analizą jakości.
5. **Whisper STT (lokalny, bez HTTP):** dyktowanie w Studio + transkrypcja plików audio na segmenty scenariusza.
6. **Lokalny katalog LLM i rozmowa bez HTTP:** skanowanie lokalnych modeli GGUF, wybór modelu, lokalny worker IPC/FFI (zero HTTP), realne przekazywanie tekstu do Studia.

### Faza P2 — Końcowy „Last Touch” (Final Visual Pass)
*Cel: wyłącznie po 100% zamknięciu P0 i P1 — ostateczne doszlifowanie estetyki.*
- Dopracowanie mikro-typografii, skali ikon, stanów hover/focus, rytmu odstępów, animacji przejść i finalnej palety materiałów Glass.
- Dopiero na tym etapie następuje wybór modelu do końcowego szlifu wizualnego.

---

## E. Kryteria „Gotowe do Last Touch” (Gate Checklist)

Przejście do fazy **P2 (last touch)** jest **zablokowane**, dopóki wszystkie poniższe punkty nie uzyskają statusu `PASS`:

- [ ] **Wszystkie 7 punktów P0 zaliczone:** zero obcinania tekstu w `FirstRunWizard`, działająca pełna zmiana języka, wybór języka na ekranie startowym, realna i regulowana przezroczystość kafli, łatwo dostępna i działająca zmiana tapety, spójna tożsamość *Director Workstation*.
- [ ] **Zdefiniowane i wdrożone kontrakty API/UX dla wszystkich 6 obszarów P1:** lokalny LLM bez HTTP, Whisper STT, katalog i import głosów, własny głos, strojenie parametrów TTS, system własnych zachowań ikon.
- [ ] **Zero clippingu i zero overflow:** zweryfikowane w rozdzielczościach `780×560`, `1180×850` oraz `1920×1080`, zarówno z otwartym, jak i zamkniętym prawym inspektorem.
- [ ] **Działający wybór języka w 100% UI:** przełączenie PL ↔ EN aktualizuje wszystkie widoki (Studio, Doctor, First Run, VoiceLab, Ustawienia, Inspector) bez mieszania języków.
- [ ] **Testy funkcjonalne i screenshoty:** 100% zielonych testów `cargo test --workspace` oraz `QtTest` offscreen (z nowymi testami P0/P1) + zrzuty ekranu z realnej sesji Wayland/X11 potwierdzające stan wizualny.
- [ ] **Zatwierdzony kierunek wizualny:** jawna akceptacja użytkownika po przeglądzie realnej aplikacji po P0 i P1.

---

## F. Porównanie kandydatów do końcowego „Last Touch”: Fable vs Hydrogen vs Carbon

> **Zasada:** Poniższe zestawienie służy wyłącznie jako analiza porównawcza pod przyszłą fazę **P2 (last touch)**. **Nie wybieramy kandydata przed zamknięciem P0 i P1.**

| Kryterium | **Fable** | **Hydrogen** | **Carbon** |
|---|---|---|---|
| **Charakter estetyczny** | Narracyjny, ekspresyjny, nastawiony na atmosferę, kompozycję i subtelne detale wizualne. | Lekki, nowoczesny, czysty, kładący nacisk na przejrzystość, przestrzeń i minimalizm. | Techniczny, precyzyjny, surowy, skoncentrowany na gęstości informacji, siatce i inżynieryjnym rygorze. |
| **Mocne strony dla Audioticon (P2)** | Wyśmienity w budowaniu unikalnej tożsamości produktu (odejście od generyczności), wyczucie nastroju szkła, tapet i akcentów świetlnych. | Świetny w czyszczeniu szumu wizualnego, poprawie czytelności typografii, hierarchii i oddechu w gęstych panelach QML. | Bardzo mocny w estetyce sprzętu audio / DAW (racki, mierniki LUFS/Peak, precyzyjne suwaki TTS, tabele segmentów i diagnostyka). |
| **Ryzyko w fazie last touch** | Ryzyko nadmiernej „artystyczności” lub dekoracyjności kosztem surowej ergonomii narzędzia produkcyjnego. | Ryzyko zbytniego „wygładzenia” interfejsu w kierunku generycznej, sterylnej aplikacji SaaS/AI Studio. | Ryzyko zbytniej ciężkości lub chłodu interfejsu (zbyt techniczny, mniej przyjazny dla lektora/twórcy treści). |
| **Praca z materiałem Glass & kontrastem** | Wysoka wrażliwość na głębię i warstwy tła; wymaga pilnowania rygoru WCAG w ciemnych motywach. | Bardzo bezpieczny kontrast i lekka, przewidywalna warstwa szkła. | Precyzyjna separacja powierzchni i obrysów (`1px` frame strokes, matowe szkło techniczne). |
| **Potencjalna rola w P2** | Finalny szlif charakteru marki, klimatu tapet/szkła i spójności emocjonalnej stacji. | Uproszczenie layoutu, dopracowanie responsywności i przejrzystości formularzy/ustawień. | Dopracowanie instrumentarium audio (Studio, VoiceLab, suwaki TTS, mierniki, Doctor). |

**Status wyboru modelu do P2:** **ODROCZONY** — decyzja zostanie podjęta dopiero po spełnieniu wszystkich kryteriów z sekcji **E** (zamknięcie P0 i P1).
