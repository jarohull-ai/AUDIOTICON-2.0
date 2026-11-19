# M12 — Lokalny release bundle Audioticon 2.0 (Linux/Omarchy)

**Data:** 18 listopada 2026
**Status:** Zbudowany i zweryfikowany lokalnie — bez AppImage, bez bundlowania GPL-FFmpeg, Qt ani modeli.
**Ścieżka bundle:** `/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/release/audioticon-local-linux/`
**Rozmiar bundle:** 19 392 684 B = **18,5 MiB** (19 MB wg `du`)

Praca wykonana wyłącznie w `backend/` i `ui/`; staging powstał w `AUDIOTICON 2.0/release/`. Nie zmieniano `Audioticon-legacy`, `ASTRA-UI-Template` ani `~/Downloads`. Nic nie zostało zacommitowane.

---

## 1. Cel i zakres

Celem było przygotowanie **działającego lokalnego bundle release**, który:

- uruchamia się z miejsca, w którym leży (relokowalny),
- korzysta z bibliotek systemowych zamiast je dublować,
- nie zawiera: modeli, ONNX Runtime, FFmpeg, venv Pythona, `target/`, `build/`,
- daje użytkownikowi jednoznaczny komunikat, gdy brakuje zależności,
- ma manifest z wersją, stanem źródeł, sumami SHA-256 i wymaganiami.

---

## 2. Wynik w skrócie

| Element | Wynik |
|---|---|
| `cargo build --release --locked --workspace` | **OK**, 8,72 s (inkrementalnie; profil release zweryfikowany od zera w M10) |
| `qmake6 CONFIG+=release` + `make -j12` | **OK**, 0 błędów, binarka linkuje **release** staticlib Rust (B3 działa) |
| Bundle | **19 MB** zamiast ~140 MB z artefaktami budowy |
| `bin/audioticon` po strip | **10 529 584 B (10,0 MiB)**, −20,0% |
| `bin/audioticon-studio` po strip | **8 824 600 B (8,4 MiB)**, −46,4% |
| `scripts/doctor.sh` | 0 błędów / 0 ostrzeżeń w środowisku z modelem; 0 błędów / 3 ostrzeżenia bez modelu |
| `run-studio.sh` | startuje natywne GUI Qt, `--probe-ffi` → `audioticon core sample rate: 44100 Hz` |
| Testy Rust | **179 / 179 passed, 0 failed** |
| Testy QtTest offscreen | **27 / 27 passed, 0 failed** |
| qmake po instalacji | **niewymagany** — dowiedzione pułapką w PATH (patrz §6.3) |

---

## 3. Build

### 3.1 Rust (release, `--locked`)

```bash
cd backend
cargo build --release --locked --workspace
# Finished `release` profile [optimized] target(s) in 8.72s
```

Artefakty:

- `backend/target/release/audioticon` — 13 153 992 B
- `backend/target/release/libaudioticon_bridge.a` — 47 628 998 B (45,4 MiB; **nie wchodzi** do bundle)

Build powtórzył preflight B2 z M11: `cxx headers (2 files, cxx.h verified valid 29550 B) published`.
Pozostałe ostrzeżenia to wyłącznie `-Wmaybe-uninitialized` w generowanym glue C++ cxx
(`rust::cxxbridge1::Vec<T>::Vec()`), znane z M10 — nie blokują release.

### 3.2 Qt (release, linkowanie release staticlib)

```bash
cd ui/build-release
qmake6 ../agent-console.pro CONFIG+=release
make -j$(nproc)
```

Wynik: `ui/build-release/astra-agent-console` — 16 453 104 B, 0 błędów.
Linia linkowania potwierdza profil release:

```
-L/home/jaro/.openclaw/workspace/AUDIOTICON\ 2.0/backend/target/release -laudioticon_bridge
```

To dowód, że poprawka B3 z M11 (`rustlink.pri` wybiera profil wg `CONFIG`) działa w praktyce:
binarka ma 16 MB, a nie 89 MB jak przy linkowaniu debugowym.

### 3.3 Środowisko budowy

