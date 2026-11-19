# M4 — Minimalny dowód linkowania Rust staticlib z Qt

**Data:** 2026-11-18
**Zakres:** jedna funkcja `core_version() -> u32` (= 44100) przeprowadzona przez
`Rust/cxx::bridge → staticlib → qmake6 → binarka Qt → wywołanie z C++`.
**Zasady:** praca wyłącznie w `AUDIOTICON 2.0/backend` i `AUDIOTICON 2.0/ui`. Zero zmian w
`Audioticon-legacy` i `ASTRA-UI-Template`. Bez modeli, bez konfiguracji ORT. Bez commitów.

---

## 0. Wynik (TL;DR)

| Wymaganie | Stan | Dowód |
|---|:--:|---|
| 1. Nowy crate `crates/bridge` | ✅ | `backend/crates/bridge/` (istniał z M3, domknięty w M4) |
| 2. `cxx-build` + `crate-type = ["staticlib", …]` | ✅ | `Cargo.toml`, `build.rs`; `libaudioticon_bridge.a` = **207 MB** |
| 3. Minimalny adapter C++ po stronie UI | ✅ | `ui/src/bridge/AudioticonCore.{h,cpp}` |
| 4. Test **i** komenda diagnostyczna na 44100 | ✅ | `Tests::ffiLinksRustCore` + `astra-agent-console --probe-ffi` |
| 5. Stały `CARGO_TARGET_DIR` dla glue cxx | ✅ | `.cargo/config.toml` + publikacja do `backend/target/cxxbridge` |
| 6. Bez modeli, bez ORT | ✅ | brak `*.onnx`, brak `ORT_DYLIB_PATH` |
| 7. `cargo check`/`test` + build Qt | ✅ | check 0, test 6/6, qmake+make 0, testy Qt 18/18 |
| 8. Zachowane 17 testów Qt | ✅ | 17 dotychczasowych + 1 nowy = **18 passed, 0 failed** |

**Dwa zdania:** mostek działa i jest odtwarzalny z czystego stanu — po `cargo clean -p` i
usunięciu `target/cxxbridge` cały łańcuch odbudowuje się jedną komendą `qmake6`.
Po drodze naprawiono **trzy realne defekty**, z których jeden (M3) sprawiał, że crate w ogóle
nie kompilował się z czystego drzewa.

---

## 1. Co powstało / co zmieniono

### 1.1 Rust — `backend/`

| Plik | Zmiana |
|---|---|
| `crates/bridge/src/lib.rs` | **+ `core_version() -> u32`** w `extern "Rust"`, implementacja czytająca `audioticon_core::DEFAULT_SAMPLE_RATE`, **+ test** |
| `crates/bridge/build.rs` | przepisany: publikacja nagłówków cxx do stałej ścieżki, odporna na symlinki i brak katalogów |
| `crates/bridge/src/bridge.cc` | **naprawiony include**: `ffi.rs.h` → `lib.rs.h` (patrz §4.1) |
| `.cargo/config.toml` | **nowy** — pinuje `target-dir = target` |

`core_version()` **nie powtarza literału 44100**: czyta stałą z `audioticon-core`. Dzięki temu
przypadkowy stub nie może dać fałszywego zielonego — wywołanie musi realnie trafić w kod rdzenia.

### 1.2 Qt — `ui/`

| Plik | Zmiana |
|---|---|
| `src/bridge/AudioticonCore.h` | **nowy** — jedyne miejsce w UI, które wie o istnieniu Rusta |
| `src/bridge/AudioticonCore.cpp` | **nowy** — `#include "audioticon-bridge/src/lib.rs.h"` + wywołanie `audioticon::bridge::core_version()` |
| `src/bridge/rustlink.pri` | **nowy** — linkowanie staticlibu + include path + samo-naprawa |
| `bridge.pri` | dołącza `rustlink.pri` (aplikacja linkuje Rusta) |
| `tests/tests.pro` | dołącza `rustlink.pri` (testy linkują Rusta) |
| `tests/tests.cpp` | **+ `ffiLinksRustCore()`** |
| `src/agent/main.cpp` | **+ `--probe-ffi`** — diagnostyka bez wyświetlacza |

Podział `.pri` jest celowy: `rustlink.pri` (samo linkowanie) jest osobne od `bridge.pri`
(fasady `QObject`). Dzięki temu testy Qt dostają link do Rusta, ale **nie** ciągną
`StudioBridge`/`BridgeObserver`.

---

