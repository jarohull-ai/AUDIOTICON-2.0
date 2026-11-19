# M1 — Projekt architektury bridge'a Rust ↔ Qt/C++ (Audioticon 2.0)

**Data dokumentu:** 2026-11-18  
**Autor:** Antigravity (Audyt i Architektura M1)  
**Status decyzji:** Wiążąca decyzja architektoniczna projektu:  
> **„Audioticon pozostaje w Ruście, ASTRA/QML jest natywną powłoką Qt. Nie przepisujemy rdzenia domenowego do C++.”**

---

## 1. Wprowadzenie i kontekst technologiczny

Projekt Audioticon 2.0 łączy dojrzały, wysokowydajny rdzeń domenowy napisany w języku Rust (`Audioticon-legacy/crates/core`, ponad 28 000 linii kodu) z nowoczesną, natywną powłoką GUI zbudowaną w technologii Qt 6 / QML / C++17 (`ASTRA-UI-Template/src/agent`, konsola agenta z interfejsem Glass UI).

### 1.1 Podział odpowiedzialności
```
┌────────────────────────────────────────────────────────────────────────┐
│                   ASTRA UI Shell (Qt 6 / C++17 / QML)                  │
│  - Komponenty QML: StudioView, AgentMain, ChatBubble, StateStrip       │
│  - Modele QML: AgentModel / StudioModel (Q_PROPERTY, sygnały, sloty)   │
│  - Odtwarzanie dźwięku: Qt 6 Multimedia (QAudioSink / QMediaPlayer)    │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                         Natywny Bridge (In-Process)
                                    │
┌───────────────────────────────────▼────────────────────────────────────┐
│                    Audioticon Core (Rust Workspace)                    │
│  - Parser reżyserii: EmojiDirector (gramatyka, style produkcji, emocje)│
│  - Normalizacja tekstu: verbalizer języka polskiego                    │
│  - Sentence-level Cache: SHA-256 (v12), detekcja trafień, retencja LRU │
│  - Silnik audio & Piper TTS: PiperBridge, ONNX Runtime, mikser WAV     │
│  - DSP & Mastering: korekcja studyjna, WSOLA, ACX ledger, filtry       │
│  - Asynchroniczny Runtime: Tokio (wielowątkowe renderowanie potokowe)  │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Porównanie wariantów bridge'a Rust ↔ Qt/C++

Wybór mechanizmu komunikacji pomiędzy Rustem a Qt/C++ determinuje wydajność, stabilność, bezpieczeństwo pamięci oraz ergonomię pracy zespołu. Przeanalizowano trzy podejścia:

| Kryterium | 1. `cxx` (Bezpieczny bridge Rust ↔ C++) | 2. `cbindgen` + C ABI (`extern "C"`) | 3. CLI jako Subprocess (`QProcess` + JSON) |
|---|---|---|---|
| **Model komunikacji** | In-process, bezpośrednie wywołania funkcji C++ ↔ Rust | In-process, płaski interfejs C z wskaźnikami i uchwytami (`handle`) | Międzyprocesowy (IPC), strumienie stdin/stdout/pliki |
| **Latencja wywołań** | **Sub-mikrosekundowa** (< 1 µs, bezpośredni skok w pamięci) | **Sub-mikrosekundowa** (< 1 µs) | **Wysoka** (5–50 ms na proces / narzut serializacji JSON) |
| **Bezpieczeństwo pamięci** | **Najwyższe** (weryfikacja na etapie kompilacji, brak surowych `void*`, RAII) | **Średnie/Niskie** (ryzyko wycieków, manualne `free()`, dangling pointers) | **Pełna izolacja procesowa** (crash nie ubija powłoki Qt) |
| **Konwersja typów** | Bezpośrednia: `rust::Str`, `rust::String`, `rust::Vec`, `rust::Box<T>` ↔ `std::string`, `std::vector` | Manualna: konwersja do `char*`, tablic C, kopiowanie buforów | Serializacja: JSON / Protobuf / parametry linii poleceń |
| **Zarządzanie czasem życia** | Automatyczne: `rust::Box<T>` zwalnia pamięć w destruktorze C++ | Manualne: konieczność parowania funkcji `create()` i `destroy()` | Cykl życia powiązany z procesem potomnym systemu operacyjnego |
| **Obsługa błędów** | Rust `Result<T, E>` mapowany na C++ exceptions (`rust::Error`) lub Result struct | Kody błędów (`int32_t`) + pobieranie tekstu błędu z wątku | Kody zakończenia procesu (`exit code`) + parsowanie strumienia stderr |
| **Zasoby w pamięci (np. modele TTS)** | Modele ONNX załadowane raz w RAM/VRAM procesu i gotowe do natychmiastowej syntezy | Modele trzymane pod wskaźnikiem `Engine*` | Konieczność ponownego ładowania modelu per komenda lub proces w tle (daemon) |
| **Przekazywanie audio (PCM/WAV)** | **Zero-copy** / bezpośredni slice `rust::Slice<const float>` do `QAudioSink` | Wskaźnik do bufora + rozmiar próbek | Zapisywanie tymczasowych plików na dysku lub piping przez strumień |
| **Złożoność budowy (build system)** | Średnia (wsparcie qmake/CMake przez prosty pre-build skrypt cargo) | Niska (kompilacja do `.so` / `.a`, nagłówek z `cbindgen`) | Zerowa (niezależne kompilacje obu binarek) |

---

## 3. Rekomendacja architektoniczna dla Audioticon 2.0

### Rekomendowany wybór: **`cxx` (In-Process Safe Bridge) jako biblioteka statyczna**

**Uzasadnienie techniczne:**
1. **Płynność interfejsu użytkownika (Zero Lag w edytorze):**
   W aplikacji desktopowej interakcja ze scenariuszem (podświetlanie dyrektyw emoji, parsowanie tekstu `parseDirectedText`, weryfikacja czy zdanie znajduje się w cache) musi odbywać się na bieżąco podczas edycji tekstu. Subprocess CLI powodowałby zacinanie interfejsu (50 ms opóźnienia i narzut setek wywołań procesu). Bridge in-process wykonuje parsowanie w ułamku milisekundy.
2. **Utrzymywanie modeli syntezy w pamięci:**
   Ładowanie modelu Piper ONNX (np. `pl_bass`) z dysku zajmuje od 200 do 600 ms. Zastosowanie CLI wymagałoby albo ponownego ładowania modelu przy każdym wyrenderowanym zdaniu, albo tworzenia skomplikowanego protokołu IPC z procesem daemonem. Bridge in-process pozwala utrzymać załadowany model w strukturze `PiperBridge` w pamięci RAM/VRAM i natychmiastowo syntezować pojedyncze zdania.
3. **Bezpieczeństwo pamięci bez powtarzalnego boilerplate'u:**
   `cxx` w przeciwieństwie do `cbindgen` generuje bezpieczny kod C++17. Pamięć obiektów Rusta jest automatycznie czyszczona przy użyciu semantyki RAII (`rust::Box<T>`), co zapobiega wyciekom pamięci w długo działającej aplikacji desktopowej.
4. **Natywne strumieniowanie audio:**
   Wygenerowane próbki PCM mogą być przekazywane bezpośrednio z Rusta do `QAudioSink` (Qt Multimedia) za pomocą `rust::Slice<const int16_t>` bez zbędnego kopiowania i bez konieczności ciągłego odczytu i zapisu na dysku SSD.

*Uwaga dot. CLI subprocess:* CLI (`crates/cli`) pozostaje w projekcie jako niezależne narzędzie do automatyzacji, skryptów wsadowych i przetwarzania bezgłowego (headless), lecz nie stanowi głównego mechanizmu napędzającego lokalne GUI.

---

## 4. Projekt pierwszego kontraktu (Rust ↔ C++ ↔ QML)

Kontrakt bridge'a definiuje strukturę biblioteki pośredniczącej `audioticon-bridge` (utworzonej jako crate w przestrzeni roboczej Rusta, eksportującej statyczną bibliotekę `libaudioticon_bridge.a`).

### 4.1 Moduł 1: `parseDirectedText`

Funkcja odpowiada za analizę tekstu wprowadzonego przez użytkownika, normalizację języka polskiego, ekstrakcję dyrektyw emoji, podział na segmenty, wyznaczenie temp, pauz i rozdziałów.

#### Definicja w `cxx::bridge` (Rust):
```rust
#[cxx::bridge(namespace = "audioticon::bridge")]
mod ffi {
    struct DirectiveItemDto {
        token: String,
        kind: String,       // "pause" | "breath" | "speed" | "dsp" | "chapter" | "emotion"
        pause_ms: u32,
        speed: f64,
        dsp: String,        // "radio" | "phone" | "cathedral" | "whisper"
        label: String,
    }

