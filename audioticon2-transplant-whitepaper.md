# White paper: przeszczep z Portable Local Studio do Audioticon 2

**Wersja:** 1.0 · **Data:** 2026-11-18
**Dawca:** Portable Local Studio (`~/Portable-Local-Studio`, lokalny serwer Node + React)
**Gospodarz:** Audioticon 2 (projekt własny, pełne źródła)
**Status:** dokument decyzyjny + wstępny plan; brakuje audytu kodu gospodarza (patrz §7.0)

---

## 1. Streszczenie decyzji

Local Studio nie będzie dalej rozbudowywane jako produkt. Zamrażamy je w roli **dawcy** i przenosimy do Audioticona 2 trzy klasy dóbr:

1. **Lektor PL** (Piper + dyrektywy emoji + verbalizer + cache segmentów + DSP) — dojrzałe, sprawdzone, najmniejsze ryzyko.
2. **Czat** (pipeline po stronie serwera + nowy, natywny dla Audioticona interfejs, w tym drugi niezależny wątek).
3. **Host LLM** (backendy llama.cpp per platforma/akceleracja, katalog GGUF, download, start/stop/health) — najcięższy i najbardziej „systemowy" element.

Zasada nadrzędna: **przenosimy kontrakty i logikę, nie widoki.** UI powstaje od zera u gospodarza, w jego stylu.

---

## 2. Problem

`scripts/server/serve.cjs` ma **8 435 linii** i jest monolitem: routing to łańcuch `if (req.url === ...)` w jednej funkcji obsługi HTTP, w tym samym pliku siedzą trasy LLM, TTS, STT, generowanie obrazów, telemetria i diagnostyka.

Konsekwencje:

- każda zmiana w lektorze dotyka pliku, w którym równolegle żyje pięć innych podsystemów;
- brak granic modułów = brak możliwości przetestowania czegokolwiek w izolacji;
- nakład pracy na „dopieszczenie" Local Studio rośnie szybciej niż wartość, bo 90% monolitu to rzeczy, których w docelowym produkcie nie potrzebujemy (generator obrazów, OpenVINO, telemetria, biblioteka nagrań).

Wniosek: dalsza rozbudowa w miejscu jest droższa niż jednorazowy, kontrolowany przeszczep.

---

## 3. Cele i nie-cele

**Cele**

- Audioticon 2 ma czytać tekst głosem PL offline, z reżyserią (pauzy, tempo, rozdziały, plany akustyczne).
- Audioticon 2 ma mieć czat lokalny (dwa niezależne wątki) korzystający z hosta LLM.
- Host LLM w Audioticonie 2 uruchamia `llama-server` dla wybranej platformy/akceleracji i zarządza modelami GGUF.
- Każdy przeniesiony moduł ma własne API, test dymny i zero wiedzy o Local Studio.

**Nie-cele**

- Nie przenosimy generatora obrazów, OpenVINO, telemetrii, SpeechTranscribera ani biblioteki outputs Local Studio.
- Nie utrzymujemy dwóch równoległych rozwojów tego samego kodu — po przeszczepie źródłem prawdy jest Audioticon 2, Local Studio dostaje tylko poprawki bezpieczeństwa.
- Nie przenosimy UI (patrz §5) i nie „przepisujemy przy okazji" architektury Audioticona 2.

---

## 4. Inwentaryzacja dawcy

### 4.1 Klasyfikacja