| Składnik | Wersja |
|---|---|
| rustc / cargo | 1.100.0 (2026-11-09) / 1.100.0 |
| GCC | 16.2.1 20260810 |
| Qt | 6.12.0 |
| glibc | 2.44 |
| FFmpeg / ffprobe | n9.0.2 |
| System | Linux 7.2.15-1-omarchy x86_64, 12 rdzeni |

---

## 4. Struktura bundle

```
release/audioticon-local-linux/
├── bin/
│   ├── audioticon            10,0 MiB  (CLI, stripped)
│   └── audioticon-studio      8,4 MiB  (natywne GUI Qt, stripped)
├── scripts/
│   ├── doctor.sh              11 KiB   (9 sekcji kontroli, bez qmake)
│   └── run-studio.sh           3,3 KiB (launcher natywnej aplikacji)
├── share/
│   ├── LICENSE-MIT
│   ├── LICENSE-APACHE
│   ├── THIRD_PARTY_NOTICES.md
│   └── README-RELEASE.md      4,5 KiB  (instalacja, zależności, XDG, licencje)
└── manifest.json               4,3 KiB (wersja, źródła, SHA-256, wymagania)
```

### 4.1 Strip — pomiary

| Plik | Przed strip | Po strip | Redukcja |
|---|---:|---:|---:|
| `bin/audioticon` | 13 153 992 B (12,5 MiB) | **10 529 584 B (10,0 MiB)** | −19,95% |
| `bin/audioticon-studio` | 16 453 104 B (15,7 MiB) | **8 824 600 B (8,4 MiB)** | −46,36% |
| **razem `bin/`** | 29 607 096 B | **19 354 184 B** | −34,6% |

`strip --strip-unneeded` nie usunął symboli potrzebnych do działania: po strip
`--probe-ffi` nadal zwraca 44 100 Hz, a testy Qt na tym samym staticlibie przechodzą.

### 4.2 Sumy kontrolne (SHA-256)

```
41391747caaf6d7e89b9fb5b9da50d2718251963cacbedf686c342de50aa21b2  bin/audioticon
6c14acc2ae290bc434ac5c91f2446dea9dc668658ed11118c4f2643bacf25a26  bin/audioticon-studio
```

---

## 5. Manifest (`manifest.json`)

| Pole | Wartość |
|---|---|
| `version` / `channel` | `1.0.0` / `local-bundle` |
| `built_at` | 2026-11-18T20:30:42+00:00 |
| `built_on` | Linux 7.2.15-1-omarchy x86_64 / Omarchy |
| `host_glibc` | 2.44 |
| `artifact_policy` | `appimage=false`, `bundles_ffmpeg=false`, `bundles_qt=false`, `bundles_models=false`, `bundles_onnxruntime=false`, `bundles_python_venv=false`, `includes_build_output=false`, `stripped=true` |
| `source_state` | `git_head: null` — repozytorium git nie ma jeszcze HEAD, więc identyfikatorem jest **odcisk treści źródeł**: `80513c357c0492c85fc25dbc255e285e3856c78c05223dd7a63e184b629cde3f` |
| `binaries[]` | ścieżka, rola, SHA-256, rozmiar w bajtach i w MiB, `stripped`, `dynamic`, `links_rust_release_staticlib` |
| `toolchain` | rustc, cargo, Qt, GCC, FFmpeg oraz **dokładne komendy budowy** |
| `system_requirements` | glibc ≥ 2.44, Qt 6.12 (lista modułów), pluginy `xcb`/`wayland`, sesja Wayland/X11, audio PipeWire/ALSA, `qmake_required_after_install: false` |
| `dependencies[]` | 7 pozycji: Qt, FFmpeg, ffprobe, Python, piper-tts, model Piper, ORT — każda z `bundled: false`, `required`, pakietem Arch i licencją |
| `verification` | ścieżki doctor / probe + oczekiwane wyjście |

Zakres odcisku źródeł: `backend/crates/**/*.rs`, `Cargo.toml`, `Cargo.lock`,
`ui/src/**/*.{cpp,h,qml,qrc,json,pri}`, `ui/agent-console.pro`, `ui/bridge.pri`,
`ui/tests/tests.{cpp,pro}`.

