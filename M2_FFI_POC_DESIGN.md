# M2 — Projekt koncepcyjny FFI Bridge (`cxx::bridge`) dla Audioticon 2.0

**Data:** 2026-11-18  
**Autor:** Antigravity (Audyt i Architektura M2)  
**Cel:** Zaprojektowanie minimalnego, bezpiecznego proof-of-concept mostu `cxx::bridge` łączącego rdzeń domenowy Rusta z natywną powłoką Qt 6 / C++.  
**Zakres ograniczenia:** Wyłącznie 5 kluczowych elementów kontraktu, bez implementacji Pipera ani QML, bez modyfikacji repozytoriów źródłowych.

---

## 1. Architektura mostu `cxx::bridge`

`cxx` generuje bezpieczne wiązania między Rustem a C++17, gwarantując sprawdzanie typów i poprawności granic w czasie kompilacji. Eliminuje konieczność pisania surowego kodu `unsafe` w warstwie C++ i zapobiega wyciekom pamięci.

```
┌────────────────────────────────────────────────────────┐
│                   Qt / C++ (ASTRA UI)                  │
│  - StudioBridge (QObject na wątku głównym GUI)         │
│  - BridgeObserver (C++ klasa powiadamiająca Qt)        │
└───────────────────────────┬────────────────────────────┘
                            │
               Wywołania C++ / Callbacki
               Granica FFI (cxx::bridge)
                            │
┌───────────────────────────▼────────────────────────────┐
│                  audioticon-bridge (Rust)              │
│  - BridgeEngine (Box<BridgeEngine> z RAII)             │
│  - parse_directed_text()                               │
│  - AudioBufferDto (własnościowy Vec<i16>)              │
│  - Wątek roboczy (Tokio / std::thread)                 │
└────────────────────────────────────────────────────────┘
```

---

## 2. Projekt kontraktu `cxx::bridge` (Plik: `crates/bridge/src/ffi.rs`)

Poniższy blok definiuje formalny interfejs FFI:

```rust
#[cxx::bridge(namespace = "audioticon::bridge")]
pub mod ffi {
    // ------------------------------------------------------------------------
    // 1. Struktury danych dla parseDirectedText
    // ------------------------------------------------------------------------
    struct DirectiveItemDto {
        token: String,
        kind: String,       // "pause" | "breath" | "speed" | "dsp" | "chapter" | "emotion"
        pause_ms: u32,
        speed: f64,
        dsp: String,
        label: String,
    }

    struct VoiceSegmentDto {
        index: u32,
        text: String,
        speed: f64,
        dsp: String,
        pause_ms: u32,
        leading_pause_ms: u32,
        cache_hash: String,
    }

    struct ChapterDto {
        segment_index: u32,
        title: String,
    }

    struct DirectedTextResultDto {
        segments: Vec<VoiceSegmentDto>,
        chapters: Vec<ChapterDto>,
        directives: Vec<DirectiveItemDto>,
        leading_pause_ms: u32,
        plain_text: String,
    }

    // ------------------------------------------------------------------------
    // 2. AudioBufferDto z własnościowym buforem PCM
    // ------------------------------------------------------------------------
    struct AudioBufferDto {
        sample_rate: u32,
        channels: u16,
        bits_per_sample: u16,
        duration_ms: u64,
        // Własnościowy bufor próbek PCM (16-bit signed).
        // cxx mapuje Vec<i16> na rust::Vec<int16_t> w C++.
        samples: Vec<i16>,
    }

    // ------------------------------------------------------------------------
    // 3. RenderStatus i maszyna stanów zadania
    // ------------------------------------------------------------------------
    #[derive(Copy, Clone, PartialEq, Eq)]
    enum TaskStage {
        Idle,
        Queued,
        Normalizing,
        Synthesizing,
        Mastering,
        Finished,
        Failed,
        Cancelled,
    }

    struct RenderStatusDto {
        job_id: u64,
        stage: TaskStage,
        current_segment: u32,
        total_segments: u32,
        cache_hits: u32,
        progress_percent: f32,
        status_message: String,
        error_message: String,
    }

    // ------------------------------------------------------------------------
    // 4 & 5. Interfejsy C++ i Rusta
    // ------------------------------------------------------------------------
    extern "C++" {
        include "BridgeObserver.h";
        type BridgeObserver;

        // Metoda wywoływana z wątku Rusta do C++ z aktualizacją statusu
        fn on_progress(self: &BridgeObserver, status: RenderStatusDto);
    }

    extern "Rust" {
        type BridgeEngine;

        // Tworzenie i cykl życia silnika
        fn create_engine() -> Result<Box<BridgeEngine>>;

        // 1. parseDirectedText
        fn parse_directed_text(
            engine: &BridgeEngine,
            raw_text: &str,
            polish: bool,
        ) -> Result<DirectedTextResultDto>;

        // 2. Generowanie/Pobranie AudioBufferDto (własnościowy bufor PCM)
        fn create_test_tone_pcm(
            engine: &BridgeEngine,
            frequency_hz: f32,
            duration_ms: u32,
        ) -> Result<AudioBufferDto>;

        // 3 & 5. Uruchomienie zadania w tle z asynchronicznym raportowaniem
        fn run_mock_task_async(
            engine: &BridgeEngine,
            job_id: u64,
            total_steps: u32,
            observer: &BridgeObserver,
        ) -> Result<()>;
    }
}
```

