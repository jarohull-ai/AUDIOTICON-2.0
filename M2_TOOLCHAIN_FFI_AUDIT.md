# M2 — Audyt toolchainu i mostka FFI (Rust ↔ Qt/C++)

**Data:** 2026-11-18
**Rola:** niezależny audytor techniczny Audioticon 2.0
**Zakres:** weryfikacja wykonalności integracji in-process przez `cxx::bridge` między istniejącym rdzeniem Rust (`Audioticon-legacy`) a powłoką UI ASTRA (Qt 6 / QML / C++17), bez HTTP i bez webowego UI.

**Zasady wykonania:** kod produkcyjny **nie był zmieniany**, pakiety **nie były instalowane**, modele **nie były pobierane**. Jedyne artefakty poza tym raportem to katalog `/tmp/ort-probe/` (program sondujący API biblioteki ORT, poza drzewami repo). Working tree wszystkich trzech repozytoriów pozostał czysty — weryfikacja w §10.

---

## 0. Podsumowanie (TL;DR)

| # | Pytanie | Odpowiedź | Wyrok |
|---|---|---|---|
| 1 | Wersje Rust / Cargo / C++17 / Qt | rustc 1.100.0, cargo 1.100.0, GCC 16.2.1 (`-std=c++17`), Qt 6.12.0, tylko `qmake6` | ✅ zgodne |
| 2 | Czy dodać `cxx`/`cxx-build` do workspace | Tak, bez konfliktu: brak `cxx` w `Cargo.lock`, MSRV `cxx` ≪ 1.92, `rustc` 1.100.0 spełnia `rust-version = "1.92"` | ✅ dodatek addytywny |
| 3 | Struktura `crates/core` i punkt wystawienia bridge'a | 32 pliki `.rs`, 8 558 linii w 10 kluczowych; **nowy crate `crates/bridge`**, zero zmian w `core`/`cli` | ✅ rekomendacja |
| 4 | ONNX Runtime i `ORT_DYLIB_PATH` | **Odkrycie krytyczne:** `api-31` wymaga ORT ≥ 1.31. Lokalne `/opt/superwhisper` = **1.24.2 → `GetApi(31)` = NULL**. Na dysku są jednak biblioteki **1.32.0**, które `GetApi(31)` obsługują | ⚠️ działa, ale nie z tą biblioteką, o którą się sądzi |
| 5 | Kompilacja biblioteki Rust dla Qt | `crate-type = ["staticlib"]` + `cxx-build` w `build.rs` + `LIBS += -laudioticon_bridge -lpthread -ldl -lm` | ✅ wykonalne |
| 6 | Tokio ↔ Qt event loop | Realne ryzyko. Wymagane: jeden `Runtime` w tle, `block_on` **nigdy** na wątku GUI, wynik przez polling lub `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` | ⚠️ wymaga dyscypliny |
| 7 | Struktura katalogów bridge'a | `crates/bridge/` + `ui/src/audioticon/` (§7) | ✅ |
| 8 | Minimalna kolejność bez modeli | Kroki 1–5 i 7 wykonalne **bez pobierania czegokolwiek**; synteza Piper (krok 6) wymaga 114 MB | ✅ 6 z 7 kroków bez modeli |

**Najważniejsze trzy zdania:**