    struct VoiceSegmentDto {
        index: u32,
        text: String,
        speed: f64,
        dsp: String,
        pause_ms: u32,
        voice: String,
        actor_emotion: String,
        cache_hash: String,
    }

    struct ChapterMarkerDto {
        segment_index: u32,
        title: String,
    }

    struct DirectedTextDto {
        plain_text: String,
        leading_pause_ms: u32,
        segments: Vec<VoiceSegmentDto>,
        chapters: Vec<ChapterMarkerDto>,
        directives: Vec<DirectiveItemDto>,
        total_pause_ms: u32,
    }

    extern "Rust" {
        type BridgeEngine;

        fn create_engine(cache_dir: &str, models_dir: &str) -> Result<Box<BridgeEngine>>;
        fn parse_directed_text(
            engine: &BridgeEngine,
            raw_text: &str,
            production_style: &str,
            normalize_polish: bool,
        ) -> Result<DirectedTextDto>;
    }
}
```

#### Mapowanie w C++ (Qt/QML):
C++ udostępnia klasę `StudioBridge : public QObject` zarejestrowaną w QML jako kontekst lub typ:
```cpp
// studiobridge.h
Q_INVOKABLE QVariantMap parseScript(const QString &rawText, const QString &style = "audio-drama") {
    rust::Str textSlice(rawText.toUtf8().constData(), rawText.toUtf8().size());
    try {
        auto dto = m_engine->parse_directed_text(textSlice, style.toStdString(), true);
        return convertToQVariant(dto);
    } catch (const rust::Error &e) {
        emit errorOccurred(QString::fromUtf8(e.what()));
        return {};
    }
}
```

---

### 4.2 Moduł 2: `render Piper` (Synteza mowy)

Synteza poszczególnych segmentów oraz całych rozdziałów z wykorzystaniem silnika Piper TTS.

#### Definicja kontraktu:
```rust
#[cxx::bridge(namespace = "audioticon::bridge")]
mod ffi {
    struct RenderOptions {
        voice_id: String,
        model_path: String,
        sample_rate: u32,
        apply_cadence: bool,
        apply_mastering: bool,
    }