---

## 3. Szczegóły implementacji poszczególnych komponentów

### 3.1 `parseDirectedText` (Implementacja w Ruście)
Most wykorzystuje istniejące moduły `EmojiDirector` oraz `normalize_polish_text` z `audioticon_core`:
```rust
pub struct BridgeEngine {
    director: audioticon_core::parser::EmojiDirector,
}

impl BridgeEngine {
    pub fn new() -> Self {
        Self {
            director: audioticon_core::parser::EmojiDirector::default(),
        }
    }

    pub fn parse_directed_text(&self, raw_text: &str, polish: bool) -> Result<DirectedTextResultDto, BridgeError> {
        if raw_text.is_empty() {
            return Ok(DirectedTextResultDto::default());
        }

        let script = self.director.parse(raw_text);
        let mut segments_dto = Vec::new();
        let mut chapters_dto = Vec::new();
        let mut directives_dto = Vec::new();
        let mut plain_text = String::new();

        // Mapowanie DirectedScript na struktury DTO...
        for (idx, item) in script.items.iter().enumerate() {
            match item {
                audioticon_core::types::DirectedItem::Speech(seg) => {
                    let text = if polish {
                        audioticon_core::verbalizer::normalize_polish_text(&seg.text)
                    } else {
                        seg.text.clone()
                    };
                    plain_text.push_str(&text);
                    plain_text.push(' ');
                    segments_dto.push(VoiceSegmentDto {
                        index: idx as u32,
                        text,
                        speed: 1.0 + (seg.rate_mod as f64 / 100.0),
                        dsp: seg.filter.clone().unwrap_or_default(),
                        pause_ms: 0,
                        leading_pause_ms: 0,
                        cache_hash: audioticon_core::cache::compute_segment_hash(seg, "default"),
                    });
                }
                audioticon_core::types::DirectedItem::Chapter(title) => {
                    chapters_dto.push(ChapterDto {
                        segment_index: idx as u32,
                        title: title.clone(),
                    });
                }
                _ => {}
            }
        }

        Ok(DirectedTextResultDto {
            segments: segments_dto,
            chapters: chapters_dto,
            directives: directives_dto,
            leading_pause_ms: 0,
            plain_text: plain_text.trim().to_string(),
        })
    }
}
```

---

### 3.2 Własnościowy bufor PCM (`AudioBufferDto`) w C++