1. Integracja jest wykonalna i nie wymaga ruszenia linii kodu w `Audioticon-legacy` — cały nowy kod mieści się w jednym nowym crate i w katalogu `src/audioticon/` powłoki Qt.
2. Problem ORT jest **rozwiązywalny bez zmiany kodu i bez pobierania**: wystarczy wskazać `ORT_DYLIB_PATH` na istniejącą bibliotekę 1.32.0. Obecna diagnoza („brak ORT w ścieżce loadera") była niepełna — prawdziwą przeszkodą jest **niezgodność wersji API**, nie brak pliku.
3. Największym ryzykiem technicznym nie jest FFI ani linkowanie, lecz **async rdzenia wciąż obecny w długo żyjącym procesie GUI** — z jednorazowym `OnceLock` na ORT i blokującymi wywołaniami `std::process::Command` w środku runtime'u Tokio.

---

## 1. Toolchain — stan faktyczny

| Narzędzie | Wersja | Uwaga w kontekście bridge'a |
|---|---|---|
| `rustc` | **1.100.0** (2026-11-09) | spełnia `rust-version = "1.92"` z `[workspace.package]` |
| `cargo` | **1.100.0** (2026-11-05) | resolver 2 |
| `g++` / `gcc` | **16.2.1** (20260810) | linkuje `staticlib` Rusta bez problemu; **nie jest LLVM** → brak cross-language LTO z `rustc` |
| `qmake6` | QMake 3.1, **Qt 6.12.0** (`/usr/lib`) | ⚠️ `qmake` (Qt5) **nie istnieje** — każdy skrypt musi wołać `qmake6` |
| Qt Multimedia | **6.12.0** (`libQt6Multimedia.so.6.12.0`, nagłówek `QAudioSink`, `pkg-config` 6.12.0) | odtwarzanie PCM przez `QAudioSink` jest dostępne — nie trzeba `QMediaPlayer` ani ffmpeg do odsłuchu |
| `ffmpeg` | **n9.0.2** | źródło znanego czerwonego testu FLAC (M0 §2.2); ścieżka syntezy używa `-ar 44100`, więc nie jest dotknięta |
| `espeak-ng` | **brak** | blokuje fonemizację Kokoro; Piper jej nie potrzebuje |
| `node` / `npm` | 26.13.1 / 11.20.0 | nieistotne dla bridge'a |

### 1.1 Edycja i standard C++

`[workspace.package] edition = "2021"`. Po stronie C++ ASTRA deklaruje `CONFIG += c++17`, co daje `-std=gnu++17`.

- **`cxx` wymaga C++14 minimum** → C++17 jest w porządku, bez zmian w `.pro`.
- `ort-sys` 2.0.0-rc.15 ma `edition = "2024"` i `rust-version = "1.92"` — oba spełnione (rustc 1.100.0). To istotne, bo gdyby toolchain był starszy, sam `cargo check` rdzenia by nie przeszedł; obecnie przechodzi (M0: exit 0).

### 1.2 Wniosek §1

Toolchain **nie blokuje** integracji. Jedyne realne ograniczenie to brak `qmake` z Qt5 — komendy muszą używać `qmake6`, co projekt już robi.

---

## 2. Czy można dodać `cxx` / `cxx-build` do istniejącego workspace

### 2.1 Stan wyjściowy

```toml
# Audioticon-legacy/Cargo.toml
[workspace]
resolver = "2"
members = ["crates/core", "crates/cli"]

[workspace.package]
version = "1.0.0"
edition = "2021"
rust-version = "1.92"
```

- `build.rs` **nie istnieje** w żadnym crate (potwierdzone: brak pliku w root i w `crates/*`).
- `crate-type` **nie jest zadeklarowany nigdzie** → `core` i `cli` są domyślnymi `rlib`/`bin`.
- `grep cxx Cargo.lock` → **brak trafień**. Nowa zależność nie koliduje z niczym istniejącym ani nie wymusza podbicia istniejącej wersji.
- `[workspace.dependencies]` pinuje wspólne wersje (m.in. `ort = "2.0.0-rc.15"`, `tokio` z `features = ["full"]`).

### 2.2 Weryfikacja wykonalności

| Kryterium | Wynik |
|---|---|
| MSRV `cxx` vs toolchain | `cxx = "1.0.205"` (dostępny w registry — `cargo search` zwrócił wynik, czyli indeks crates.io jest osiągalny); MSRV `cxx` jest znacznie niższy niż 1.92 | ✅ |
| Konflikt wersji | brak `cxx` w `Cargo.lock` | ✅ |
| Wpływ na `core` / `cli` | dodanie **nowego membera** nie zmienia ich manifestów; budują się jak dotąd | ✅ |
| Wpływ na CI | `cargo test --workspace` zacznie budować także bridge (nowy `build.rs`, kompilacja glue C++) → czas CI rośnie; rekomendacja: dodać `cargo check -p audioticon-core` jako szybką bramkę i bridge testować osobno | ⚠️ |
| Sieć | pierwsze `cargo build` bridge'a pobierze `cxx`, `cxx-build`, `cc`, `cxxbridge-macro`, `scratch`, `link-cplusplus` | ⚠️ jednorazowo |

### 2.3 Rekomendacja

Dodać **wyłącznie do nowego crate'a** `crates/bridge`:

```toml
# crates/bridge/Cargo.toml (projekt, nie wprowadzony w M2)
[package]
name = "audioticon-bridge"
version.workspace = true
edition.workspace = true
license.workspace = true

[lib]
name = "audioticon_bridge"
crate-type = ["staticlib"]        # JEDYNE miejsce z staticlib w całym workspace

[dependencies]
audioticon-core = { path = "../core" }
cxx = "1.0"
tokio = { workspace = true }

[build-dependencies]
cxx-build = "1.0"
```

`cxx` i `cxx-build` **nie** trafiają do `[workspace.dependencies]`, dopóki bridge jest jedynym konsumentem — trzymanie ich lokalnie ogranicza promień rażenia przy przyszłej aktualizacji.

---

## 3. Struktura `crates/core` i najlepszy punkt wystawienia bridge'a

### 3.1 Inwentarz

`crates/core/src/` — **32 pliki `.rs`**, plus dwa osadzone workery Pythona (`kokoro_worker.py`, `piper_worker.py` przez `include_str!`) i `voices.json`.

Rozmiary plików istotnych dla mostka:

| Plik | Linie | Rola w bridge |
|---|---:|---|
| `audio.rs` | 2 808 | `trait SpeechSynthesizer`, `PiperBridge`, `EdgeTtsBridge`, `ElevenLabsBridge`, `stitch_wav_files`, `create_silence_wav`, `create_breath_wav`, `build_ffmpeg_filter_chain`, `AudioDirectorEngine` |
| `parser.rs` | 1 483 | `EmojiDirector` — **nie wystawiać przez FFI** (patrz §3.3) |
| `kokoro.rs` | 1 073 | `KokoroBridge`, jedyny konsument `ort` |
| `presets.rs` | 951 | `PresetRegistry` (DSP) |
| `models.rs` | 596 | `models_dir`, katalog modeli |
| `model_hub.rs` | 502 | licencje i pochodzenie modeli |
| `types.rs` | 510 | `TextSegment`, `DirectedScript`, `EmojiAction` |
| `cache.rs` | 322 | `compute_segment_hash`, LRU |
| `pcm_stream.rs` | 183 | `PcmStreamFile` — dyskowy strumień PCM, bounded memory |

`lib.rs` ma **130 linii i 31 bloków `pub use`** — cały publiczny API jest re-eksportowany płasko. To dobra wiadomość: bridge nie potrzebuje żadnych zmian w `core`, bo wszystko, czego potrzebuje, jest już `pub`.

### 3.2 Które elementy są „gotowe do mostu"

Rdzeń jest **asynchroniczny z natury**:

- `trait SpeechSynthesizer` deklarowany przez `#[async_trait::async_trait]`, metody `async fn synthesize(&self, segment: &TextSegment, out_path: &Path) -> Result<()>`.
- `audio.rs` i `kokoro.rs` to jedyne pliki używające `async_trait`; `tokio::` pojawia się 31× w `audio.rs` i 8× w `kokoro.rs` (`tokio::sync::Mutex`, `tokio::fs`, `tokio::process`).
- CLI opakowuje całość w `#[tokio::main]` (`crates/cli/src/main.rs`) — czyli runtime Tokio jest **założeniem architektonicznym**, nie detalem.

To determinuje kształt mostka: **FFI musi być synchroniczne i nieblokujące**, a asynchroniczność zostaje po stronie Rusta.

### 3.3 Punkt wystawienia — trzy warianty

| Wariant | Opis | Ocena |
|---|---|---|
| **A. Nowy crate `crates/bridge`** | `#[cxx::bridge]` w `bridge/src/lib.rs`, zależność `audioticon-core` | ✅ **Rekomendacja** |
| B. Feature-gated moduł w `core` | `#[cfg(feature = "ffi")] pub mod ffi;` | ❌ `core` zyskuje `build.rs` i `cxx`; testy `core` zaczynają kompilować C++; rdzeń-oracle przestaje być „nietykalny" |
| C. Osobne repo / crate poza workspace | — | ❌ rozbija `cargo test --workspace`, komplikuje pinowanie wersji `core` |

**Dlaczego A:**

- `core` i `cli` pozostają **bit w bit** nietknięte → zachowana własność „legacy jako oracle testowy", kluczowa dla parzystości golden z E1.
- Jedyne `build.rs` w całym drzewie żyje w `bridge` — łatwo je wskazać w audycie i w CI.
- Typy FFI (`SharedStruct`) są zdefiniowane **raz**, w jednym pliku, zamiast rozsiane po `core`.
- `crate-type = ["staticlib"]` dotyczy tylko `bridge`; `core` dalej jest `rlib` i może być testowany jednostkowo bez C++.

### 3.4 Co dokładnie wystawić (a czego nie)

**Wystawić (stabilne, już `pub`):**

```
Engine::new(config)                      -> status
Engine::synthesize(text, voice, speed)   -> job_id        // async w środku
Engine::job_state(job_id)                -> (state, progress, error)
Engine::cancel(job_id)
Engine::voice_catalog()                  -> JSON           // models.rs + model_hub.rs
Engine::probe_ort()                      -> JSON           // doctor
Engine::cache_stats() / cache_clear()
Engine::assemble(wav_paths, out_path)    -> status         // stitch_wav_files
```

**Nie wystawiać:**

- `parser::EmojiDirector`, `verbalizer::normalize_polish_text` — **tekst → dyrektywy należy do C++**. E1 dostarczył `DirectiveParser` + `PolishVerbalizer` z goldenem 44 + 11 przypadków zgodnym z dawcą bit-po-bicie. Wystawienie drugiego parsera przez FFI stworzyłoby **dwa źródła prawdy** i gwarantowany rozjazd przy pierwszej poprawce gramatyki.
- `types::EmojiAction`, `DirectedItem`, `ActorTag` — enomy zagnieżdżone z `Option<String>`/`Vec<ActorTag>`; przez `cxx` wymagałyby ręcznego mapowania na `SharedStruct`. Zbędne, jeśli parsowanie zostaje w C++.

**Konsekwencja architektoniczna:** granica FFI przebiega na **segmencie**, nie na tekście. C++ parsuje wejście na listę `VoiceSegment` (już to robi w E1), a do Rusta wysyła gotowe `text + rate + dsp + pause`. To najkrótszy i najstabilniejszy kontrakt.

---

## 4. ONNX Runtime i problem `ORT_DYLIB_PATH`

### 4.1 Co robi kod

`kokoro.rs:36` — `static ORT_ENV: OnceLock<Result<Environment, String>>`. Inicjalizacja raz na proces (`get_ort_environment`, linia 40). Kolejność w `find_onnxruntime_dylib()` (linia 60):

1. `$ORT_DYLIB_PATH` — jeśli istnieje, użyj;
2. `~/.local/share/audioticon/venv/lib{64}`;
3. `$VIRTUAL_ENV/lib`;
4. `/usr/lib`, `/usr/local/lib`, `/usr/lib/x86_64-linux-gnu` — rekurencyjnie, do 6 poziomów.

`/opt` **nie jest skanowane**. `ORT_DYLIB_PATH` w środowisku jest **pusty**.

`ort` jest pinowany jako `2.0.0-rc.15` z `default-features = false, features = ["std", "ndarray", "load-dynamic", "api-31"]`.

### 4.2 Co to znaczy: `api-31` ≠ dowolna wersja ORT

Dokumentacja `ort` (ort.pyke.io, version-mapping): **`ort` v2.0.0+ odpowiada ONNX Runtime v1.31.0**, a minimalna wersja API jest sterowana feature'em `api-<N>`. Zbudowany z `api-31` kod wywołuje `OrtGetApiBase()->GetApi(31)`.

Jeśli biblioteka w runtime jest starsza, `GetApi(31)` zwraca `NULL` i inicjalizacja pada — **niezależnie od tego, jak poprawnie ustawiono `ORT_DYLIB_PATH`**.

### 4.3 Dowód empiryczny (nie wnioskowanie z tabelki)

Sonda `/tmp/ort-probe/probe.c`: `dlopen` → `dlsym("OrtGetApiBase")` → `GetVersionString()` + `GetApi(n)` dla n ∈ {17,20,…,31}. Wyniki:

| Biblioteka | `GetVersionString()` | `GetApi(24)` | `GetApi(25)` | `GetApi(31)` |
|---|---|---|---|---|
| `/opt/superwhisper/lib/libonnxruntime.so` | **1.24.2** | OK | **NULL** | **NULL** |
| `~/.hermes/hermes-agent/venv/.../libonnxruntime.so.1.32.0` | **1.32.0** | OK | OK | **OK** |
| `~/Portable-Local-Studio/app/tts-runtime/piper-venv/.../libonnxruntime.so.1.32.0` | **1.32.0** | OK | OK | **OK** |
| `~/.cache/uv/archive-v0/IK0X…/libonnxruntime.so.1.32.0` | **1.32.0** | OK | OK | **OK** |

Biblioteka 1.24.2 wypisała na stderr wprost:

```
The requested API version [31] is not available, only API versions [1, 24]
are supported in this build. Current ORT Version is: 1.24.2
```

### 4.4 Wniosek — korekta wcześniejszej diagnozy

Raport M0 stwierdzał: „brak `libonnxruntime` w ścieżce loadera → natywny TTS nie ruszy". To prawda tylko częściowo:

- ❌ `/opt/superwhisper` (1.24.2) **nie zadziała** z obecnym buildem `api-31` — i nie zadziała nawet po ustawieniu `ORT_DYLIB_PATH`. To **nie** jest problem ścieżki, to problem wersji API.
- ✅ Na dysku **są** biblioteki 1.32.0, które `api-31` obsługują. Wskazanie na nie rozwiązuje sprawę **bez zmiany kodu i bez pobierania**.

### 4.5 Rekomendacja

**Dewelopment (natychmiast, zero zmian w kodzie):**

```bash
export ORT_DYLIB_PATH=/home/jaro/Portable-Local-Studio/app/tts-runtime/piper-venv/lib/python3.12/site-packages/onnxruntime/capi/libonnxruntime.so.1.32.0
```

Wybór `piper-venv` jest świadomy: to venv runtime'u TTS dawcy, więc jest semantycznie najbliższy i najmniej prawdopodobne, że zniknie przy porządkach. `~/.cache/uv/...` może zostać wyczyszczony przez GC cache'a, a `~/.hermes/...` należy do innego agenta.

**Produkcja (później):** nie zależeć od cudzego venva. Albo skopiować bibliotekę do zasobów aplikacji (`ui/runtime/libonnxruntime.so`, w `.gitignore`), albo przejść na `ort` z `download-binaries`/`copy-dylibs` (co pobiera ORT przy buildzie — świadoma decyzja, nie teraz).

**Alternatywa, jeśli wolimy zostać przy 1.24.2:** zmienić feature na `api-24` w `[workspace.dependencies]`. To **jedna linia w manifeście rdzenia**, ale dotyka `core`/`cli` — dlatego wariant odkładam jako opcję zapasową, nie jako pierwszy ruch.

### 4.6 Dwie pułapki, które trzeba znać

1. **`OnceLock` na ORT jest pułapką w GUI.** W procesie CLI trwającym sekundy to bez znaczenia. W aplikacji desktopowej działającej godzinami: pierwsza nieudana inicjalizacja zostaje **zcache'owana na całe życie procesu**. Użytkownik poprawia `ORT_DYLIB_PATH`, a aplikacja dalej twierdzi, że ORT nie ma — do restartu. `probe_ort()` w bridge musi raportować to jawnie („wynik z cache, wymagany restart"), a docelowo warto rozważyć reset.
2. **`api-31` a dostępność binarek.** ORT 1.31/1.32 nie jest trywialnie dostępny jako samodzielny artefakt — dlatego fakt, że 1.32.0 leży w trzech venvach, jest realnym ułatwieniem, ale też **kruchem** zależnością.

---

## 5. Czy da się skompilować bibliotekę Rust używaną przez Qt

**Tak.** To standardowy, dobrze udokumentowany scenariusz. Kluczowe decyzje:

### 5.1 `crate-type` i kierunek linkowania

```toml
[lib]
name = "audioticon_bridge"
crate-type = ["staticlib"]     # → libaudioticon_bridge.a
```

**Dlaczego `staticlib`, a nie `cdylib`:**

| | `staticlib` | `cdylib` |
|---|---|---|
| Dystrybucja | jeden plik binarny Qt | `.so` obok binarki |
| `LD_LIBRARY_PATH` w runtime | **niepotrzebny** | wymagany lub `RPATH` |
| Ryzyko „zła wersja .so w systemie" | brak | realne |
| Duplikacja Rusta w procesie | brak | możliwa przy dwóch `.so` |
| Czas linkowania aplikacji | dłuższy | krótszy |

Dla aplikacji desktopowej `staticlib` jest bezpieczniejszy. `cxx` obsługuje oba warianty.

### 5.2 `build.rs` i wygenerowany glue

```rust
// crates/bridge/build.rs
fn main() {
    cxx_build::bridge("src/lib.rs")
        .std("c++17")
        .compile("audioticon_bridge_cxx");
}
```

`cxx-build` generuje parę plików (`lib.rs.cc` / `lib.rs.h`) do
`target/<profile>/build/audioticon-bridge-<hash>/out/cxxbridge/`.

⚠️ **Hash w ścieżce jest problemem dla `.pro`.** qmake nie potrafi zgadywać tego katalogu. Rozwiązanie: ustalić stały `CARGO_TARGET_DIR` i wygenerować nagłówek/glue w przewidywalnym miejscu, albo dodać do `.pro` krok `QMAKE_EXTRA_TARGETS` uruchamiający `cargo build` przed linkowaniem i wskazać ścieżkę jawnie. To najbrzydszy fragment całej integracji — warto go zaprojektować raz, porządnie.

### 5.3 Linkowanie z Qt

```qmake
LIBS += -L$$PWD/../../crates/bridge/target/release -laudioticon_bridge \
        -lpthread -ldl -lm
```

- Kolejność ma znaczenie: `-laudioticon_bridge` **przed** bibliotekami systemowymi.
- `-lpthread -ldl -lm` są wymagane przez `std` Rusta (i przez `load-dynamic` ORT, który używa `dlopen`).
- **Nie** używać `-C prefer-dynamic` ani `build-std` — oba psują samowystarczalność `staticlib`.
- **Nie** włączać cross-language LTO: `g++` 16 nie jest LLVM, a `rustc` używa LLVM → niezgodne backendy.

### 5.4 Panika na granicy FFI

`cxx` **nie propaguje paniki Rusta do C++**. Panika na granicy FFI kończy proces zamiast cofać się przez ramki C++ — i to jest zachowanie pożądane, ale trzeba je traktować jako regułę projektową: **każda funkcja mostu zwraca `Result`/status, żadna nie może panikować**. W kodzie rdzenia paniki są (m.in. `expect("ORT session initialized")` w `kokoro.rs`), więc bridge musi je tłumić na swojej granicy (mapowanie `catch_unwind` → status błędu w tych miejscach, gdzie to sensowne).

### 5.5 Rozmiar i czas

- `staticlib` z `core` → oczekiwane **+3…8 MB** do binarki; `core` nie ciągnie ciężkich zależności (brak `reqwest`, brak `image`).
- Ponieważ `ort` jest z `load-dynamic`, **żadne symbole ORT nie wchodzą do `staticlib`** — linkowanie z Qt pozostaje czyste, a ORT jest rozwiązywany w runtime przez `dlopen`. To duża zaleta obecnej konfiguracji i **należy ją zachować**.
- Czas linkowania aplikacji Qt rośnie (linker musi wciągnąć `.a`), ale to jednorazowy koszt przy buildzie.

---

## 6. Tokio ↔ Qt event loop — realne problemy

To najbardziej niedoceniana część projektu. Rdzeń jest async **wszędzie**, a Qt ma własną pętlę zdarzeń na wątku głównym. Trzy klasy problemów:

### 6.1 `block_on` na wątku GUI = zamrożony interfejs

Jeśli funkcja FFI wywołana z QML zrobi `runtime.block_on(...)`:

- UI przestaje odpowiadać na czas syntezy (setki ms – sekundy);
- jeśli Qt jest w zagnieżdżonej pętli (modalne okno, `QEventLoop`), powstaje ryzyko **deadlocka**;
- `QAudioSink` i animacje QML gubią klatki.

**Reguła:** funkcje mostu **zwracają natychmiast** (np. `u64 job_id`), a praca idzie na runtime Tokio.

### 6.2 Jeden runtime, założony raz, żyjący do końca procesu

- Utworzyć **jeden** `tokio::runtime::Runtime` (`Builder::new_multi_thread().worker_threads(min(4, ncpu))`), trzymany w `OnceLock`/`LazyLock` w bridge.
- **Nie dropować** runtime'u z wątku innego niż ten, który go stworzył — `Runtime::drop` panikuje przy złym kontekście. Przy zamykaniu aplikacji trzeba mieć jawną, kontrolowaną ścieżkę `shutdown`.
- **Nie tworzyć** runtime'u per wywołanie — koszt i wycieki wątków.

### 6.3 Dwa warianty dostarczania wyniku

| Wariant | Mechanizm | Ocena |
|---|---|---|
| **A. Polling** | C++ trzyma `QTimer` (50 ms) i woła `job_state(id)`; Rust trzyma `AtomicU8`/`AtomicU32` per job | ✅ **Rekomendacja na v1** |
| B. Callback | Rust woła `fn(u64, f32, *const c_char)`; C++ robi `QMetaObject::invokeMethod(obj, ..., Qt::QueuedConnection)` | ⚠️ szybsze, ale trzeba pilnować życia obiektu C++ i wątku |

Polling wygrywa na starcie, bo **całkowicie eliminuje wywołania z wątku Rusta do Qt** — nie ma współdzielonego obiektu C++, nie ma `QueuedConnection`, nie ma ryzyka use-after-free, gdy użytkownik zamknie okno w trakcie syntezy. Koszt: 20 wybudzeń/s, pomijalny.

### 6.4 Blokujące wywołania wewnątrz runtime'u

`grep std::process::Command` w `core`: **11 plików** (m.in. `audio.rs` 4×, `auto_director.rs` 6×, `kokoro.rs` 2×, `voicelab.rs` 2×). To wywołania **synchroniczne**.

Szczególnie istotne: `PiperBridge::spawn_worker_for` (w `audio.rs`) używa `std::process::Command` i **blokującego `read_line`** na stdout dziecka, czekając na linię gotowości. Wywołane bezpośrednio w zadaniu async **zablokuje wątek roboczy runtime'u** — przy `worker_threads = 4` to 25% pula na czas startu procesu Pythona (setki ms).

**Rekomendacja:** w bridge każdą operację startu workera opakować w `tokio::task::spawn_blocking`. Bez zmian w `core` — wystarczy, że bridge wywoła to z właściwego kontekstu. (Docelowo `core` powinien sam używać `tokio::process`, ale to zmiana w legacy — poza zakresem M2.)

### 6.5 Pozostałe detale

- **stderr dzieci**: `Stdio::inherit()` w GUI bez konsoli trafia donikąd — bezpieczne, ale warto przekierować do pliku logu, żeby uniknąć blokady na pełnym pipe.
- **Sygnały**: Qt/HIG nie instaluje handlerów SIGINT w GUI; Tokio też nie — bez konfliktu.
- **Wątki**: `runtime` tworzy własne wątki; QML/Qt nie może ich używać do niczego. Zero `QObject` tworzonych z Rusta.

---

## 7. Rekomendowana struktura katalogów

```
Audioticon-legacy/                      # rdzeń — BEZ ZMIAN
├── Cargo.toml                          # + "crates/bridge" w members
└── crates/
    ├── core/                           # nietknięte (oracle)
    ├── cli/                            # nietknięte
    └── bridge/                         # NOWY — jedyne miejsce z FFI
        ├── Cargo.toml                  # crate-type = ["staticlib"]
        ├── build.rs                    # cxx_build::bridge("src/lib.rs")
        └── src/
            ├── lib.rs                  # #[cxx::bridge] mod ffi { … }
            ├── engine.rs               # Runtime + rejestr jobów
            ├── jobs.rs                 # id, stan, progres, anulowanie
            └── convert.rs              # SharedStruct ↔ typy core

AUDIOTICON 2.0/ui/                      # powłoka Qt (worktree M2)
├── agent-console.pro                   # + include(src/audioticon/audioticon.pri)
└── src/
    ├── core/                           # E0–E1: Paths, DirectiveParser, PolishVerbalizer
    │   └── core.pri
    └── audioticon/                     # NOWY — strona C++ kontraktu
        ├── audioticon.pri              # LIBS += … -laudioticon_bridge -lpthread -ldl -lm
        ├── Engine.h / Engine.cpp       # QObject: init(), synthesize(), sygnały
        ├── Voices.h / Voices.cpp       # QAbstractListModel z katalogu głosów
        └── Player.h / Player.cpp       # QAudioSink, PCM 44,1 kHz mono
```

### 7.1 Zasady nazewnictwa

- Crate: `audioticon-bridge`; biblioteka: `audioticon_bridge`; nagłówek wygenerowany: `audioticon_bridge/src/lib.rs.h`.
- C++: przestrzeń nazw `audioticon::ffi` (surowe typy z `cxx`) vs `audioticon::ui` (fasada `QObject`). QML **nigdy** nie widzi typów z `cxx` — tylko `QObject` z `Q_PROPERTY`/sygnałami.
- `src/core/core.pri` i `src/audioticon/audioticon.pri` są **rodzeństwem**: `core` to czysty C++ bez zależności od Rusta (i tak musi zostać — testy E1 działają bez toolchainu Rusta), `audioticon` to warstwa, która Rusta wymaga.

### 7.2 Dlaczego `src/audioticon/` a nie wrzucenie do `src/core/`

Testy E1 (`tests/tests.cpp`, 17/17) linkują wyłącznie `core.pri`. Dodanie FFI do `core.pri` sprawiłoby, że **każdy** build testów wymaga skompilowanego Rusta — a to złamałoby zasadę „testy parsera działają bez modeli i bez toolchainu Rusta". Rozdzielenie `.pri` utrzymuje ten rozdział.

---

## 8. Minimalna kolejność implementacji bez pobierania modeli

Kluczowe: **6 z 7 kroków nie wymaga ani jednego modelu**. Model (`pl_PL-bass-high`, 114 MB) pojawia się dopiero w kroku 6.

| # | Krok | Wymaga modelu? | Co dowodzi |
|---|---|:--:|---|
| **1** | **Szkielet `crates/bridge` + `build.rs` + jedna funkcja** `core_sample_rate() -> u32` (zwraca `DEFAULT_SAMPLE_RATE` = 44100) | ❌ | Że `staticlib` + `cxx` + linkowanie z qmake **w ogóle działa**. To najtańszy możliwy test najdroższego ryzyka (integracja buildów). Jeśli to padnie, reszta nie ma sensu. |
| **2** | **`Engine` + runtime Tokio + rejestr jobów** z atrapą syntezatora, która woła `create_silence_wav(out, 200 ms, 44100)` | ❌ | Całą granicę Tokio↔Qt: natychmiastowy zwrot `job_id`, polling `job_state`, anulowanie, brak zamrożenia UI. `create_silence_wav` już istnieje i nie potrzebuje modeli. |
| **3** | **`voice_catalog()`** → JSON z `models.rs` + `model_hub.rs` (id, język, licencja, pochodzenie, czy zainstalowany) + `QAbstractListModel` w QML | ❌ | Że dane katalogowe płyną przez FFI i renderują się w liście modułów. Czyste dane, zero inferencji. |
| **4** | **`probe_ort()` / `audioticon doctor`** → ścieżka ORT, wersja, `GetApi(31)` OK/NULL, obecność modeli, `ffmpeg`, głosy | ❌ | Rozstrzyga §4 **end-to-end**: pokaże, że z `ORT_DYLIB_PATH` na 1.32.0 backend jest gotowy, a z `/opt/superwhisper` — nie. |
| **5** | **`assemble()`** (`stitch_wav_files`) + **`QAudioSink`** odtwarzanie: sklej 3× cisza/oddech (`create_breath_wav`) i odtwórz | ❌ | Pełny łańcuch „wynik → plik → głośnik" **bez TTS**. Odsłuch może ocenić człowiek. |
| **6** | **Realna synteza `PiperBridge`** — pierwsze zdanie przez `pl_PL-bass-high` | ✅ 114 MB | Jakość i czas syntezy; pierwszy moment, w którym potrzebny jest głos. |
| **7** | **Cache zdań** (`compute_segment_hash`) + powtórka | ❌ (na kroku 5/6) | Trafienie cache < 50 ms, klucz `v1\|voice\|speed\|dsp\|rate\|text`. |

### 8.1 Uzasadnienie kolejności

- **Krok 1 przed wszystkim** — bo integracja buildów (qmake + cargo + `cxx` + linker) jest jedynym ryzykiem, które może unieważnić całą architekturę. Testowanie jej jedną funkcją zwracającą `44100` kosztuje godzinę; odkrycie problemu po napisaniu `Engine` kosztuje dni.
- **Krok 2 przed 3** — bo asynchroniczność jest trudniejsza niż dane. Katalog głosów można dorobić w dowolnym momencie; przeciek wątku albo zamrożenie UI trzeba złapać wcześnie.
- **Krok 5 przed 6** — bo pozwala zweryfikować odtwarzanie i składanie WAV **bez** czekania na model i bez oceny jakości głosu. Odsłuch ciszy i oddechu jest brzydki, ale diagnostycznie wystarczający.
- **Krok 4 wcześnie** — bo jeśli ORT okaże się nie do uratowania, lepiej wiedzieć przed krokiem 6 niż po pobraniu 114 MB.

### 8.2 Czego w tej kolejności świadomie nie ma

- **Nie ma mostkowania parsera** — C++ już go ma z goldenem (§3.4).
- **Nie ma DSP przez FFI** — `build_ffmpeg_filter_chain` i presety są w `core`; bridge je wywoła, ale natywne DSP w C++ to temat późniejszy (i zgodny z decyzją „DSP natywny C++", ale nie na tym etapie).
- **Nie ma LLM/chat** — poza pionowym wycinkiem.
- **Nie ma Kokoro** — wymaga `espeak-ng` (brak) i modelu 88 MB; Piper jest pierwszą ścieżką.

---

## 9. Rejestr ryzyk

| ID | Ryzyko | Waga | Mitygacja |
|---|---|:--:|---|
| **BR-01** | `cxx-build` generuje glue do katalogu z hashem → `.pro` nie potrafi go znaleźć | 🔴 wysoka | Stały `CARGO_TARGET_DIR` + jawna ścieżka w `.pri`, albo `QMAKE_EXTRA_TARGETS` z `cargo build` przed linkiem. Zaprojektować w kroku 1. |
| **BR-02** | `block_on` na wątku GUI zamraża UI | 🔴 wysoka | Kontrakt: funkcje FFI zwracają `job_id` natychmiast; zero `block_on` w ścieżce QML |
| **BR-03** | `OnceLock` cache'uje nieudaną inicjalizację ORT na całe życie procesu | 🟠 średnia | `probe_ort()` raportuje „wynik z cache"; docelowo reset/restart |
| **BR-04** | `api-31` wymaga ORT ≥ 1.31, a `/opt/superwhisper` to 1.24.2 | 🟠 średnia | `ORT_DYLIB_PATH` → 1.32.0 (potwierdzone sondą); produkcyjnie vendorować bibliotekę |
| **BR-05** | Zależność od cudzych venvów (`.hermes`, `piper-venv`, cache `uv`) | 🟠 średnia | Skopiować bibliotekę do `ui/runtime/` (gitignore) przed pierwszym buildem produkcyjnym |
| **BR-06** | Blokujące `std::process::Command` (+ `read_line`) w zadaniu async | 🟠 średnia | `spawn_blocking` w bridge przy starcie workera Piper/Kokoro |
| **BR-07** | Dwie kopie ASTRA; E0–E1 niezacommitowane w jednej | 🟠 średnia | Pracować **tylko** w `AUDIOTICON 2.0/ui` (worktree z `src/core`, 17/17 testów wg `M2_UI_WORKTREE.md`); nie ruszać `.openclaw/workspace/ASTRA-UI-Template` |
| **BR-08** | Panika Rusta na granicy FFI kończy proces | 🟡 niska | Wszystkie funkcje mostu zwracają status; `catch_unwind` tam, gdzie sensowne |
| **BR-09** | `cargo test --workspace` czerwony na FLAC (ffmpeg 9 + `loudnorm` bez `-ar`) | 🟡 niska | Znane z M0; nie jest bramką bridge'a, ale **nie może** być bramką CI |
| **BR-10** | Cross-language LTO / `prefer-dynamic` | 🟡 niska | Nie włączać — `g++` ≠ LLVM; `staticlib` ma być samowystarczalny |
| **BR-11** | Rozjazd parsera, jeśli ktoś wystawi `EmojiDirector` przez FFI | 🟡 niska | Zapisana zasada: tekst→dyrektywy wyłącznie w C++ (§3.4) |

---

## 10. Weryfikacja „nie zmieniałem kodu"

Sprawdzone po zakończeniu audytu (`git status --porcelain` w każdym repo):

- `Audioticon-legacy` — **0 linii** zmian,
- `ASTRA-UI-Template` — **0 linii** zmian,
- `AUDIOTICON 2.0` — jedyny nowy plik to ten raport.

Artefakt pomocniczy poza repo: `/tmp/ort-probe/{probe.c,probe}` (program sondujący API biblioteki ORT). Nie jest częścią projektu i można go usunąć.

---

## 11. Czego **nie** zweryfikowałem (ograniczenia)

- **Nie dodałem `cxx` do manifests** ani nie uruchomiłem `cargo build` bridge'a — polecenie zabraniało zmian w kodzie. Wniosek o wykonalności opiera się na: braku `cxx` w `Cargo.lock`, spełnionym MSRV, istnieniu `build.rs`-a jako mechanizmu oraz na tym, że `cargo search cxx` potwierdził osiągalność registry. **Pierwszy realny `cargo build` bridge'a pozostaje do wykonania w kroku 1.**
- **Nie skompilowałem `staticlib` ani nie zlinkowałem go z qmake** — to właśnie krok 1 planu. Twierdzenie „się da" jest oparte na standardowym mechanizmie `crate-type = ["staticlib"]`, nie na wykonanym linkowaniu.
- **Nie uruchomiłem syntezy** — brak modeli. Nie oceniam jakości ani czasu Piper.
- **Nie sprawdziłem Kokoro** — brak `espeak-ng` i modelu; ścieżka ORT dla Kokoro jest potencjalnie otwarta po ustawieniu `ORT_DYLIB_PATH`, ale fonemizacja i tak padnie bez `espeak-ng`.
- **Sonda ORT sprawdza dostępność API, nie poprawność inferencji.** To, że `GetApi(31)` zwraca wskaźnik, dowodzi zgodności ABI, nie tego, że sesja ONNX załaduje konkretny model.
- **Nie testowałem zachowania `Runtime::drop` w kontekście Qt** — to ryzyko teoretyczne, opisane na podstawie semantyki Tokio, nie zaobserwowane.

---

## 12. Reprodukcja

```bash
# 1. Wersje toolchainu
rustc --version; cargo --version; g++ --version | head -1
qmake6 --version | head -3; pkg-config --modversion Qt6Multimedia

# 2. Czy cxx już jest w drzewie (oczekiwane: brak trafień)
grep -n cxx ~/.openclaw/workspace/Audioticon-legacy/Cargo.lock || echo "brak cxx — brak konfliktu"
grep -rn "crate-type" ~/.openclaw/workspace/Audioticon-legacy/crates/*/Cargo.toml || echo "brak crate-type"

# 3. ONNX Runtime — wersja i dostępne API
ls -l /opt/superwhisper/lib/libonnxruntime.so*
echo "ORT_DYLIB_PATH=[$ORT_DYLIB_PATH]"
# sonda (źródło w /tmp/ort-probe/probe.c):
gcc -O0 -o /tmp/ort-probe/probe /tmp/ort-probe/probe.c -ldl
/tmp/ort-probe/probe /opt/superwhisper/lib/libonnxruntime.so          # 1.24.2, GetApi(31)=NULL
/tmp/ort-probe/probe ~/Portable-Local-Studio/app/tts-runtime/piper-venv/lib/python3.12/site-packages/onnxruntime/capi/libonnxruntime.so.1.32.0   # 1.32.0, GetApi(31)=OK

# 4. Async w rdzeniu
grep -n "async_trait" ~/.openclaw/workspace/Audioticon-legacy/crates/core/src/*.rs
grep -c "std::process::Command" ~/.openclaw/workspace/Audioticon-legacy/crates/core/src/*.rs | grep -v ":0"
```

---

## 13. Rekomendacja końcowa

**Integracja przez `cxx::bridge` jest wykonalna i bezpieczna dla legacy, pod trzema warunkami:**

1. **Bridge jako osobny crate `crates/bridge`** z `crate-type = ["staticlib"]` — `core` i `cli` pozostają nietknięte, a własność „legacy jako oracle" zostaje zachowana.
2. **Granica FFI przebiega na segmencie, nie na tekście** — parsowanie i verbalizacja zostają w C++ (E1 z goldenem), Rust dostaje gotowe `text/rate/dsp/pause`.
3. **Async zostaje w Rust, a UI nigdy nie blokuje** — jeden runtime w tle, `job_id` zwracany natychmiast, wyniki przez polling (v1) lub `QueuedConnection` (później).

Pierwszy krok to **jedna funkcja zwracająca `44100`** przeprowadzona przez cały łańcuch `cargo → .a → qmake → binarka`. Jeśli ten jeden test przejdzie, reszta planu z §8 jest rutynowa. Jeśli nie przejdzie, lepiej wiedzieć teraz niż po napisaniu `Engine`.

Korekta najważniejszego wcześniejszego ustalenia: **problem ORT to nie brak pliku, lecz niezgodność wersji API (`api-31` vs 1.24.2)** — i jest już rozwiązywalny bez zmian w kodzie, przez `ORT_DYLIB_PATH` wskazujący na obecną w systemie bibliotekę 1.32.0.
