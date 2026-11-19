# M9 — Audyt wizualny i kierunek stylu Premium dla Audioticon 2.0

**Data utworzenia:** 2026-11-18  
**Autor:** Antigravity (Architektura UI/UX & Systemy Wizualne)  
**Status:** Oficjalna specyfikacja kierunku artystycznego, ergonomii i stylu Premium  
**Plik podglądu bazowego:** `/tmp/audioticon-m8-preview.png`  
**Środowisko:** Qt 6 / QML / Natywny C++17 + Rust Tokio FFI  

---

## Diagnoza stanu obecnego (Audyt zrzutu ekranu M8)

Analiza zrzutu ekranu `/tmp/audioticon-m8-preview.png` wykazuje wyraźny rozdźwięk pomiędzy zaawansowanym, natywnym silnikiem audio (Rust + Piper ONNX + C++ E1 Directive Engine + QAudioSink), a powłoką wizualną odziedziczoną po szablonie asystenta LLM:

1. **Obcy „Governor zadań” (LLM Agent Monitor):** Dolne 25–30% okna zajmuje wielki panel telemetrii agenta LLM (`100/100`, `Runtime agenta · dry-run`, `TOKENY/S 20.8`, `NARZĘDZIA 1`, `BŁĘDY 0`, wykres latencji). W dedykowanej stacji roboczej do produkcji audiobooków jest to element mylący i marnujący cenną przestrzeń roboczą.
2. **Ściśnięty obszar roboczy (Vertical Cramping):** Pomiędzy nagłówkiem okna, edytorem reżyserii, listą zdań a wspomnianym monitorem LLM, kluczowa praca reżyserska i odsłuchowa odbywa się na zaledwie ~40% powierzchni ekranu.
3. **Konflikt wizualny w edytorze reżyserii:** Zrzut ekranu ujawnia nałożenie tekstu (`Rozdział 1: Początek...`) na obszar palety dyrektyw. Przyciski emoji zniknęły lub zostały przysłonięte przez pole tekstowe, a sam edytor wygląda jak surowy formularz notatnika.
4. **Brak tożsamości sprzętu audio (Audio Hardware Aesthetics):** Suwak odtwarzacza to cienka, systemowa linia `Slider`, przyciski to domyślne prostokąty z Qt Quick Controls, a etykiety `HIT` i `MISS` przypominają znaczniki w bazodanowym panelu CRUD zamiast precyzyjnych wskaźników pamięci cache próbek dźwiękowych.
5. **Nacisk na tekst zamiast na falę dźwiękową:** Brak reprezentacji fali (waveform), brak wskaźników wysterowania (peak/VU meters), brak estetyki znanej z nowoczesnych cyfrowych stacji roboczych (DAW) takich jak Ableton Live 12, Teenage Engineering, iZotope RX czy FabFilter.

**Cel transformacji M9:** Przekształcenie Audioticon 2.0 z „tekstowego panelu administracyjnego” w **prestiżową, mroczną, szklano-aluminiową stację reżyserii dźwięku lektorskiego (Modern Audio Workstation)**, w której każda kontrolka budzi zaufanie inżyniera dźwięku i lektora.

---

## 1. Nowa hierarchia wizualna całego okna

Interfejs zostaje przeorganizowany w 4 precyzyjne pasma funkcjonalne o rygorystycznym podziale pionowym:

```
┌────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ 1. HEADER BAR (40 px): Sygnet Audioticon 2.0 | Tytuł Projektu | SMPTE Master Clock | Silnik TTS | ☰ ⚙ │
├──────┬──────────────────────────────────────────────────────────────────────┬──────────────────────────┤
│ 2.   │ 3. OBSZAR CENTRALNY (STUDIO WORKSPACE)                               │ 4. CONTEXT INSPECTOR     │
│ NAV  │ ┌──────────────────────────────────────────────────────────────────┐ │ (DOKOWANY / DRAWER)     │
│ RAIL │ │ EDYTOR REŻYSERII I PALETA DYREKTYW (Directive Rack)              │ │ 340 px                   │
│      │ │ Pasek narzędzi emoji + Edytor z podświetlaniem składni           │ │                          │
│ 56px │ └──────────────────────────────────────────────────────────────────┘ │ [Inspektor] [Kolejka (2)]│
│      │ ┌──────────────────────────────────────────────────────────────────┐ │ [Głosy]     [ACX Meter]  │
│ [🎙️] │ │ TIMELINE SEGMENTÓW I PRÓBEK (Segment Timeline Matrix)            │ │                          │
│Studio│ │ Multi-track list ze zintegrowanym podglądem mini-waveformów      │ │ • Parametry segmentu     │
│ [👥] │ │ Kolumny: # | Zdanie | Tempo | Filtr DSP | Pauza | Cache | Odsłuch │ │ • Spektrum / Krzywa      │
│Voice │ └──────────────────────────────────────────────────────────────────┘ │ • Kontrola pamięci cache │
│ [📁] │ ┌──────────────────────────────────────────────────────────────────┐ │ • Konfiguracja głosu     │
│Vault │ │ MASTER TRANSPORT DOCK (Konsola Odsłuchu i Taśmy)                 │ │                          │
│ [💬] │ │ [⏮] [▶ ODTWÓRZ] [⏹] [⏭]  [==== Scrubber fali z markerami ===]    │ │                          │
│Agent │ └──────────────────────────────────────────────────────────────────┘ │                          │
├──────┴──────────────────────────────────────────────────────────────────────┴──────────────────────────┤
│ 5. MASTER AUDIO & SYSTEM TELEMETRY STRIP (28 px - Zastępuje stary Governor):                           │
│ [● Piper ONNX: pl_bass 114MB]  [Cache: 142MB / 89% HIT]  [EBU R128: -19.4 LUFS]  [True Peak: -1.2 dBFS]│
└────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

### Reguły hierarchii:
- **Zero pożeraczy miejsca:** Stary 136-pikselowy `StateStrip` z metrykami LLM zostaje usunięty z widoku Studia. Telemetria audio zostaje skondensowana do eleganckiego, 28-pikselowego paska statusu na samym dole.
- **Oddech dla danych:** Zyskane ~110 px wysokości zostaje oddane w całości na listę segmentów i transport.
- **Optyczna równowaga:** Główne okno skupia wzrok na osi akcji: **Napisz → Sprawdź segmenty → Posłuchaj na transport barze**.

---

## 2. Branding Audioticon 2.0 (Sonic & Precision Identity)

Zamiast generycznego sci-fi trójkąta i technicznego napisu „ASTRA ENGINE / LOCAL RUNTIME”, wprowadzamy tożsamość dedykowaną profesjonalnemu audio:

- **Sygnet (The Waveform Monogram):**
  - Połączenie litery **A** ze stylizowanym rezonatorem akustycznym i falą sinusoidalną.
  - Subtelny gradient od neonowego szmaragdu (`#22e58b`) do błękitu studyjnego (`#00d2ff`).
  - Efekt wklęsłego frezu w ciemnym metalu z delikatną poświatą (inner glow).
- **Logotyp tekstowy:**
  - `AUDIOTICON` — czysty, geometryczny krój sans-serif (wersaliki, tracking `+3.0px`, waga `Font.DemiBold`).
  - Wskaźnik wersji: `2.0` w indeksie górnym w kolorze akcentu z miniaturowym znacznikiem `PRO STUDIO`.
  - Sub-label: `DIRECTOR WORKSTATION // NATIVE RUNTIME` — precyzyjny, techniczny podpis w czcionce monospace.

---

## 3. Paleta kolorów, kontrast i materiały Glass (Studio Dark Palette)

Koncepcja odchodzi od surowego, płaskiego błękitu terminala na rzecz wielowarstwowego, anodyzowanego aluminium i przyciemnianego szkła studyjnego.

### 3.1 Kolory tła i powierzchni (Surface Architecture)