Bufor próbek `samples: Vec<i16>` jest przekazywany jako `rust::Vec<int16_t>`.
- **Brak wycieków pamięci:** `AudioBufferDto` jest zwracany przez wartość lub `std::unique_ptr`. Pamięć próbek alokowana w Ruście jest zwalniana natychmiast, gdy obiekt C++ wychodzi z zakresu (RAII).
- **Konwersja do Qt Multimedia (`QAudioSink` / `QByteArray`):**
```cpp
// Przykład użycia w C++
void StudioBridge::playBuffer(const audioticon::bridge::AudioBufferDto &buffer) {
    // 1. Zero-copy dostęp do bufora próbek int16
    const int16_t *dataPtr = buffer.samples.data();
    size_t sampleCount = buffer.samples.size();
    size_t byteCount = sampleCount * sizeof(int16_t);

    // 2. Opakowanie w QByteArray dla Qt (opcjonalne kopiowanie lub buforowanie IODevice)
    QByteArray pcmData(reinterpret_cast<const char*>(dataPtr), static_cast<int>(byteCount));

    // 3. Konfiguracja formatu audio dla QAudioSink
    QAudioFormat format;
    format.setSampleRate(buffer.sample_rate);
    format.setChannelCount(buffer.channels);
    format.setSampleFormat(QAudioFormat::Int16);

    // 4. Strumieniowanie do urządzenia audio
    m_audioSinkDevice->write(pcmData);
}
```

---

### 3.3 Obsługa błędów: `RustError` → Bezpieczny komunikat dla Qt

W mechanizmie `cxx`:
1. Jeżeli funkcja w Ruście zwraca `Result<T, E>`, gdzie `E` implementuje `std::error::Error`, to w przypadku błędu `cxx` automatycznie generuje i rzuca w C++ wyjątek typu `rust::Error` (będący podklasą `std::exception`).
2. Treść `what()` wyjątku zawiera sformatowany komunikat z Rusta.
3. W warstwie pośredniczącej C++ każdy punkt wejścia jest zabezpieczony blokiem `try-catch`:

```cpp
QVariantMap StudioBridge::parseScript(const QString &rawScript) {
    try {
        rust::Str textStr(rawScript.toUtf8().constData(), rawScript.toUtf8().size());
        auto result = m_engine->parse_directed_text(textStr, true);
        return convertToQVariant(result);
    } catch (const rust::Error &e) {
        // Bezpieczne przekazanie błędu do UI bez wyłożenia aplikacji
        const QString errorMsg = QString::fromUtf8(e.what());
        qWarning() << "[AudioticonBridge Error]:" << errorMsg;
        emit errorOccurred("PARSE_ERROR", errorMsg);
        return {};
    }
}
```

---

### 3.4 Asynchroniczny callback postępu bez blokowania GUI

Aby wątek GUI nie został zablokowany, długotrwałe operacje (np. mock task / render) uruchamiane są w puli wątków Rusta. Aktualizacje postępu trafiają do kolejki zdarzeń Qt za pośrednictwem `QMetaObject::invokeMethod`.

#### Nagłówek C++: `BridgeObserver.h`
```cpp
#pragma once

#include <QObject>
#include <QPointer>
#include <QMetaObject>

// Wstępna deklaracja struktury wygenerowanej przez cxx
namespace audioticon::bridge {
    struct RenderStatusDto;
}

class StudioBridge;

class BridgeObserver {
public:
    explicit BridgeObserver(StudioBridge *target) : m_target(target) {}

    // Metoda wywoływana bezpośrednio z wątku roboczego Rusta
    void on_progress(audioticon::bridge::RenderStatusDto status) const;

private:
    QPointer<StudioBridge> m_target;
};
```

#### Implementacja C++: `BridgeObserver.cpp`
```cpp
#include "BridgeObserver.h"
#include "StudioBridge.h"
#include "audioticon-bridge/src/ffi.rs.h"

void BridgeObserver::on_progress(audioticon::bridge::RenderStatusDto status) const {
    if (!m_target) return;

    // Przekazanie aktualizacji z wątku roboczego Rusta na główny wątek Qt
    // Qt::QueuedConnection zapobiega wyścigom danych i odświeża QML w głównym wątku.
    QMetaObject::invokeMethod(m_target.data(), [target = m_target, status]() {
        if (target) {
            target->handleStatusUpdate(status);
        }
    }, Qt::QueuedConnection);
}
```