    struct RenderResultDto {
        success: bool,
        segment_hash: String,
        wav_path: String,
        duration_ms: u64,
        cache_hit: bool,
        error_message: String,
    }

    extern "Rust" {
        fn synthesize_segment(
            engine: &BridgeEngine,
            segment: &VoiceSegmentDto,
            options: &RenderOptions,
        ) -> Result<RenderResultDto>;

        fn cancel_current_render(engine: &BridgeEngine);
    }
}
```

---

### 4.3 Moduł 3: `sentence cache` (Pamięć podręczna zdań)

Sentence-level caching jest kluczową przewagą Audioticonu. Każde wyrenderowane zdanie posiada unikalny hash SHA-256 (wersja `CACHE_VERSION = 12`) wyliczany z tekstu, prędkości, filtrów DSP, tagów aktorskich oraz identyfikatora głosu.

#### Definicja kontraktu:
```rust
#[cxx::bridge(namespace = "audioticon::bridge")]
mod ffi {
    struct CacheStatsDto {
        total_files: u64,
        total_bytes: u64,
        size_mb: f64,
    }

    extern "Rust" {
        fn compute_segment_hash(
            segment: &VoiceSegmentDto,
            voice_id: &str,
        ) -> String;

        fn is_segment_cached(engine: &BridgeEngine, hash: &str) -> bool;

        fn get_cached_wav_path(engine: &BridgeEngine, hash: &str) -> String;

        fn get_cache_stats(engine: &BridgeEngine) -> Result<CacheStatsDto>;

        fn prune_cache_lru(engine: &BridgeEngine, max_bytes: u64) -> Result<u64>;

        fn clear_all_cache(engine: &BridgeEngine) -> Result<()>;
    }
}
```

---

### 4.4 Moduł 4: `status zadania` (Wielowątkowy Task Progress & State)

Długotrwałe operacje renderowania (np. renderowanie całego rozdziału złożonego z 200 zdań) muszą informować interfejs QML o postępie bez blokowania pętli zdarzeń Qt.

#### Definicja kontraktu (Wzorzec Obserwatora C++):
```rust
#[cxx::bridge(namespace = "audioticon::bridge")]
mod ffi {
    enum TaskState {
        Idle,
        Queued,
        Synthesizing,
        Mastering,
        Completed,
        Failed,
        Cancelled,
    }