| Poziom | Hex / RGBA | Zastosowanie |
|---|---|---|
| **Deep Void** (Podstawa okna) | `#080c11` | Główne tło aplikacji pod panelami szkła. |
| **Rack Surface** (Kontenery główne) | `rgba(13, 20, 27, 0.88)` | Panele edytora, timeline'u i nawigacji. |
| **Glass Elevated** (Karty i popover) | `rgba(20, 31, 42, 0.94)` | Pływający inspektor, karty dialogowe, menu. |
| **Frame Stroke** (Obrys bazowy) | `#1a2632` | 1-pikselowe obramowanie wszystkich modułów. |
| **Frame Active** (Obrys fokusowany) | `#2a4052` / `rgba(34, 229, 139, 0.4)` | Aktywny edytor, zaznaczony segment. |

### 3.2 Paleta semantyczna i wskaźniki audio

- **Signal Emerald (`#22e58b`):**
  - Podstawowy akcent marki.
  - Przycisk odtwarzania (`Play`), stan `Cache HIT`, aktywny lektor, wskaźnik prawidłowego poziomu LUFS.
- **Sonic Cyan (`#00d2ff`):**
  - Wskaźnik postępu renderowania, procesory DSP (`Radio`, `Katedra`), suwak scrubbera, zaznaczenie segmentu na osi czasu.
- **Vintage Amber (`#ffaa33`):**
  - Stan `Cache MISS`, pauzy reżyserskie (`Pauza`, `Stop`), zbliżanie się do limitu przesterowania (-3 dBFS).
- **Crimson Peak (`#ff3b56`):**
  - Przesterowanie sygnału (True Peak > 0 dBFS), błędy kompilacji dyrektyw, błąd silnika Piper, przycisk natychmiastowego zatrzymania (`Stop / Abort`).
- **Director Lilac (`#b388ff`):**
  - Znaczniki rozdziałów (`📖 Rozdział`), metadane narratora, przełączanie ról postaci.

---

## 4. Typografia i skala tekstu (Studio Typography System)

Czytelność w trudnych warunkach oświetleniowych (ciemna reżyserka, wielogodzinna praca lektora):

| Rola | Krój | Rozmiar / Waga | Tracking | Zastosowanie |
|---|---|---|---|---|
| **Display Clock** | Tabular Monospace | 20 px / Medium | +1.0 px | Główny licznik transportu (`00:02:14.380`). |
| **Section Header** | Sans-Serif | 11 px / Bold | +1.8 px | Nagłówki paneli wersalikami (`TIMELINE SEGMENTÓW`). |
| **Prose / Editor** | Modern Sans | 13.5 px / Regular | 0.0 px | Edytor tekstu, podgląd zdań w tabeli (wysoka czytelność). |
| **Parameter Label** | Monospace | 10 px / Regular | +0.8 px | Parametry techniczne (`44.1 kHz`, `1.15x`, `900 ms`). |
| **Status Badge** | Monospace | 8.5 px / Bold | +1.2 px | Pigułki `HIT`, `MISS`, `IDLE`, `SYNTH`. |
| **Micro Caption** | Sans-Serif | 9 px / Regular | +0.5 px | Pasek stanu, objaśnienia skrótów klawiaturowych. |

---

## 5. Wygląd NavRail, StudioView i ContextInspector

### 5.1 NavRail (Pionowy Rack Nawigacji)
- Szerokość: **56 px**.
- Każda ikona umieszczona w precyzyjnie wytłaczanym kwadracie 40×40 px z zaokrągleniem 10 px.
- **Stan aktywny:** Neonowy lewy pasek (3 px szerokości, wysokość 22 px, kolor `#22e58b`) z miękkim radialnym blaskiem na ikonie.
- **Wskaźnik pracy silnika:** Miniaturowa dioda LED (1.5 px) przy ikonie Studia pulsująca delikatnie w trakcie renderowania w tle.

### 5.2 StudioView (Centralny Stół Realizatorski)
- Podział centralnego obszaru na dwie główne sekcje z zachowaniem proporcji **38% (góra) do 62% (dół)**:
  - **Góra:** Edytor reżyserski z paletą emoji.
  - **Dół:** Tabela zdań i sekwencji audio.
- Wyraźne krawędzie separujące panele z subtelnym fazowaniem (bevel) symulującym sprzęt studyjny.