## 2. Wyniki — wszystkie komendy i kody wyjścia

### 2.1 Rust

```
backend$ cargo build -p audioticon-bridge            → EXIT 0
  warning: cxx headers (2 files) published ... to backend/target/cxxbridge
  Finished `dev` profile in 0.98s

backend$ cargo test -p audioticon-bridge             → EXIT 0
  test result: ok. 6 passed; 0 failed
    test_core_version_reports_core_sample_rate ... ok
    (5 pozostałych z M3)

backend$ cargo check --workspace                     → EXIT 0
  0 errors, 0 ostrzeżeń „unused field in workspace.package"
```

### 2.2 Qt — aplikacja

```
ui$ mkdir build-console && cd build-console
build-console$ qmake6 ../agent-console.pro           → EXIT 0
build-console$ make -j$(nproc)                       → EXIT 0
  brak „undefined reference", brak błędów kompilacji
```

### 2.3 Dowód runtime — komenda diagnostyczna

```
$ ./astra-agent-console --probe-ffi          # bez DISPLAY, bez QML
audioticon core sample rate: 44100 Hz
EXIT 0
```

Kod wyjścia jest częścią kontraktu: `0` = 44100, `3` = inna wartość. Nadaje się do CI.

Potwierdzenie, że w binarce jest **kod Rusta, nie atrapa C++**:

```
$ nm -C astra-agent-console | grep -c core_version
6
audioticon$bridge$cxxbridge1$205$core_version          ← glue cxx
audioticon_bridge::core_version                        ← funkcja Rusta
cxx::unwind::prevent_unwind::<…::__core_version, u32>  ← runtime cxx (panic guard)

$ nm -C astra-agent-console | grep -c "rust_begin_unwind\|std::rt"
9                                                       ← Rust std w binarce
```

### 2.4 Qt — testy

```
build-tests$ qmake6 ../tests/tests.pro               → EXIT 0
build-tests$ make -j$(nproc)                         → EXIT 0
build-tests$ QT_QPA_PLATFORM=offscreen ./tests       → EXIT 0

Totals: 18 passed, 0 failed, 0 skipped, 0 blacklisted, 4097ms
```

**17 dotychczasowych testów zachowanych i zielonych**, w tej samej kolejności; nowy
`ffiLinksRustCore` wchodzi jako drugi (po `initTestCase`):

```
PASS: initTestCase            PASS: ffiLinksRustCore  ← NOWY
PASS: catalogAndDefaults      PASS: toggleMoveReset
PASS: chartsAxes              PASS: historyIsBounded
PASS: unavailableGreysOut     PASS: persistence
PASS: stripRoundTrip          PASS: languageSwitch
PASS: rendersAndResponds      PASS: directiveGolden
PASS: directivePaletteMatchesDonor
PASS: agentTurnStreamsAndShowsTools
PASS: agentStopHaltsTheStream PASS: agentSessionsKeepTheirOwnTranscript
PASS: agentConsoleRenders     PASS: cleanupTestCase
```

### 2.5 Regresja UI

```
$ QT_QPA_PLATFORM=offscreen ./astra-agent-console --screenshot /tmp/m4-final.png
EXIT 0, screenshot 158 481 B   ← interfejs nadal się rysuje
```

---

## 3. Artefakty — dokładne ścieżki

| Artefakt | Ścieżka | Rozmiar |
|---|---|---|
| Rust staticlib | `AUDIOTICON 2.0/backend/target/debug/libaudioticon_bridge.a` | **207 MB** |
| Nagłówki cxx (stała ścieżka) | `AUDIOTICON 2.0/backend/target/cxxbridge/audioticon-bridge/src/lib.rs.h` | 29 KB |
| Binarka aplikacji | `AUDIOTICON 2.0/ui/build-console/astra-agent-console` | **87 MB** |
| Binarka testów | `AUDIOTICON 2.0/ui/build-tests/tests` | **87 MB** |
| Screenshot regresyjny | `/tmp/m4-final.png` | 158 481 B |
| Logi buildów | `/tmp/m4-*.log` | — |

⚠️ **207 MB staticlib / 87 MB binarka to skutek profilu `debug`.** Rust składuje tam
symbolami debugowymi cały rdzeń (tokio, ort, zip, ndarray…). Dla porównania binarka Qt bez
Rusta miała ~2 MB. To nie problem M4, ale przed jakimkolwiek dystrybuowaniem trzeba przejść na
`--release` (+ `strip`); realnie zejdzie do kilku–kilkunastu MB.

---