| Element | Rozmiar | Sprzężenie | Werdykt |
|---|---|---|---|
| `scripts/server/tts-directives.cjs` — parser dyrektyw, verbalizer PL, WAV join, DSP, klucz cache | 641 linii, 0 zależności | brak (czysty Node: `fs`, `path`, `crypto`, `child_process`) | **ZIELONY** — kopia 1:1 |
| `scripts/server/model-kind.cjs` — typ modelu po pliku | 140 linii | brak | **ZIELONY** — kopia 1:1 |
| `scripts/setup/setup-piper.sh`, `scripts/workers/tts-voiceconv.py` | 2 pliki | ścieżki względem `ROOT` | **ZIELONY** po parametrze `--root` |
| Głosy PL: `pl_bass` (114 MB), `pl_mc_speech` (63 MB), `pl_mls_6892` (63 MB) + 2 głosy klonowane | ~240 MB modeli | manifesty w katalogu głosów | **ZIELONY** (dane, nie kod) |
| Blok Piper/TTS w `serve.cjs`: `getPiperCatalogVoice` (2379), `installPiperVoice` (2386), `getTtsModels` (2425), `getPiperStatus` (2534), `startTts` (2629), `synthesizeTts` (2826), `runPiperToWav` (2980), `synthesizePiperTts` (3057), `renderDirectedSegment` (3194), `synthesizeDirectedTts` (3238), `saveSynthesizedWav` (3039) | ~1 200 linii | `ROOT`, katalog `app/tts-runtime`, manifesty, ffmpeg | **ŻÓŁTY** — wyciąć do `piper.cjs` z jawnym API |
| Głos klonowany: `synthesizeVoiceCloneTts` (3105), `runVoiceConversion` (3016), `getVoiceCloneStatus` (2574) | ~250 linii | Python venv OpenVoice (część z 7,1 GB `app/tts-runtime`) | **ŻÓŁTY** — opcjonalny, ciężki |
| Host LLM: `startLlm` (5048), `startLlmWithBackend` (5140), `pingLlmReady` (3535), `getLlmBackendCandidates` (2116), katalog modeli (~1610–1680), backendy per OS/accel (`app/llm-backend`, **1,3 GB**) | ~1 400 linii | platforma, DLL-e, porty, proces potomny, download | **ŻÓŁTY/TWARDY** — największe ryzyko |
| Czat po stronie serwera: `/api/llm/chat` (7791), konwersacje, streaming, statystyki | ~400 linii | `llmProc`, historia na dysku | **ŻÓŁTY** — wyciąć razem z hostem LLM |
| `TextChat.jsx` (1 485), `TextToSpeech.jsx` (1 431), `ModelManager.jsx` (2 345), `Generator.jsx` (1 955) | ~7 200 linii UI | style, stan, kontekst Local Studio | **CZERWONY** — nie przenosić |

### 4.2 Co to znaczy „gotowy przeszczep" (definicja akceptacji modułu)

Moduł jest gotowy do przeniesienia, gdy:

1. działa bez `serve.cjs` — importowany przez `require()`, nie przez kopiuj-wklej;
2. nie zna `ROOT` — wszystkie ścieżki przychodzą w opcjach/konfiguracji;
3. nie zakłada portów ani procesów globalnych — dostaje fabrykę/instancję;
4. ma **kontrakt API** (wejście/wyjście JSON) opisany w dokumencie modułu;
5. ma test dymny uruchamialny jednym poleceniem, bez UI;
6. nie loguje sekretów i nie zapisuje niczego poza wskazanym katalogiem roboczym.

Dziś warunek 1–3 spełnia tylko `tts-directives.cjs` i `model-kind.cjs`. Reszta wymaga wycięcia — i to jest właściwa praca etapu M1/M3.

---

## 5. Architektura docelowa w Audioticonie 2

Trzy moduły, trzy granice, zero wspólnego stanu globalnego:

```
Audioticon 2
├── core/llm-host      → llama-server: wykrywanie backendu, start/stop, health, katalog GGUF, download
├── core/chat          → historia, streaming, kontekst, dwa niezależne wątki
└── core/voice         → Piper: głosy, dyrektywy, verbalizer, cache, DSP, render WAV
        ↑
        └── tts-directives.cjs (czysty parser, bez I/O poza plikami tymczasowymi)
```

Zależności idą tylko w jedną stronę: `chat → llm-host`, `voice` jest samodzielny. UI woła wyłącznie HTTP modułów, nigdy ich wnętrza.

### 5.1 Kontrakt `voice` (proponowany)

```
GET  /voice/voices          → [{ id, name, language, engine, sizeBytes, license, origin, installed }]
POST /voice/install         { id }                      → { ok, id, sizeBytes }
GET  /voice/status          → { ready, engine, voices, cacheSegments }
POST /voice/speak           { text, voice, speed, dsp? } → { audioFile, durationMs, segments,
                                                            cachedSegments, directiveCount, chapters }
GET  /voice/outputs         → [{ id, file, text, voice, durationMs, createdAt }]
DELETE /voice/outputs/:id
```