### 5.3 ContextInspector (Wielofunkcyjny Dok Boczny)
- Szerokość: **330 px**.
- Elegancki segmentowany pasek zakładek (Segmented Tab Bar) z ikonami:
  - `[🔎 Segment]` `[⏳ Kolejka (1)]` `[🎙️ Modele]` `[📁 Materiały]` `[📊 ACX]`
- Płynne przełączanie zakładek z animacją przesunięcia zawartości (slide fade 140 ms).
- Wskaźnik przypięcia pinezki (`Pin`) z czytelnym stanem wciśnięcia / odpięcia.

---

## 6. Stany renderowania, cache HIT/MISS, błędy i pusty ekran

### 6.1 Pigułki Cache HIT / MISS (Precision Hardware Badges)
Zamiast obecnych płaskich prostokątów z tekstem:
- **`HIT` (Cache Trafiony):**
  - Tło: Głęboka zieleń butelkowa `rgba(34, 229, 139, 0.12)`.
  - Obrys: 1 px `#1b6e4b`.
  - Tekst: `#22e58b` w czcionce Monospace z miniaturową kropką LED `●` po lewej.
  - Tooltip: Wyświetla rozmiar pliku WAV i czas syntezy z cache (np. `48 KB · 0 ms (Pamięć RAM)`).
- **`MISS` (Wymaga Syntezy):**
  - Tło: Neutralny matowy grafit `rgba(255, 170, 51, 0.08)`.
  - Obrys: 1 px `rgba(255, 170, 51, 0.3)`.
  - Tekst: `#ffaa33` z symbolem fali `~` oznaczającym oczekiwanie na przetworzenie przez Pipera.

### 6.2 Aktywny stan renderowania (Synthesis Shimmer)
- Podczas syntezy segmentu jego wiersz na osi czasu otrzymuje delikatny, przesuwający się gradient liniowy (**shimmer effect** w kolorze błękitu `#00d2ff`), informujący o pracy wątku Tokio w tle.
- Na pasku Transportu procent renderowania nie zastępuje licznika czasu, lecz pojawia się jako precyzyjny niebieski pasek postępu nad suwakiem odtwarzania.

### 6.3 Komunikacja błędów (Resilient Error Handling)
- Gdy Piper lub urządzenie audio zgłosi błąd, interfejs nie wyświetla surowego czerwonego napisu u dołu.
- Pojawia się **pływający banner diagnostyczny (Error Bar)** pod edytorem z ciemnoczerwonym tłem (`#240e13`), opisem błędu ludzkim językiem („Nie znaleziono pliku modelu ONNX w katalogu”) oraz przyciskiem natychmiastowej akcji `[Wybierz model]` lub `[Spróbuj ponownie]`.

### 6.4 Pusty ekran (Empty State)
- Gdy edytor jest pusty, zamiast zwykłego tekstu na szarym tle pojawia się estetyczna kompozycja graficzna:
  - Zarys mikrofonu studyjnego w wektorowej linii (stroke `#223542`).
  - Tytuł: `GOTOWY DO REŻYSERII AUDIOBOOKA`.
  - Trzy kafelki szybkiego startu:
    - `[📖 Wklej rozdział z dyrektywami demo]`
    - `[📁 Zaimportuj plik EPUB / Markdown]`
    - `[🎙️ Wybierz głos lektora (Piper Bass)]`

---

## 7. Mikroanimacje i przejścia (Subtle Studio Motion)

Zasada: **Żadnych powolnych, rozpraszających animacji**. Wszystkie przejścia są natychmiastowe, sprężyste i funkcjonalne (120–180 ms):

1. **Wybór segmentu:** Po kliknięciu wiersza segmentu krawędź płynnie rozświetla się szmaragdem w czasie 120 ms, a prawy panel bez opóźnienia przełącza się na zakładkę Inspektora.
2. **Odtwarzanie:** Ikona `▶` w ułamku sekundy morphuje w `⏸` (Micro-morphing 90 ms).
3. **Scrubber Scrubbing:** Wskaźnik pozycji porusza się z częstotliwością 60 FPS z idealnie gładką interpolacją, nie powodując skoków dźwięku ani drżenia etykiety czasu.
4. **Wysuwanie Drawera:** Prawy panel w trybie pływającym wysuwa się z użyciem krzywej `Easing.OutCubic` (160 ms), a tło pod nim przyciemnia się miękkim cieniem akrylowym.