    struct ProgressUpdate {
        job_id: u64,
        state: TaskState,
        current_segment: u32,
        total_segments: u32,
        cache_hits: u32,
        percent: f32,
        eta_seconds: u32,
        status_message: String,
    }

    extern "C++" {
        include "TaskObserver.h";
        type TaskObserver;

        // Callback wywoływany z wątku roboczego Rusta do C++
        fn on_progress(self: &TaskObserver, update: ProgressUpdate);
    }

    extern "Rust" {
        fn start_batch_render(
            engine: &BridgeEngine,
            script: &DirectedTextDto,
            options: &RenderOptions,
            observer: &TaskObserver,
        ) -> Result<u64>; // Zwraca job_id
    }
}
```

---

### 4.5 Moduł 5: `odtworzenie PCM/WAV` (Audio Streaming & Playback)

Dla natychmiastowego odsłuchu pojedynczego zdania (lub po kliknięciu ikony głośnika w QML) dane audio mogą być przekazane na dwa sposoby:
1. **Gotowy plik WAV z cache** — odtwarzany przez `QMediaPlayer` lub `QSoundEffect`.
2. **Strumień bufora PCM w pamięci** — bez zapisu na dysku, bezpośrednio do `QAudioSink`.

#### Definicja kontraktu bufora PCM:
```rust
#[cxx::bridge(namespace = "audioticon::bridge")]
mod ffi {
    struct AudioBufferDto {
        sample_rate: u32,
        channels: u16,
        bits_per_sample: u16,
        // Surowe próbki PCM (16-bit signed integer) przekazane bezpiecznie jako slice
        samples: Vec<i16>,
    }