## 4. Trzy defekty znalezione i naprawione

M4 nie było „dopisać jedną funkcję". Zastany szkielet z M3 **nie budował się z czystego
drzewa**. Poniżej, bo każdy z nich groził cichym zepsuciem.

### 4.1 `bridge.cc` includował nagłówek, którego nie ma (blokujący)

```cpp
#include "audioticon-bridge/src/ffi.rs.h"   // ← nie istnieje po clean build
```

`cxx-build` generuje dla tego crate'a **jeden** nagłówek- parasol: `lib.rs.h`.
`ffi.rs.h` leżał w `target/` tylko jako pozostałość wcześniejszej, ręcznej kopii.

Skutek: po `cargo clean -p audioticon-bridge` build padał na
`fatal error: audioticon-bridge/src/ffi.rs.h: No such file or directory`.
Wcześniejsze „działa" było artefaktem zabrudzonego `target/` — **fałszywy zielony**.

Poprawka: `#include "audioticon-bridge/src/lib.rs.h"` (z komentarzem ostrzegawczym).

### 4.2 `fs::copy` pisał „przez" symlinki i nie tworzył katalogów

Pierwsza wersja publikacji glue:

- katalog docelowy zawierał **symlinki** (`lib.rs.h → ../debug/build/<hash>/…`); `fs::copy`
  rozwiązuje symlink i nadpisuje **cel**, a nie link → publikowany był stary nagłówek, bez
  `core_version`;
- `copy_tree` nie wołał `create_dir_all`, więc przy czystym `target/` kopiowanie kończyło się
  `copied == 0` i panic.

Poprawka: katalog docelowy jest usuwany i odtwarzany od zera (`remove_dir_all`), `copy_tree`
tworzy katalogi, a kopiowane są **wyłącznie pliki `.h`**.

### 4.3 `cargo` nie powtarza `build.rs`, gdy zniknie tylko opublikowany glue

Po usunięciu `target/cxxbridge` samo `cargo build -p audioticon-bridge` kończy się w **0.04 s**
i **nie odtwarza** nagłówka — cargo uznaje crate za aktualny i co gorsza **odtwarza z cache
stare ostrzeżenie** „cxx headers published…". Wygląda jak sukces, a katalogu nie ma.

Poprawka dwutorowa:
- `build.rs` publikuje do ścieżki liczonej z `CARGO_MANIFEST_DIR` (a nie z `OUT_DIR`), więc
  wynik jest niezależny od `CARGO_TARGET_DIR`;
- `rustlink.pri` **samo-naprawia**: gdy brak nagłówka, woła
  `cargo clean -p audioticon-bridge && cargo build -p audioticon-bridge` (samo `build` by nie
  wystarczyło — patrz wyżej), a jeśli to nie pomoże, przerywa z czytelnym `error()`.

Zweryfikowane: usunięcie `target/cxxbridge` → `qmake6` wypisuje
`rustlink: brak opublikowanego glue cxx - przebudowuje audioticon-bridge`, glue wraca,
`make` linkuje, `--probe-ffi` → 44100.

### 4.4 Drobiazg: qmake czyta `$$VAR.` jako nazwę zmiennej

`error("… w $$AUDIOTICON_RUST_LIBDIR. Uruchom…")` wypisywało **pustą** ścieżkę, bo qmake
skleja `AUDIOTICON_RUST_LIBDIR.` w jedną nazwę zmiennej. Wszystkie komunikaty używają teraz
`$${VAR}`.

---

## 5. Jak to jest zbudowane (dlaczego tak, a nie inaczej)

### 5.1 Glue jest w staticlibie — Qt tylko includuje nagłówek

`cxx_build::bridge(...).compile(...)` kompiluje **wygenerowany `lib.rs.cc`** oraz nasz
`src/bridge.cc` do biblioteki Rusta. Dlatego:

- Qt **nie** kompiluje glue ponownie — zrobiłoby to duplikaty symboli;
- Qt potrzebuje wyłącznie **nagłówka** (`lib.rs.h`) i samego `.a`;
- `rustlink.pri` dodaje `-laudioticon_bridge -lpthread -ldl -lm` (`-ldl` przyda się później dla
  `ort` z `load-dynamic`).

### 5.2 Kolejność include-path ma znaczenie