#### Implementacja w Ruście (asynchroniczny worker):
```rust
pub fn run_mock_task_async(
    _engine: &BridgeEngine,
    job_id: u64,
    total_steps: u32,
    observer: &ffi::BridgeObserver,
) -> Result<(), BridgeError> {
    // Klonujemy observer dla wątku w tle (dzięki wskaźnikowi)
    let observer_ptr = observer as *const ffi::BridgeObserver as usize;

    std::thread::spawn(move || {
        let obs = unsafe { &*(observer_ptr as *const ffi::BridgeObserver) };
        for step in 1..=total_steps {
            std::thread::sleep(std::time::Duration::from_millis(50));
            let percent = (step as f32 / total_steps as f32) * 100.0;
            
            obs.on_progress(ffi::RenderStatusDto {
                job_id,
                stage: if step == total_steps { ffi::TaskStage::Finished } else { ffi::TaskStage::Synthesizing },
                current_segment: step,
                total_segments: total_steps,
                cache_hits: step / 2,
                progress_percent: percent,
                status_message: format!("Renderowanie kroku {} z {}", step, total_steps),
                error_message: String::new(),
            });
        }
    });

    Ok(())
}
```

---

## 4. Wykaz plików do utworzenia / modyfikacji w kolejnym etapie (M3)

Do wdrożenia opisanego mostu w roboczej kopii UI (`ui`) oraz przestrzeni Rusta potrzebne będą następujące pliki:

### A. Nowe pliki po stronie Rusta:
1. `/home/jaro/.openclaw/workspace/Audioticon-legacy/crates/bridge/Cargo.toml`  
   - Definicja crate'u `audioticon-bridge` (typ `crate-type = ["staticlib", "rlib"]`), zależność do `cxx = "1.0"`, `audioticon-core`, `thiserror`.
2. `/home/jaro/.openclaw/workspace/Audioticon-legacy/crates/bridge/build.rs`  
   - Skrypt budowania `cxx_build::bridge("src/ffi.rs").compile("audioticon-bridge")`.
3. `/home/jaro/.openclaw/workspace/Audioticon-legacy/crates/bridge/src/lib.rs`  
   - Główny plik eksportujący moduły i implementację `BridgeEngine`.
4. `/home/jaro/.openclaw/workspace/Audioticon-legacy/crates/bridge/src/ffi.rs`  
   - Deklaracja makra `#[cxx::bridge]` z kontraktem opisanym w sekcji 2.

### B. Nowe i modyfikowane pliki po stronie Qt/C++ (`ui`):
1. `/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/ui/src/bridge/BridgeObserver.h`  
   - Nagłówek obserwatora postępu wywoływanego przez Rust.
2. `/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/ui/src/bridge/BridgeObserver.cpp`  
   - Implementacja z `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.
3. `/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/ui/src/bridge/StudioBridge.h`  
   - Wrapper `QObject` eksponujący metody i sygnały do QML (`parseScript`, `audioProgress`, `errorOccurred`).
4. `/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/ui/src/bridge/StudioBridge.cpp`  
   - Konwersja struktur DTO Rusta na `QVariantMap` / `QVariantList` dla widoków QML.
5. `/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/ui/bridge.pri`  
   - Reguły qmake dla nagłówków `cxxbridge`, ścieżek `INCLUDEPATH` oraz linkowania statycznego `libaudioticon_bridge.a`.
6. `/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/ui/agent-console.pro`  
   - Dodanie `include(bridge.pri)`.

---

## 5. Podsumowanie weryfikacji założeń

1. **Brak Pipera i QML na tym etapie:** Projekt obejmuje wyłącznie warstwę FFI C++/Rust, struktury DTO, bufor PCM i wielowątkowość.
2. **Bezpieczeństwo baz:** Żadne repozytorium nie zostało zmienione. Projekt przygotowano pod realizację w dedykowanej roboczej kopii `ui`.
3. **Zgodność z decyzją architektoniczną:** Rdzeń pozostaje w 100% w Ruście; C++ w warstwie ASTRA pełni rolę bezpiecznego klienta konsumującego usługi silnika.