---

## 8. Wygląd edytora reżyserii emoji (Directive Composer)

Rozwiązanie problemu nałożenia tekstu na paletę emoji z podglądu M8:

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│ PALETA DYREKTYW:                                                          Znaków: 1420 │
│ [NARRACJA: 📖 Rozdział  🛑 Stop 900ms  ⏸️ Pauza 400ms  😮‍💨 Oddech]                         │
│ [TEMPO:    🐢 Zwolnij   🐇 Przyspiesz]                                                 │
│ [FILTRY:   📻 Radio     📱 Telefon     🏛️ Katedra      🤫 Szept]                        │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ 1 │ 📖 Rozdział 1: Początek.                                                           │
│ 2 │ 🛑 Ciemność ogarnęła całą dolinę, gdy nagle lektor rzekł:                         │
│ 3 │ 🐢 'Czas wyruszyć w drogę.' ⏸️ 📻                                                  │
│ 4 │                                                                                    │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ [🎙️ Zreżyseruj (C++ E1)]   [▶ Syntetyzuj cały rozdział (Piper)]     Szacowany czas: ~3:45 │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

### Elementy kluczowe:
- **Fizyczna separacja palety:** Pasek emoji staje się górnym paskiem narzędziowym (Toolbar) o stałej wysokości 44 px, z podziałem na logiczne grupy (Narracja, Tempo, Filtry DSP).
- **Kafelki dyrektyw:** Każdy przycisk emoji posiada subtelny podpis parametru w czcionce 8 px (np. pod `🛑` widnieje szare `900ms`, pod `🐢` widnieje `0.85x`).
- **Numeracja linii:** W edytorze pojawia się dyskretna kolumna z numerami linii tekstu (w kolorze `#364f5c`), co ułatwia pracę przy korekcie długich rozdziałów.
- **Directive Syntax Chips (Podświetlanie znaczników):** Wprowadzone emoji w edytorze zyskują delikatne półprzezroczyste tło (chip styling), dzięki czemu nie zlewają się ze słowami lektora.

---

## 9. Timeline segmentów i transport audio (Waveform & Transport Bar)

### 9.1 Lista segmentów (The Voice Timeline Matrix)
- Wiersze zyskują delikatne naprzemienne tła (`zebra striping`: `#0c1218` oraz `#0f171f`), co zapobiega gubieniu wzroku przy dziesiątkach zdań.
- **Zintegrowany mini-waveform:** W kolumnie odsłuchu, obok przycisku `▶`, pojawia się 48-pikselowy podgląd fali dźwiękowej (wykres szczytów z cache WAV), dający inżynierowi dźwięku natychmiastową informację o dynamice i długości ciszy.
- **Aktywny segment:** Gdy odtwarzacz gra dany segment, cały wiersz otrzymuje delikatną neonową poświatę z lewej strony oraz wskaźnik postępu grania w czasie rzeczywistym.

### 9.2 Master Transport Bar (Dolna Konsola Odsłuchu)
- Wysokość zwiększona do **52 px**, co nadaje mu rangę centralnego panelu kontrolnego.
- **Przyciski kontrolne:**
  - `[⏮]` Przewiń do początku / poprzedni segment.
  - `[ ▶ ODTWÓRZ ]` Duży, centralny przycisk o wysokości 34 px, z neonowym obrysem i wyraźnym stanem aktywności.
  - `[ ⏹ ]` Przycisk zatrzymania i resetu pozycji do zera.
  - `[⏭]` Następny segment.
- **Interactive Scrubber (Wizualny pasek taśmy):**
  - Zamiast zwykłej linii suwaka — pasek o wysokości 14 px z podziałką milisekundową oraz pionowymi znacznikami granic zdań.
  - Segmenty wyrenderowane w cache oznaczone są na pasku kolorem szmaragdowym, a segmenty jeszcze nieprzeliczone — dyskretną szarością.