`text` przyjmuje gramatykę emoji (§8.2). Głosy nieznane zwracają `400` z `{ error, code }`; tekst bez liter/cyfr zwraca `422 EMPTY_SPEECH` (to naprawiony crash Pipera — nie może wracać jako `500`).

### 5.2 Kontrakt `llm-host`

```
GET  /llm/backends          → { available: [cpu, vulkan, rocm, cuda, sycl], chosen }
GET  /llm/models            → [{ file, kind, sizeBytes, quantization, contextHint }]
POST /llm/download          { file, url }               → 202 + progres
POST /llm/start             { file, backend?, ctx?, gpuLayers? } → { ok, pid, port, backend }
POST /llm/stop
GET  /llm/status            → { running, model, backend, port, tokensPerSecond }
```

### 5.3 Kontrakt `chat`

```
POST /chat/threads                     → { id }
GET  /chat/threads                     → [{ id, title, model, updatedAt }]
POST /chat/threads/:id/messages        { content, stream: true } → SSE tokenów + statystyki
DELETE /chat/threads/:id
```

Wątek ma własny `model` — to jest podstawa „dodatkowego czatu": dwa wątki mogą rozmawiać z dwoma różnymi modelami jednocześnie, przy jednym uruchomionym `llama-server` (lub dwóch, jeśli VRAM pozwala — decyzja w M3).

---

## 6. Ryzyka i pułapki

| Ryzyko | Waga | Mitygacja |
|---|---|---|
| Backendy llama.cpp per platforma to **1,3 GB** (linux/rocm 1,2 GB, vulkan 86 MB, cpu 44 MB) i DLL-e na Windowsie | wysoka | pobierać backend przy pierwszym starcie, nie w paczce; jednoznaczny komunikat przy `STATUS_DLL_NOT_FOUND` (0xC0000135) |
| Modele GGUF leżą w `app/llm-models` (**4,4 GB** lokalnie) | wysoka | nigdy nie commitować; katalog modeli gospodarza + download z progresem |
| `app/tts-runtime` waży **7,1 GB** (venv z OpenVoice/torch) | wysoka | Piper to ~30 MB binarki + modele; klonowanie głosu (OpenVoice) traktować jako instalację opcjonalną, nie domyślną |
| Cache segmentów rośnie bez limitu (`app/tts-cache` 3,8 MB dziś) | średnia | TTL + limit rozmiaru; klucz SHA-256 z (voice, speed, dsp, text, sampleRate) już to umożliwia |
| Zmiana DSP/verbalizera unieważnia cache | średnia | wersjonować algorytm w kluczu (`v1`), nie czyścić katalogu ręcznie |
| Licencje głosów | średnia | `docs/polish-tts-notes.md` ma pochodzenie i licencje; UI musi pokazywać `license` i `origin` przy głosie |
| Głosy klonowane (`pl_george`, `pl_george_soft`) zależą od OpenVoice i referencji zaszytej w kodzie | niska | zostawić w dawcy; w Audioticonie dopiero po uploadzie referencji (poza zakresem tego planu) |
| Dwa różne `spawn` (Piper, llama-server) w jednym procesie Electrona/Node | średnia | jeden wrapper procesów potomnych, zabijanie po PID drzewa, nigdy `pkill -f` |
| Rozjazd dwóch kopii po przeszczepie | wysoka | po M1 źródłem prawdy jest Audioticon 2; Local Studio zamrożone |

---

## 7. Wstępny plan działania