---

## 6. `scripts/doctor.sh`

### 6.1 Kontrole (9 sekcji)

| # | Kontrola | Sposób | Bez tego |
|---|---|---|---|
| 1 | Qt 6 runtime | `ldd bin/audioticon-studio` → brak `not found`; wersja z rozwiązanej `libQt6Core.so.6` | **FAIL** — GUI nie wstanie |
| 1b | plugin platformy | szuka `platforms/libqxcb.so`/`libqwayland.so` w `QT_PLUGIN_PATH` i katalogach systemowych | **FAIL** — brak okna |
| 2 | ffmpeg | `command -v`, wersja + obecność filtra `loudnorm`, koderów FLAC i AAC | **FAIL** (brak ffmpeg) / WARN (brak kodera) |
| 3 | ffprobe | `command -v`, wersja | **FAIL** |
| 4 | Python 3 | `AUDIOTICON_PYTHON` → `VIRTUAL_ENV` → `$XDG_DATA_HOME/audioticon/venv` → `python3` | **FAIL** |
| 5 | piper-tts | `import piper` w wybranym interpreterze lub CLI `piper` | WARN — synteza wyłączona |
| 6 | model Piper | `AUDIOTICON_PIPER_MODEL` → `models/pl_PL-{bass-high,gosia-medium,darkman-medium}.onnx` → dowolny `*.onnx`; sprawdza też `.onnx.json` | WARN |
| 7 | ONNX Runtime | `ORT_DYLIB_PATH` → venv/lib → `/usr/lib`, `/usr/local/lib`, `/usr/lib/x86_64-linux-gnu`, `find -maxdepth 6` | WARN — Kokoro wyłączone |
| 8 | audio | `/dev/snd`, socket PipeWire, `PULSE_SERVER`/`pactl info` | WARN — odsłuch niemy |
| 9 | XDG | `mkdir -p` + realny zapis pliku próbnego w `data`, `outputs`, `models`, `cache` | **FAIL** |

Kody wyjścia: `0` = brak błędów, `1` = co najmniej jeden FAIL, `2` = `--strict` przy ostrzeżeniach, `64` = zły argument.
Doctor **nigdy** nie wywołuje qmake, nie kompiluje i nie pobiera niczego; o braku qmake informuje wprost w nagłówku sekcji 1.

### 6.2 Przebiegi

| Przebieg | Błędy | Ostrzeżenia | Exit |
|---|---:|---:|---:|
| Środowisko docelowe (`VIRTUAL_ENV=piper-venv`, `AUDIOTICON_PIPER_MODEL=pl_PL-bass-high.onnx`) | 0 | **0** | 0 |
| Gołe środowisko (bez modelu, venv, ORT) | 0 | 3 (piper-tts, model, ORT) | 0 |
| Ten sam goły + `--strict` | 0 | 3 | **2** (zgodnie z projektem) |

Przykład werdyktu w środowisku docelowym:

```
[7/9] ONNX Runtime (libonnxruntime.so)
  [ OK ] libonnxruntime: .../piper-venv/lib/python3.12/site-packages/onnxruntime/capi/libonnxruntime.so.1.32.0
Podsumowanie
  błędy: 0   ostrzeżenia: 0
  Wynik: GOTOWE — wszystkie zależności obecne.
```

### 6.3 Dowód „qmake nie jest wymagany”

Do `PATH` dopisano katalog z podrobionymi `qmake6` i `qmake`, które przy każdym
wywołaniu tworzą plik-znacznik i kończą się kodem 127:

```bash
PATH=/tmp/m12audit/qmake-shim:$PATH scripts/doctor.sh   # EXIT 0
# QMAKE_CALLED=NO
```

Doctor przeszedł w całości (włącznie z kontrolą Qt przez `ldd`) i **ani razu** nie
sięgnął po qmake. To jest bezpośredni dowód wymagania „qmake nie jest wymagany po instalacji”.

### 6.4 Dwie usterki doctora znalezione i naprawione podczas weryfikacji