    extern "Rust" {
        fn render_segment_to_pcm(
            engine: &BridgeEngine,
            segment: &VoiceSegmentDto,
            options: &RenderOptions,
        ) -> Result<AudioBufferDto>;
    }
}
```
W C++ próbki z `AudioBufferDto::samples` są natychmiast pakowane do `QByteArray::fromRawData` i przesyłane do `QIODevice` podłączonego do `QAudioSink`.

---

## 5. Bezpieczeństwo pamięci, obsługa błędów i wielowątkowość

### 5.1 Bezpieczeństwo pamięci (Memory Safety & Lifetimes)
- **Brak surowych wskaźników (`void*`):** Wszystkie struktury Rusta przekazywane do C++ są opakowane w `rust::Box<BridgeEngine>`, co na poziomie C++ odpowiada `std::unique_ptr`. Usunięcie obiektu w C++ automatycznie wywołuje destruktor `Drop` w Ruście.
- **Wymiana ciągów tekstowych:** Użycie `rust::Str` (niemutowalne referencje UTF-8) eliminuje zbędne alokacje przy odczycie z `QString::toUtf8()`. Przy zwracaniu danych do Qt, `rust::String` jest kopiowany do `QString::fromUtf8()`.
- **Bufory próbek audio:** Slices `rust::Slice<const i16>` gwarantują poprawność granic tablic i brak przepełnień buforów (brak buffer overflow).

### 5.2 Obsługa błędów (Error Handling)
- Wszelkie błędy domenowe w Ruście implementują `std::error::Error` (za pomocą `thiserror`/`anyhow`).
- W bridge'u `cxx`, zwrot `Result<T, E>` jest automatycznie tłumaczony na rzucenie wyjątku C++ `rust::Error` (pochodzącego od `std::exception`).
- Wrapper C++ przechwytuje wyjątki w blokach `try-catch` i transformuje je w sygnały Qt (`emit errorOccurred(QString)`), dzięki czemu żadna niespodziewana sytuacja w wątku audio nie powoduje paniki i awarii całej aplikacji GUI.

### 5.3 Wielowątkowość i asynchroniczność (Concurrency Model)
1. **Izolacja wątku głównego GUI (Main Thread):**
   Wątek główny Qt (na którym działa silnik sceny QML i obsługa zdarzeń okna) **nigdy** nie wykonuje blokujących operacji TTS ani zapisu na dysku.
2. **Wewnętrzny Runtime Rusta:**
   `BridgeEngine` w Ruście posiada własny wielowątkowy runtime Tokio (`tokio::runtime::Runtime`). Zadania renderowania są kolejkowane w puli wątków workerów.
3. **Powiadamianie Qt przez `QueuedConnection`:**
   Gdy wątek Rusta wywołuje metodę `TaskObserver::on_progress`, kod C++ wykonuje:
   ```cpp
   void TaskObserver::on_progress(ProgressUpdate update) const {
       // Bezpieczne przekazanie do wątku głównego Qt
       QMetaObject::invokeMethod(m_targetObject, [this, update]() {
           m_targetObject->handleProgressFromWorker(update);
       }, Qt::QueuedConnection);
   }
   ```
   Gwarantuje to pełne bezpieczeństwo wątkowe (brak data race) i natychmiastowe odświeżenie kontrolek QML.

---

## 6. Integracja w projekcie Qt (`agent-console.pro`)

Bridge zostanie dołączony do projektu Qt za pomocą dedykowanego pliku konfiguracyjnego `bridge.pri`:

```qmake
# bridge.pri - Integracja natywnego bridge'a Audioticon Rust

INCLUDEPATH += $$PWD/../bridge/include $$PWD/../bridge/target/cxxbridge/audioticon-bridge/src

# Ścieżka do zbudowanej biblioteki Rusta
win32 {
    LIBS += -L$$PWD/../bridge/target/release -laudioticon_bridge -lws2_32 -luserenv -lbcrypt
} else {
    LIBS += -L$$PWD/../bridge/target/release -laudioticon_bridge -lpthread -ldl -lm
}

# Automatyczna kompilacja biblioteki Rust przed linkowaniem Qt
rust_build.target = $$PWD/../bridge/target/release/libaudioticon_bridge.a
rust_build.commands = cargo build --release --manifest-path $$PWD/../bridge/Cargo.toml
PRE_TARGETDEPS += $$rust_build.target
QMAKE_EXTRA_TARGETS += rust_build
```

---

## 7. Wnioski i plan wdrożenia (Roadmap)

1. **Utrzymanie decyzji bazowej:** Rdzeń domenowy w Ruście (synteza, pipeline audio, cache, format `.audioticon`) pozostaje w 100% zachowany. Nie przepisujemy 28 000 linii Rusta do C++.
2. **Wykorzystanie osiągnięć E1:** Zrealizowany w E1 port `DirectiveParser` i testy golden (17 passed) posłużą jako referencyjny zestaw walidacyjny sprawdzający poprawność działania mostu FFI w relacji 1:1.
3. **Kolejne kroki (M2):**
   - Utworzenie crate'u `crates/bridge` z definicją `cxx::bridge`.
   - Podpięcie `BridgeEngine` do `agent-console.pro`.
   - Zastąpienie symulowanego modelu `AgentModel` rzeczywistym modelem `StudioModel` zasilanym przez silnik Audioticon.