- **Wyświetlacz czasu:** Duży, kontrastowy zegar w kroju monospace: `00:01:24.180 / 00:03:45.000`.

---

## 10. Ekran VoiceLab, biblioteki eksportów i modeli

Aby Audioticon 2.0 był kompletnym środowiskiem produkcyjnym, pozostałe moduły z lewej nawigacji zyskują spójny wygląd:

### 10.1 VoiceLab (Katalog Głosów i Narzędziownia Lektorów)
- Prezentacja dostępnych modeli Piper ONNX w postaci **kart lektorskich (Voice Cards)**:
  - Nazwa głosu: np. `PL Bass High (Męski - Narrator)`.
  - Parametry techniczne: `ONNX Runtime · 114 MB · 22.05 kHz · Apache-2.0`.
  - Pasmo podstawowe: `85–165 Hz` z małym wykresem spektralnym barwy.
  - Przycisk szybkiego odsłuchu próbki: `[▶ Odsłuchaj próbkę]`.
  - Przycisk aktywacji: `[Ustaw jako główny głos projektu]`.

### 10.2 Biblioteka Eksportów (Master Export Vault)
- Tabela wygenerowanych plików audio z kompletnymi metadanymi masteringowymi:
  - Format docelowy: `M4B (Audiobook z rozdziałami)`, `MP3 320 kbps`, `WAV 44.1 kHz 24-bit`.
  - Zgodność z normą ACX / Audible: zielony znacznik `ACX COMPLIANT (Integrated: -19.4 LUFS, Peak: -1.2 dBFS, Noise: -64 dBFS)`.
  - Akcje jednym kliknięciem: `[Otwórz folder]`, `[Kopiuj do schowka]`, `[Odtwórz master]`.

---

## 11. Responsywność od 780×560 do 1920×1080 (Responsive Breakpoints)

System gwarantuje wzorowe zachowanie na każdym ekranie — od laptopa z małą matrycą po panoramiczne monitory studyjne:

### 11.1 Tryb Compact (780×560 – 920×700 px)
- NavRail zmniejsza szerokość do **48 px** (same ikony bez etykiet).
- ContextInspector automatycznie przechodzi w tryb **Floating Drawer (nakładana szuflada)** z zaciemnieniem tła (`scrim`), nie zabierając miejsca w poziomie.
- W edytorze reżyserii paleta emoji zwija się do menu rozwijanego `[Dyrektywy ▾]`.
- Transport Bar ukrywa etykiety tekstowe przycisków, eksponując same symbole `▶`, `⏸`, `⏹`.

### 11.2 Tryb Standard (1180×850 px — Rozdzielczość Bazowa)
- Układ 3-kolumnowy: NavRail (56 px) + Centralne Studio (elastyczne ~780 px) + Zadokowany Inspektor (330 px).
- Pełna paleta dyrektyw w edytorze, czytelna tabela zdań, pełny pasek transportowy.

### 11.3 Tryb Ultra-Wide / 4K (1920×1080 px i więcej)
- Centralne Studio automatycznie przełącza się w **układ dwukolumnowy (Side-by-Side)**:
  - **Po lewej:** Edytor tekstu ze znacznikami na pełną wysokość.
  - **Po prawej:** Timeline segmentów z rozbudowanym podglądem fal audio i spektrogramem.
- Inspektor boczny rozszerza się do 380 px, wyświetlając pełnowymiarowy analizator głośności EBU R128 w czasie rzeczywistym.

---

## 12. UX klawiatury i dostępność (Pro Keyboard Workflow)

Prawdziwy inżynier dźwięku i lektor rzadko sięgają po mysz. Interfejs zyskuje ergonomię klasy workstation:

- **Spacja (`Space`):**
  - Natychmiastowe odtworzenie / pauza (`Play / Pause`) bieżącego pliku lub zaznaczonego segmentu.
  - Jeśli trwa renderowanie — pojedyncze naciśnięcie natychmiast je anuluje.