1. **Fałszywe alarmy o `loudnorm` i FLAC.** Pierwsza wersja używała
   `ffmpeg -filters | grep -q …` przy `set -o pipefail`: `grep -q` kończy się po
   pierwszym trafieniu, ffmpeg dostaje SIGPIPE (141), a `pipefail` zamienia to na
   niepowodzenie całej rury → doctor ostrzegał o braku filtra i kodera, które
   w rzeczywistości są obecne. Naprawa: listy filtrów/koderów zbierane raz do
   zmiennych i sprawdzane przez here-string (`grep -q <<<"$lista"`), bez rury.
2. **Zbyt płytki skan ORT.** `find -maxdepth 4` nie docierał do
   `venv/lib/python3.12/site-packages/onnxruntime/capi/libonnxruntime.so.1.32.0`
   (5 poziomów), więc ORT w venv był niewykrywany. Podniesiono do `maxdepth 6`,
   żeby dopasować się do wyszukiwania w `audioticon-core` (`scan_for_libonnxruntime`).
   Ta sama poprawka trafiła do `run-studio.sh`.

Po obu poprawkach doctor w środowisku docelowym raportuje 0/0, a w gołym 3 oczekiwane ostrzeżenia.

---

## 7. `scripts/run-studio.sh` (launcher)

Zachowanie zgodne z wymaganiami:

- **ścieżki bundle** — `BUNDLE_DIR` liczony z `readlink -f "${BASH_SOURCE[0]}"`, więc bundle działa po przeniesieniu; `bin/` bundle trafia na początek `PATH`;
- **twarda brama** — brak binarki, nierozwiązane biblioteki (`ldd` → `not found`) lub brak pluginu platformy kończą się czytelnym komunikatem po polsku plus gotową komendą `pacman` i wskazaniem `scripts/doctor.sh`, kod `2`;
- **miękkie braki** — brak `ffmpeg`/`ffprobe` tylko ostrzega i nie blokuje startu (UI wstaje, eksport nie zadziała);
- **model i ORT** — jeśli `AUDIOTICON_PIPER_MODEL`/`ORT_DYLIB_PATH` nie są ustawione, launcher wybiera je z katalogów XDG/venv; brak modelu nie jest błędem (aplikacja pokaże baner „Brak modelu głosu” zgodnie z B13);
- **brak serwera HTTP** — launcher nie uruchamia żadnego serwera ani przeglądarki; kończy się `exec` na natywnej binarce Qt (zasoby QML są w `qrc`, więc nie ma katalogu z QML do serwowania);
- **przekazywanie argumentów** — `"$@"` trafia do aplikacji (`run-studio.sh --probe-ffi` działa).

### 7.1 Testy launcha

| Test | Oczekiwanie | Wynik |
|---|---|---|
| `run-studio.sh --probe-ffi` | `audioticon core sample rate: 44100 Hz`, exit 0 | **OK**, exit 0 |
| kopia bundle w `/tmp/m12audit/bundle-copy` → `--probe-ffi` | relokowalność, exit 0 | **OK**, exit 0 |
| kopia bez `bin/audioticon-studio` | komunikat „brak binarki … bundle jest niekompletny”, exit 2 | **OK**, exit 2 |
| start GUI `QT_QPA_PLATFORM=offscreen`, 5 s | proces żyje (timeout 124) | **OK**, exit 124 |
| log startu | brak błędów QML | tylko `Failed to open VDPAU backend libvdpau_nvidia.so` i `radv is not a conformant Vulkan implementation` — komunikaty sterownika GPU, nie aplikacji |

---

## 8. Czego bundle nie zawiera (potwierdzenie reguł)

| Kategoria | W bundle | Uzasadnienie |
|---|---|---|
| modele Piper | **nie** | własne licencje (CC0/CC-BY), ~60–110 MB na model |
| ONNX Runtime | **nie** | `ort` z `load-dynamic`; ładowany z venv/systemu |
| FFmpeg | **nie** | build systemowy jest GPL (`--enable-gpl --enable-version3`) |
| Qt | **nie** | LGPLv3, linkowanie dynamiczne do bibliotek systemowych |
| venv Pythona | **nie** | instalowany przez użytkownika (`piper-tts`) |
| `target/`, `build*/` | **nie** | bundle zawiera wyłącznie 9 plików z §4 |