Nagłówek cxx zaczyna się od `#include "BridgeObserver.h"` — to typ z crate'a backendu.
W UI istnieje **inna** klasa o tej samej nazwie (`ui/src/bridge/BridgeObserver.h`, `QObject`
w globalnej przestrzeni). Dlatego `rustlink.pri` podaje
`backend/crates/bridge/include` **przed** `ui/src/bridge`. Bez tego kompilator podstawiał
qt-owy `BridgeObserver` i nagłówek się nie kompilował (dokładnie taki błąd wystąpił w trakcie).

### 5.3 Jedno miejsce styku z Rustem

`AudioticonCore.h` nie zawiera niczego z `cxx`. Typy generowane przez cxx nie wyciekają do
QML, do innych nagłówków ani do harnessu testowego. Gdy dojdzie asynchroniczność (M5),
wymiana `int coreSampleRate()` na `startRender()/pollJob()` dotknie **jednego** pliku `.cpp`.

---

## 6. Odtwarzanie od zera

```bash
# 1. Rust
cd "/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/backend"
cargo build -p audioticon-bridge        # publikuje glue do target/cxxbridge
cargo test  -p audioticon-bridge        # 6/6
cargo check --workspace                 # 0 błędów

# 2. Aplikacja Qt
cd "../ui" && mkdir -p build-console && cd build-console
qmake6 ../agent-console.pro && make -j"$(nproc)"
./astra-agent-console --probe-ffi       # → "audioticon core sample rate: 44100 Hz"

# 3. Testy Qt
cd .. && mkdir -p build-tests && cd build-tests
qmake6 ../tests/tests.pro && make -j"$(nproc)"
QT_QPA_PLATFORM=offscreen ./tests       # → 18 passed, 0 failed

# 4. Test samo-naprawy (opcjonalny)
rm -rf ../../backend/target/cxxbridge
qmake6 ../agent-console.pro             # sam odbuduje glue
```

---

## 7. Czego M4 świadomie **nie** obejmuje

- **Zero modeli, zero ORT** — zgodnie z poleceniem. `core_version()` nie dotyka inferencji;
  `ort` jest w grafie zależności, ale z `load-dynamic` nie wymaga biblioteki w czasie linkowania
  (potwierdzone: link Qt przeszedł bez `ORT_DYLIB_PATH`).
- **Brak asynchroniczności** — żadnego `tokio::Runtime`, jobów, callbacków. To M5; ryzyko
  opisane w `M2_TOOLCHAIN_FFI_AUDIT.md` §6 pozostaje nietknięte.
- **`StudioBridge` nadal jest makietą** (QTimer + `parseDirectedText` z E1). Nie podłączałem go
  do Rusta — to nie było potrzebne do dowodu linkowania, a zmiana makiety bez testów
  asynchroniczności byłaby pozorowana.
- **Profil `debug`** — binarki 87 MB. Release/strip to osobna decyzja.
- **Brak CI** — `cargo test --workspace` w backendzie nadal zawiera znany czerwony test FLAC
  (ffmpeg 9 + `loudnorm` bez `-ar`, patrz `M0_BUILD_TEST_REPORT.md` §2.2). Dlatego bramką M4
  jest `cargo test -p audioticon-bridge` + `cargo check --workspace`, nie cały workspace.

---

## 8. Ograniczenia tego raportu

- Nie uruchamiałem `cargo build --release` ani nie mierzyłem wpływu staticlibu na czas
  linkowania w release.
- Nie testowałem zachowania `--probe-ffi` na maszynie bez Qt platform plugin — działa
  `QT_QPA_PLATFORM=offscreen` i działa bez `DISPLAY`, bo ścieżka diagnostyczna wychodzi przed
  `QGuiApplication`.
- Nie weryfikowałem, czy `BridgeObserver`/`parse_directed_text` z M3 są funkcjonalnie poprawne —
  sprawdziłem tylko, że **kompilują się i linkują**. Ich logika (mapowanie dyrektyw, hash cache)
  wymaga osobnego przeglądu w M5.
- Sprawdziłem `cargo check --workspace`, nie `cargo test --workspace` (powód w §7).

---

## 9. Następny krok (propozycja, nie wykonane)

Zgodnie z kolejnością z audytu M2 krok 2: **`Engine` + runtime Tokio + rejestr jobów**, z
atrapą syntezatora wołającą `create_silence_wav(200 ms)`. Kontrakt:
`start_job() -> u64` (natychmiast), `job_state(id)` (polling z `QTimer` 50 ms), `cancel(id)`.
Bez modeli. Wtedy `AudioticonCore.cpp` — jedyny plik dotykający cxx — wymieni
`coreSampleRate()` na `startSynthesis()/pollJob()`.