Szacunki są rzędowe (jedna „sesja" = jedna dłuższa jednostka pracy), nie wiążące.

### M0 — audyt gospodarza (blokada startu, 1 sesja)

- Odnaleźć repo Audioticona 2, ustalić runtime (Electron main / serwer Node / inne), sposób rejestracji tras i styl UI.
- Ustalić, czy istnieje już jakikolwiek TTS/LLM i czy da się je zastąpić, czy tylko obok.
- **Wyjaśnić znaczenie „dodatkowego czatu"**: drugi niezależny wątek (inny model) czy czat z odczytem odpowiedzi lektorem. To zmienia zakres M2 i M4.
- Wyjście: mapa punktów wpięcia (plik + linia) + decyzja o kształcie modułów.

### M1 — lektor (2–3 sesje)

1. Wyciąć blok Pipera z `serve.cjs` do `piper.cjs` z kontraktem §5.1 (bez UI, bez `ROOT`).
2. Skopiować `tts-directives.cjs` i `model-kind.cjs` bez zmian.
3. Podłączyć `POST /voice/speak` w Audioticonie 2.
4. Test dymny: 6 przypadków — czysty tekst, dyrektywy, tekst bez słów (`...`), emoji poza gramatyką, data/liczby słownie, powtórka z cache.
5. **Kryterium akceptacji:** te same czasy i pliki co w Local Studio (pierwsze 5 segmentów ~5,6 s, powtórka < 50 ms, `422` dla tekstu bez słów).

### M2 — czat (2 sesje)

1. Wyciąć `/api/llm/chat` + historię do `chat` z kontraktem §5.3.
2. Nowy komponent czatu w Audioticonie 2 (bez przenoszenia `TextChat.jsx`).
3. Rozstrzygnąć „dodatkowy czat" według M0 i zaimplementować wariant.
4. **Kryterium akceptacji:** streaming działa, historia przeżywa restart, dwa wątki nie mieszają kontekstu.

### M3 — host LLM (3–4 sesje, najtrudniejszy)

1. Wyciąć `startLlm`/`startLlmWithBackend`/`pingLlmReady` + katalog do `llm-host`.
2. Wykrywanie backendu i czytelne błędy (brak DLL, brak sterownika, za mało VRAM).
3. Download modeli z progresem i anulowaniem.
4. **Kryterium akceptacji:** start/stop 3 modeli (Q4_K_M 7B, Q4 0,5B, Q8 8B) na dostępnym backendzie, `/llm/status` zgodny z rzeczywistym procesem, brak zombie procesów po stopie.

### M4 — nowe UI (2–3 sesje)

Patrz §8. Kolejność: powłoka → studio głosu → czat → most czat↔studio.

### M5 — utwardzenie (1 sesja)

Limity cache, TTL, logi bez ścieżek użytkownika, dokumentacja modułów, zamrożenie Local Studio (tag/commit „donor-frozen").

---

## 8. Nowe UI w Audioticonie 2

Sekcja dodatkowa: interfejs powstaje **w Audioticonie 2 od zera**, wzorowany na tym, co się sprawdziło w Local Studio, ale bez przenoszenia komponentów.

### 8.1 Zasady

- Jedna powłoka, trzy widoki: **Studio głosu**, **Czat**, **Modele**.
- Stały pasek statusu u góry: aktywny backend (CPU/Vulkan/ROCm), model, VRAM/RAM, stan silnika TTS, czas ostatniej syntezy.
- Zero modali na ścieżce krytycznej — ustawienia w panelu bocznym, nie w okienkach.
- Wszystko offline-first: brak stanów „połącz z internetem"; download modeli to jawny, anulowalny proces.

### 8.2 Studio głosu

**Composer (lewa kolumna, 60%):**

- Pole tekstu z licznikiem znaków i szacowanym czasem czytania.
- **Paleta dyrektyw** — klikalna, grupowana, z auto-rozmieszczeniem przy kursorze (sprawdzone w Local Studio; bez tego użytkownik nie trafia w granicę znaku).
  Grupy: `Pauza` ⏸️ 400 ms · 🛑 900 ms · 😮‍💨 600 ms; `Tempo` 🐢 ×0,85 · 🐇 ×1,15 · 🎙️ reset; `Struktura` 📖 rozdział 900 ms; `Plan akustyczny` 📻 radio · 📱 telefon · 🏛️ sala · 🤫 szept.
- Pod polem: pasek „co zrozumiałem" — lista segmentów z czasami i typem (mowa / pauza / zmiana planu). To zastępuje zgadywanie, czy emoji zadziałało.

**Panel głosu (prawa kolumna, 40%):**

- Karty głosów: nazwa, język, silnik, rozmiar, **pochodzenie i licencja** (wymóg z notatek o głosach), przycisk „próbka".
- Suwak tempa bazowego, wybór planu akustycznego domyślnego, przełącznik cache.
- Przyciski: `Czytaj`, `Stop`, `Eksport WAV`, `Eksport MP3`.

**Biblioteka nagrań (dolna szuflada):**

- Lista: tekst (skrót), głos, długość, data, liczba segmentów, czy z cache.
- Akcje: odsłuch, eksport, usuń, „wczytaj do composera".

### 8.3 Czat

- **Dwa niezależne wątki obok siebie** (split view, przełącznik A/B): każdy z własnym modelem, temperaturą i historią; wspólny tylko host LLM.
- Lista wątków w panelu bocznym z tytułem generowanym z pierwszej wiadomości.
- Przy każdej odpowiedzi asistanta: `Przeczytaj` (szybki TTS), `Przeczytaj w studiu` (przenosi tekst do composera z zachowaniem rozdziałów), `Kopiuj`, `Zapisz jako notatkę`.
- Streaming z widocznym tempem (tok/s) i tokenami; błąd modelu pokazany inline przy wiadomości, nie jako toast.
- Kontekst: wskaźnik zajętości okna kontekstu z ostrzeżeniem przed przepełnieniem.

### 8.4 Most czat ↔ studio

Jedno polecenie „Wyślij odpowiedź do studia" przenosi tekst odpowiedzi do composera, dzieli go na akapity i wstawia 📖 między sekcje. To jest funkcja, której Local Studio nie ma, a która uzasadnia trzymanie czatu i lektora w jednej aplikacji.

### 8.5 Stany i dostępność

| Stan | Zachowanie |
|---|---|
| Brak modelu LLM | Czat aktywny tylko do odczytu historii, wyraźny CTA „Pobierz model" |
| Model się ładuje | Pasek progresu z etapem (wykrywanie backendu → start procesu → warm-up), możliwość anulowania |
| Synteza trwa | Segmenty pojawiają się na bieżąco, `Stop` natychmiastowy |
| Tekst bez słów | Inline: „Nie ma czego przeczytać — tekst nie zawiera liter ani cyfr", bez błędu technicznego |
| Offline / brak backendu | Konkretna przyczyna (brak DLL, brak sterownika, za mało VRAM), nie „coś poszło nie tak" |

Skróty: `Ctrl+Enter` czytaj, `Esc` stop, `Ctrl+1/2` przełącz wątek czatu, `Ctrl+K` paleta poleceń.

### 8.6 Czego świadomie nie ma w nowym UI

- Generowania obrazów, OpenVINO, telemetrii, biblioteki transkrypcji — to zostaje w dawcy.
- Kreatora instalacji backendów — backend wybierany automatycznie, instalacja tylko na żądanie.
- Uploadu referencji do klonowania głosu — osobny temat po M5.

---

## 9. Załączniki

### 9.1 Pliki do przeniesienia (ZIELONE, kopia 1:1)

```
scripts/server/tts-directives.cjs
scripts/server/model-kind.cjs
scripts/setup/setup-piper.sh
scripts/workers/tts-voiceconv.py        (tylko jeśli klonowanie w zakresie)
docs/polish-tts-notes.md                (pochodzenie i licencje głosów)
```

### 9.2 Gramatyka dyrektyw (stan obecny)

| Emoji | Znaczenie | Parametr |
|---|---|---|
| 📖 | rozdział | pauza 900 ms |
| 🛑 | pauza długa | 900 ms |
| ⏸️ | pauza | 400 ms |
| 😮‍💨 | oddech | 600 ms (na razie cisza, nie wdech) |
| 🐢 / 🐇 / 🎙️ | tempo | ×0,85 / ×1,15 / reset |
| 📻 / 📱 / 🏛️ / 🤫 | plan akustyczny | radio / telefon / sala / szept |

Emoji spoza gramatyki są wycinane. Fragmenty bez liter i cyfr nie trafiają do silnika (naprawa crashu `wave.Error: # channels not specified`).

### 9.3 Otwarte pytania (blokują M0)

1. Ścieżka i runtime Audioticona 2 (Electron main / serwer Node / inne).
2. Czy gospodarz ma już własny TTS/LLM, czy przejmuje je w całości.
3. Znaczenie „dodatkowego czatu": drugi wątek z innym modelem czy czat z lektorem odpowiedzi.