- **Klawisze J-K-L (Standard montażu audio/wideo):**
  - `J`: Przewiń wstecz o 5 sekund / skocz do poprzedniego zdania.
  - `K`: Pauza / Stop.
  - `L`: Odtwarzaj w przód / następne zdanie.
- **Strzałki Góra / Dół (`↑` / `↓`):**
  - Płynna nawigacja po liście zdań w timeline z automatycznym podglądem w Inspektorze.
- **`Enter` na zaznaczonym segmencie:**
  - Natychmiastowa synteza i odsłuch wyłącznie tego zdania (`studio.playSegment(index)`).
- **`Ctrl + 1..6`:**
  - Błyskawiczny skok między modułami: `Ctrl+1` Studio, `Ctrl+2` Czat AI, `Ctrl+3` VoiceLab, `Ctrl+4` Projekty, `Ctrl+5` SFX, `Ctrl+6` Pomiary.
- **`Ctrl + I`:**
  - Otwarcie / zamknięcie prawego panelu Inspektora.
- **Wskaźnik Fokusu Klawiatury (Focus Ring):**
  - Aktywny element otrzymuje wyrazisty, 1.5-pikselowy obrys w kolorze błękitu studyjnego (`#00d2ff`) z subtelnym rozmyciem poświaty, gwarantując pełną czytelność bez użycia myszy.

---

## 13. Zestawienie TOP 7 zmian o największym efekcie („The Wow Factor”)

Wdrożenie poniższych 7 modyfikacji przyniesie natychmiastowy skok jakościowy z poziomu „amatorskiego narzędzia” do **„profesjonalnej stacji lektorskiej 2026 roku”**:

1. **Usunięcie panelu Governor LLM i zastąpienie go `Mastering Audio Strip`:**
   - Likwidacja mylącego wykresu tokenów LLM. Wprowadzenie na dole smukłego paska z miernikami EBU R128 LUFS, True Peak oraz stanem bufora pamięci cache.
2. **Nowy Scrubber taśmy w Transport Barze:**
   - Zastąpienie zwykłego suwaka interaktywną osią czasu audio z podziałką milisekundową, znacznikami zdań i wskaźnikami obecności w cache.
3. **Płytki Hardware LED dla Cache HIT / MISS:**
   - Precyzyjne, fotorealistyczne wskaźniki z podświetleniem LED, natychmiast budzące skojarzenia ze sprzętowym rackiem DSP.
4. **Naprawa i separacja edytora reżyserii emoji:**
   - Wyeliminowanie błędu nakładania tekstu z preview M8. Wydzielenie eleganckiej, pogrupowanej palety dyrektyw z podglądem parametrów (`900ms`, `0.85x`, `DSP`).
5. **Autoscroll i podświetlenie granego zdania (Live Sentence Tracking):**
   - Podczas odtwarzania lista zdań automatycznie przewija się do aktualnie mówionego zdania, a jego wiersz świeci miękkim szmaragdowym światłem.
6. **Mini-waveform przy każdym segmencie:**
   - Miniaturowy wykres dynamiki głosu w tabeli, pozwalający lektorowi ocenić długość i głośność zdania przed jego odsłuchaniem.
7. **Nowoczesny sygnet i typografia studyjna:**
   - Zastąpienie generycznych napisów profesjonalnym brandingiem `AUDIOTICON 2.0 // NATIVE AUDIO ENGINE` oraz zegarem SMPTE z dokładnością do milisekund.

---

## Podsumowanie i status wdrożeniowy

Niniejsza specyfikacja definiuje kompletny język wizualny dla Audioticon 2.0. Zgodnie z wytycznymi:
- **Nie modyfikowano kodu źródłowego ani plików QML.**
- Dokument stanowi ścisłą bazę wytycznych dla kolejnego etapu implementacji powłoki wizualnej (M10).
- Całość zachowuje pełną kompatybilność z istniejącymi kontraktami `StudioBridge`, `AudioPlayer`, `AgentModel` oraz zestawem 24 testów automatycznych.