Weryfikacja: `find . -type f` w bundle zwraca dokładnie 9 plików (2 binarki, 2 skrypty, 4 dokumenty, manifest).

---

## 9. Testy pełne (wymaganie 10)

### 9.1 Rust — `cargo test --workspace --no-fail-fast`

Środowisko: `VIRTUAL_ENV=…/piper-venv`, `AUDIOTICON_PIPER_MODEL=…/pl_PL-bass-high.onnx` (włącza testy realnego Pipera).

| Pakiet | Wynik |
|---|---|
| `audioticon-bridge` | 15 passed, 0 failed (5,59 s — w tym realna synteza Piper i pipeline rozdziału z cache) |
| `audioticon-cli` | 34 passed, 0 failed |
| `audioticon-core` | 130 passed, 0 failed (w tym `test_export_to_flac_synthetic`, `_flac_16bit_`, `_mp3_`, `_m4b_`) |
| **razem** | **179 passed, 0 failed** |

### 9.2 QtTest — `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./tests`

Zbudowane świeżo z `CONFIG+=release` (16 638 448 B, więc testy biegną na tym samym
release'owym staticlibie, który trafił do bundle).

```
Totals: 27 passed, 0 failed, 0 skipped, 0 blacklisted, 6994ms
```

W tym: `ffiLinksRustCore`, `jobFlowStartProgressFinish`, `jobCancelStopsTheJob`,
`audioPlayerPlaybackCycle`, `preflightCxxHeaderIsValid`,
`fallbackWithoutModelReportsMissingVoiceModel`, `uiScreenshotSmokeTest`.

### 9.3 CLI i FFI

| Komenda | Wynik |
|---|---|
| `bin/audioticon --version` | `audioticon 1.0.0`, exit 0 |
| `bin/audioticon presets` | 26 presetów, exit 0 |
| `bin/audioticon models list` | 142 modele katalogu, exit 0 |
| `bin/audioticon estimate` | 31 segmentów / 994 znaki / ~$0.2982, exit 0 (bez sieci dla demo) |
| `bin/audioticon-studio --probe-ffi` | `audioticon core sample rate: 44100 Hz`, exit 0 |

### 9.4 Doctor

Patrz §6.2 — 0/0 w środowisku docelowym, 3 oczekiwane ostrzeżenia bez modelu, `--strict` → 2.

---

## 10. Znaleziska i ryzyka

**R1 (średnie, testy).** Testy Qt `jobFlowStartProgressFinish` i `jobCancelStopsTheJob`
zakładają **mock** (asercja `outputPath().contains("200ms")` oraz wyścig anulowania).
Przy ustawionym `AUDIOTICON_PIPER_MODEL` + `VIRTUAL_ENV` uruchamia się prawdziwa
synteza i oba testy padają: pierwszy nie znajduje „200ms” w nazwie, drugi kończy
stanem `Finished` zamiast `Cancelled`. Bez tych zmiennych jest 27/27.
To nie jest defekt bundle'a (produkcja z modelem działa), ale **testy są wrażliwe na
środowisko**; warto je rozdzielić na „mock flow” i „real flow” (osobny plik/sekcja).
Nie zmieniano testów — poza zakresem M12.

**R2 (niskie, CLI).** Potokowanie wyjścia CLI do `head` daje kod **101**: Rust `println!`
panikuje na zamkniętym pipe (SIGPIPE). Widać to np. przy `audioticon models list | head`.
Uruchomienie bez potoku kończy się 0. Kosmetyczne, ale warto rozważyć `SIGPIPE` default
w `main` (np. `libc::signal(SIGPIPE, SIG_DFL)`) w przyszłym etapie.

**R3 (oczekiwane).** Bez modelu doctor raportuje 3 ostrzeżenia, a aplikacja pokazuje
baner „Brak modelu głosu” (zachowanie z B13). To stan poprawny, nie błąd bundle'a.

**R4 (potwierdzone).** qmake nie jest potrzebny po instalacji — dowód z pułapką w PATH (§6.3).

**R5 (rozwiązane).** Dwie usterki doctora (fałszywe alarmy ffmpeg przez `pipefail`/SIGPIPE,
zbyt płytki skan ORT) naprawione w trakcie M12 i zweryfikowane ponownie.

**R6 (informacyjne).** `bin/audioticon` i `bin/audioticon-studio` są dynamiczne: wymagają
glibc 2.44 z hosta budowy. Bundle nie zadziała na starszej dystrybucji — manifest podaje
`host_glibc`, a doctor pokaże brakujące symbole jako `FAIL`.

---

## 11. Odtworzenie krok po kroku

```bash
ROOT="/home/jaro/.openclaw/workspace/AUDIOTICON 2.0"
S="$ROOT/release/audioticon-local-linux"

# 1. Rust release
cd "$ROOT/backend" && cargo build --release --locked --workspace

# 2. Qt release (linkuje release staticlib)
cd "$ROOT/ui/build-release" && qmake6 ../agent-console.pro CONFIG+=release && make -j"$(nproc)"

# 3. Staging
rm -rf "$S" && mkdir -p "$S"/{bin,scripts,share}
cp "$ROOT/backend/target/release/audioticon" "$S/bin/audioticon"
cp "$ROOT/ui/build-release/astra-agent-console" "$S/bin/audioticon-studio"
chmod 0755 "$S/bin/"*
strip --strip-unneeded "$S/bin/audioticon" "$S/bin/audioticon-studio"
cp "$ROOT/backend/LICENSE-MIT" "$ROOT/backend/LICENSE-APACHE" \
   "$ROOT/backend/THIRD_PARTY_NOTICES.md" "$S/share/"
# scripts/ + share/README-RELEASE.md + manifest.json — jak w §4 i §5

# 4. Weryfikacja
"$S/scripts/doctor.sh"
"$S/bin/audioticon-studio" --probe-ffi
cd "$ROOT/backend" && cargo test --workspace --no-fail-fast
cd "$ROOT/ui/build-tests" && qmake6 ../tests/tests.pro CONFIG+=release && make -j"$(nproc)"
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./tests
```

Wskazówka: testy Qt uruchamiaj **bez** `AUDIOTICON_PIPER_MODEL`/`VIRTUAL_ENV` (patrz R1).

---

## 12. Czego nie zweryfikowano

- **Wygląd GUI nie był oglądany.** Aplikacja startuje offscreen i przechodzi testy
  renderowania, ale nie sprawdzano okna na żywej sesji Wayland/X11 ani zrzutów ekranu.
- **Brak testu na czystej maszynie** bez Qt/FFmpeg/Pythona — doctor symulowano tylko
  przez odcięcie zmiennych środowiskowych i pułapkę qmake; nie usuwano pakietów systemowych.
- **Brak testu na starszej dystrybucji** (glibc < 2.44) — ograniczenie dynamicznego linkowania.
- **Nie testowano faktycznej syntezy z poziomu GUI** w bundle (testy Rust pokrywają realny
  Piper przez bridge; GUI startowało offscreen bez modelu i z modelem tylko w testach Rust).
- **Brak instalatora i integracji z menu** — M12 świadomie kończy się na katalogu bundle.

---

## 13. Pliki utworzone / zmienione

| Plik | Status |
|---|---|
| `release/audioticon-local-linux/` (9 plików) | **nowe** — artefakt M12 |
| `release/audioticon-local-linux/scripts/doctor.sh` | **nowy** |
| `release/audioticon-local-linux/scripts/run-studio.sh` | **nowy** |
| `release/audioticon-local-linux/share/README-RELEASE.md` | **nowy** |
| `release/audioticon-local-linux/manifest.json` | **nowy** |
| `M12_LOCAL_RELEASE_BUNDLE.md` | **nowy** (ten raport) |
| `backend/`, `ui/` — kod źródłowy | **bez zmian** |
| `ui/build-release/`, `ui/build-tests/`, `backend/target/` | artefakty budowy (gitignored) |

Nie wykonano żadnego commita.
