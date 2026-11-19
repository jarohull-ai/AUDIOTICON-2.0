# M10-B — Audyt release i stabilizacji Audioticon 2.0

Data: 2026-11-18
Zakres: `AUDIOTICON 2.0/backend`, `AUDIOTICON 2.0/ui`
Zasady: brak zmian w kodzie produkcyjnym, brak zmian QML/UI, brak pobierania pakietów/modeli, brak commitów.

## Środowisko pomiarowe

| Element | Wartość |
|---|---|
| OS | Linux x86_64 (12 rdzeni, 31 GB RAM) |
| Rust | rustc 1.100.0 (0ef1bc386 2026-11-09), cargo 1.100.0 |
| C/C++ | GCC/G++ 16.2.1 |
| Qt | 6.12.0 (`/usr/lib`), qmake6 3.1 |
| FFmpeg | n9.0.2 (Lavf 63.1.102, Lavc 63.1.102) |
| Model Piper | `pl_PL-bass-high.onnx` (109 MB) + `.json` (4,9 KB) |
| ONNX Runtime | `libonnxruntime.so.1.32.0` (34 MB, w venv Hermes) |

---

## 1. `cargo test --workspace` oraz problem FFmpeg 9 / loudnorm

### Wynik `cargo test --workspace --no-fail-fast`

| Target | Wynik |
|---|---|
| `audioticon-bridge` (lib) | **13 passed, 0 failed** |
| `audioticon-cli` (main) | 0 testów |
| `audioticon-cli` (`tests/cli_e2e.rs`) | **34 passed, 0 failed** |
| `audioticon-core` (lib) | **127 passed, 1 failed** |
| **Razem** | **174 / 175 passed** |

Jedyny failing test:

```
audio::tests::test_export_to_flac_synthetic ... FAILED
panicked at crates/core/src/audio.rs:2803:
FLAC export failed: Konwersja do FLAC zakończyła się niepowodzeniem: ExitStatus(unix_wait_status(59904))
```

`unix_wait_status(59904)` = kod wyjścia 234 → ffmpeg przerwany, nie błąd wywołania.

### Przyczyna — dokładnie zdiagnozowana

To **nie** jest problem z FLAC jako takim, tylko z **`loudnorm` w FFmpeg 9 + enkoderem FLAC**.
`loudnorm` w tej wersji pracuje wewnętrznie w 192 kHz i **nie przywraca docelowego sample rate**, jeśli żaden filtr za nim tego nie zrobi:

```
[format_out_#0:0] auto-inserting filter 'auto_aresample_1' between 'Parsed_loudnorm_0' and 'format_out_#0:0'
[auto_aresample_1] ch:1 fmt:dbl r:192000Hz -> ch:1 fmt:s32 r:192000Hz
[flac] encoding as 24 bits-per-sample ...
[flac] invalid block size: 192000
[enc:flac] Error while opening encoder -> Could not open encoder before EOF
```

FLAC odrzuca 192 kHz („invalid block size”), więc enkoder nigdy się nie otwiera. Objaw w logu (`Error sending frames to consumers: Invalid argument`, `Task finished with error code: -22`) jest mylący — wygląda jak błąd negocjacji formatu, a jest błędem parametrów enkodera.

Reprodukcja (materiał 1 s, 24 kHz mono, dokładnie jak w teście):

| Wariant filtra | Wynik |
|---|---|
| `aresample=48000:soxr,loudnorm=...` → FLAC `-sample_fmt s32` | **FAIL** (0 B) |
| `loudnorm=...` → FLAC `-sample_fmt s16` | **FAIL** (0 B) |
| `loudnorm=...,aformat=sample_fmts=s32` → FLAC s32 | **FAIL** (0 B) |
| `aresample=48000:soxr` (bez loudnorm) → FLAC s32 | OK |
| `loudnorm=...` → WAV `pcm_s16le` | OK (ale 192 kHz!) |
| **`loudnorm=...,aresample=48000:resampler=soxr:precision=28`** → FLAC s32 | **OK** — 48 kHz, mono, 24 bit |

Wniosek: w FFmpeg 9 kolejność filtrów musi być **`loudnorm` → `aresample`**, a nie `aresample` → `loudnorm`.
Obecny kod (`audio.rs:2251-2253`) ma `aresample` **przed** `loudnorm`, czyli po loudnorm sample rate zostaje 192 kHz.

### Co NIE jest zepsute

- `test_export_to_m4b_synthetic` — **pass** (AAC przyjmuje 192 kHz, więc defekt jest maskowany).
- `test_real_audio_loudness_measurement` (ledger, `loudnorm=print_format=json`) — **pass**.
- `loudnorm` jako taki działa (pomiar i normalizacja), pada dopiero enkoder FLAC.
- Eksport MP3 nie ma testu syntetycznego; ścieżka MP3 (`audio.rs:2176`) ma ten sam układ filtrów co M4B, czyli również kończy na 192 kHz — do sprawdzenia przy okazji poprawki.

**Zalecenie (wymaga osobnej zgody, nie wprowadzone):** w `export_to_flac` przenieść `aresample` za `loudnorm` (lub dodać `aresample` po nim) i dodać asercję sample rate w teście. To 2-linijkowa zmiana, ale leży w kodzie produkcyjnym.

---

## 2. Ostrzeżenia generowanego glue cxx

Kompilacja `audioticon-bridge` w release: **22 linie warningów, wszystkie z generowanego `lib.rs.cc`**, zero warningów z własnych plików `.cc`/`.rs`.

| Warning | Liczba | Lokalizacja |
|---|---|---|
| `'<unknown>' may be used uninitialized [-Wmaybe-uninitialized]` | 3 (unikalne miejsca) | `cxxbridge/sources/audioticon-bridge/src/lib.rs.cc:1407, 1443, 1479` |

Konstruktory `rust::cxxbridge1::Vec<T>::Vec()` dla `VoiceSegmentDto`, `ChapterDto`, `DirectiveItemDto` (GCC 16 + `-O3`). Kod jest generowany przez cxx 1.0.205, wywołuje `cxxbridge1$rust_vec$...$new(this)` — analiza GCC nie widzi inicjalizacji w kodzie Rust. **Fałszywy alarm, bez wpływu na runtime.** Nie tłumić w kodzie projektu; ewentualnie `#[allow]`/`-Wno-maybe-uninitialized` tylko dla glue w `build.rs`, ale to kosmetyka.

Osobno: **`include/cxx.h` w lokalnym rejestrze cargo ma 0 bajtów** (patrz Blocker B2) — to był realny powód nieudanego builda, nie warning.

---

## 3. Build Rust/Qt w profilu release

### Rust

```
cargo build --release --workspace      → OK, 31,75 s (zimny target, 12 rdzeni)
```

Zimny build w izolowanym `CARGO_TARGET_DIR=/tmp/...` — pełna reprodukcja od zera, bez pobierania czegokolwiek.

### Qt

```
qmake6 ../agent-console.pro CONFIG+=release && make -j12   → OK, 0 błędów
```

Aplikacja startuje (smoke, `QT_QPA_PLATFORM=offscreen`, 6 s): brak błędów QML, tylko ostrzeżenia sterownika GPU (`libvdpau_nvidia.so`, `radv ... testing use only`) — nie związane z kodem.

**Uwaga krytyczna:** `ui/src/bridge/rustlink.pri` ma na sztywno `AUDIOTICON_RUST_PROFILE = debug`, więc „release” Qt **linkuje debugowy** `libaudioticon_bridge.a` (212 MB, bez optymalizacji Rust). Build przechodzi, ale to nie jest prawdziwy release (patrz Blocker B3).

---

## 4. Rozmiary binarek (release + strip)

| Artefakt | Surowy | Po `strip` |
|---|---|---|
| `target/release/audioticon` (CLI) | 13 MB | **11 MB** |
| `target/release/libaudioticon_bridge.a` (staticlib) | 46 MB | — (archiwum) |
| `ui/build-release/astra-agent-console` (Qt) | 89 MB | **18 MB** |
| `ui/build-tests/tests` (Qt testy) | 89 MB | nie stripowany |
| *referencja:* `target/debug/audioticon` | 109 MB | — |
| *referencja:* `target/debug/libaudioticon_bridge.a` | 212 MB | — |

18 MB po strip dla aplikacji Qt **z debugowym** Rustem to wartość górna. Po przełączeniu `rustlink.pri` na `release` należy oczekiwać wyraźnego spadku (do potwierdzenia pomiarem po zmianie).

---

## 5. Wymagane biblioteki runtime

### `audioticon` (CLI)
Z linkowania: tylko `libgcc_s`, `libm`, `libc`, `ld-linux` — reszta statyczna (Rust). Runtime zewnętrzny: **`ffmpeg` w PATH** (mastering, M4B/FLAC/MP3), opcjonalnie Python + `piper-tts` dla syntezy, opcjonalnie `libonnxruntime.so` dla Kokoro.

### `astra-agent-console` (Qt)
Z `ldd` (95 zależności). Minimum do dystrybucji:

- **Qt 6.12**: `Qt6Core, Qt6Gui, Qt6Qml, Qt6QmlModels, Qt6QmlMeta, Qt6QmlWorkerScript, Qt6Quick, Qt6QuickControls2, Qt6QuickTemplates2, Qt6Multimedia, Qt6Network, Qt6Sql, Qt6OpenGL, Qt6DBus`
- **QML modules**: `QtQuick/Controls` (Basic/Fusion/Imagine/…), `QtQuick/Layouts`, `QtQuick/Window`
- **Plugins**: `platforms/libqxcb.so` (X11), `libqwayland*.so`, `imageformats/`, `multimedia/`, `sqldrivers/`
- **Systemowe**: `libstdc++`, `libicu{uc,i18n,data} 78`, `libharfbuzz`, `libfreetype`, `libfontconfig`, `libpng16`, `libxcb`, `libX11`, `libxkbcommon`, `libGLX/libOpenGL/libEGL`, `libpulse`, `libsndfile`, `libFLAC`, `libmp3lame`, `libogg/libvorbis/libopus`, `libz/libzstd/libbz2`, `libdbus-1`, `libsystemd`, `libcurl` (+ `libssl/libcrypto`, `libnghttp2/3`, `libssh2` — przez QtNetwork), `libgomp`
- **Narzędzia**: `ffmpeg` (eksport), Python 3 + `piper-tts` (synteza)
- **Opcjonalnie**: `libonnxruntime.so.1.32.0` (Kokoro)

Na czystym systemie bez Qt 6.12 i bez pluginów `xcb` aplikacja się nie uruchomi. Obecnie **nie ma bundlowania** (brak AppImage/`linuxdeploy`, brak `qt.conf`, brak kopiowanych pluginów).

---

## 6. `ORT_DYLIB_PATH` i plan dostarczenia ORT 1.32

Stan kodu (`crates/core/src/kokoro.rs`):

1. `ORT_DYLIB_PATH` (jeśli istnieje) → `ort::init_from(dylib)`.
2. Bez zmiennej: skan `~/.local/share/audioticon/venv/lib{,64}`, `$VIRTUAL_ENV/lib`, `/usr/lib`, `/usr/local/lib`, `/usr/lib/x86_64-linux-gnu` (do 6 poziomów, nazwa zaczynająca się od `libonnxruntime.so`).
3. Brak znalezienia → `ort::init()` (domyślne ładowanie) → błąd, jeśli biblioteki nie ma.
4. Runtime ładuje **`load-dynamic`**, więc `ldd` aplikacji **nie zawiera** `libonnxruntime` — brak twardej zależności linkowania (potwierdzone).

Stan maszyny: biblioteka jest tylko w venv Hermes (`~/.hermes/.../libonnxruntime.so.1.32.0`), **nie ma** jej w `/usr/lib` ani w katalogu aplikacji — na czystej maszynie Kokoro nie zadziała.

**Plan dostarczenia ORT 1.32 (propozycja):**

1. Dodać do paczki `lib/libonnxruntime.so.1.32.0` (34 MB) + symlink `libonnxruntime.so`.
2. W `main()` ustawiać `ORT_DYLIB_PATH` na `<appdir>/lib/libonnxruntime.so.1.32.0` **tylko jeśli** użytkownik go nie ustawił (nie nadpisywać środowiska deweloperskiego).
3. Alternatywnie: `RUNPATH=$ORIGIN/lib` + pozostawienie `ort::init()` — ale `ORT_DYLIB_PATH` jest bardziej przewidywalny i już obsłużony.
4. Wersjonowanie: kod używa `ort` z feature `api-31`; przy podmianie biblioteki trzymać się 1.32.x i dodać weryfikację przy starcie (czytelny komunikat zamiast paniki dlopen).
5. Licencja: ONNX Runtime = MIT — dopisać do `THIRD_PARTY_NOTICES.md` i do paczki.
6. Docelowo: rozważyć, czy Kokoro jest w ogóle w pierwszym pakiecie (Piper działa bez ORT). Jeśli nie — ORT nie jest blockerem v1.

---

## 7. Wykrywanie modelu `pl_PL-bass-high`

`resolve_piper_model()` (`crates/bridge/src/lib.rs:215`):

1. `AUDIOTICON_PIPER_MODEL` — jeśli wskazuje istniejący plik, wygrywa.
2. `models_dir()` = `$XDG_DATA_HOME/audioticon/models` (typowo `~/.local/share/audioticon/models`), kolejno:
   `pl_PL-bass-high.onnx`, `pl_PL-gosia-medium.onnx`, `pl_PL-darkman-medium.onnx`.
3. Brak → `None` → fallback.

Ustalenia:

- **`~/.local/share/audioticon/models/` jest puste** na tej maszynie → aplikacja startuje w fallbacku, mimo że model leży w `Portable-Local-Studio`.
- W kodzie produkcyjnym **nie ma** żadnych ścieżek `/home/jaro/...` (są tylko w testach warunkowych) — to dobrze, brak „dev leaków”.
- Auto-detekcja jest **env-gated**: bez `AUDIOTICON_PIPER_MODEL` użytkownik nie dostanie realnej syntezy, dopóki nie wgra modelu przez `audioticon models`/instalator.
- `setup.sh` pobiera `pl_PL-darkman-medium` (CC0) do `models_dir` — czyli ścieżka instalacyjna istnieje, ale wymaga sieci.
- Rekomendacja: instalator/`setup.sh` powinien kopiować model do `models_dir` i wypisywać wykrytą ścieżkę (`piper_model_path()` już istnieje w API mostu, warto pokazać w UI/diagnostyce).

---

## 8. Zachowanie fallbacku bez modelu

`BridgeEngine::start_job` (`lib.rs:861`):

- **model obecny** → `split_chapter_segments` → `spawn_chapter_render` (cache per segment, stitche, `chapter_{job_id}.wav`).
- **brak modelu** → `spawn_mock_render`: placeholder, 200 ms WAV, 3–6 kroków postępu (`planned_steps`), statusy „queued → running → finished”.
- Fallback **nie jest błędem**: interfejs działa, testy przechodzą.

Weryfikacja: `cargo test -p audioticon-bridge` — **13/13 w obu konfiguracjach** (z `AUDIOTICON_PIPER_MODEL` i bez), w tym `test_job_flow_queued_progress_finished` na ścieżce mock. Z modelem: `test_real_piper_synthesis_one_segment` i `test_real_piper_chapter_pipeline_cache` **realnie syntetyzują** (5,61 s).

Ryzyko produktowe: użytkownik bez modelu dostanie 200 ms ciszy i „Finished”. Powinien być wyraźny komunikat „brak modelu głosu” w `status_message` / UI, zanim uzna to za zepsuty produkt.

---

## 9. Wykluczenie `target/`, `build/`, modeli i binarek z artefaktów

Stan `.gitignore`:

- `backend/.gitignore`: `target/`, `dist/`, `*.wav`, `*.mp3` (z wyjątkami `docs/audio|samples`), `*.m4b`, `*.onnx`, `*.onnx.json`, `cache/`, `models/`, `tests/`
- `ui/.gitignore`: `build*/`, `*.o`, `*.user`, `Makefile`, `tests/tests`

Sprawdzone `find` (>5 MB, poza `target/` i `build*/`): tylko sample audio w `backend/docs/samples/` (3 × ~6,5 MB, świadomie whitelistowane w `.gitignore`).

Uwagi do paczkowania:

- `.gitignore` **nie wystarcza** — packager musi mieć jawną listę include, nie exclude. Do paczki: binarka, `lib/`, QML/plugins Qt, licencje, instrukcja. Nie: `target/`, `build*/`, `*.onnx`, `*.wav`, `*.flac`, `dist/`, `*.ffmeta.txt`, `*.tmp.*`.
- `backend/docs/samples/*.mp3` (~40 MB) nie powinny iść do paczki instalacyjnej.
- **Repo git jest na poziomie `~/.openclaw/workspace`** (`git status` z `backend/` pokazuje `../../ASTRA-UI-Template/`, `../../Audioticon-legacy/` itd.). `backend/.gitignore` nie chroni przed commitami z katalogu wyżej — build/CI musi działać na wyodrębnionym drzewie projektu, inaczej artefakty innych projektów wejdą do commita.
- Dorzucić `*.tmp.flac`, `*.ffmeta.txt`, `piper_*.wav`, `chapter_*.wav` (cache roboczy) do wykluczeń.

---

## 10. Propozycja powtarzalnego builda release

Skrypt do dodania jako `backend/scripts/build_release_full.sh` (propozycja — **nie utworzony**, bo to nowy plik poza zakresem „bez zmian”). Ma być idempotentny i **fail-fast** na preflightach, które dziś wywalają build w niejasny sposób.

```bash
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"   # backend/
PROJ="$(dirname "$ROOT")"                                  # AUDIOTICON 2.0/
UI="$PROJ/ui"
OUT="$PROJ/dist"
PROFILE=release
CXXH="$HOME/.cargo/registry/src/index.crates.io-"*/"cxx-"*/include/cxx.h

log() { printf '\n=== %s ===\n' "$*"; }

log "1/7 Preflight: narzędzia"
for t in cargo rustc qmake6 make strip ffmpeg; do
  command -v "$t" >/dev/null || { echo "BRAK: $t"; exit 1; }
done
rustc --version; qmake6 -query QT_VERSION; ffmpeg -version | head -1

log "2/7 Preflight: cxx glue header (cache cargo)"
if ! grep -q 'namespace rust' $CXXH 2>/dev/null; then
  echo "ERROR: include/cxx.h jest pusty/uszkodzony."
  echo "Napraw offline: tar -xzf ~/.cargo/registry/cache/*/cxx-1.0.205.crate \\"
  echo "  -C \$(dirname $(dirname $(dirname $CXXH))) cxx-1.0.205/include/cxx.h"
  exit 1
fi

log "3/7 Rust release (workspace)"
( cd "$ROOT" && cargo build --release --locked --workspace )

log "4/7 Qt release (łączy Rust staticlib)"
rm -rf "$UI/build-release" && mkdir -p "$UI/build-release"
( cd "$UI/build-release" && qmake6 ../agent-console.pro CONFIG+=release && make -j"$(nproc)" )

log "5/7 Testy (wymagają ffmpeg; bez modelu część testów działa w fallbacku)"
( cd "$ROOT" && cargo test --workspace --no-fail-fast )
( cd "$UI" && QT_QPA_PLATFORM=offscreen ./build-release/tests 2>/dev/null || true )

log "6/7 Pakowanie (jawne include, zero target/build)"
STAGE="$OUT/audioticon-$(date +%Y%m%d)"
rm -rf "$STAGE" && mkdir -p "$STAGE"/{bin,lib,share}
cp "$ROOT/target/release/audioticon"        "$STAGE/bin/"
cp "$UI/build-release/astra-agent-console"  "$STAGE/bin/audioticon-studio"
strip "$STAGE/bin/"*
# opcjonalnie: ORT 1.32
# cp /path/libonnxruntime.so.1.32.0 "$STAGE/lib/" && ln -s ... 
cp "$ROOT/LICENSE-MIT" "$ROOT/LICENSE-APACHE" "$ROOT/THIRD_PARTY_NOTICES.md" "$STAGE/share/"

log "7/7 Archiwum + sumy"
( cd "$OUT" && tar -czf "$(basename "$STAGE").tar.gz" "$(basename "$STAGE")" \
  && sha256sum "$(basename "$STAGE").tar.gz" > "$(basename "$STAGE").tar.gz.sha256" )
```

Uwagi wykonawcze:

- `--locked` gwarantuje ten sam `Cargo.lock`; brak `--offline` celowo (pierwszy build może potrzebować crate'ów), ale w CI warto `--offline` z vendorem.
- **Krok 4 wymaga najpierw poprawki `rustlink.pri`** (`AUDIOTICON_RUST_PROFILE = release`), inaczej „release” Qt wciąż linkuje debug (Blocker B3). Do czasu zgody na tę zmianę skrypt powinien wypisać ostrzeżenie.
- Warto dodać `rust-toolchain.toml` (pin 1.100.0) i sprawdzanie minimalnej wersji Qt (6.12) oraz FFmpeg.
- Alternatywa minimalna bez skryptu: trzy komendy z preflightem `grep namespace rust` na `cxx.h`.

---

## 11. Blockery przed pierwszym pakietem instalacyjnym

### A. Twarde (blokują wydanie)

**B1 — Eksport FLAC nie działa na FFmpeg 9 (regresja w repo).**
`loudnorm` zostawia 192 kHz, FLAC odrzuca blok 192000. Test `test_export_to_flac_synthetic` czerwony.
Fix: `aresample` **po** `loudnorm` w `export_to_flac` (i sprawdzić MP3 pod tym samym kątem) + asercja sample rate w teście. *Wymaga zgody — zmiana w kodzie produkcyjnym.*

**B2 — Uszkodzony `include/cxx.h` w lokalnym rejestrze cargo (0 B).**
`cargo build` (także debug od zera) pada na `cxx.cc` z mylącymi błędami C++ (`std::uint8_t` / `rust::` undeclared). Wersja w `.crate` ma 29 550 B. Naprawa offline (bez pobierania):
`tar -xzf ~/.cargo/registry/cache/*/cxx-1.0.205.crate -C <src>/cxx-1.0.205 cxx-1.0.205/include/cxx.h`
*(Wykonane w trakcie audytu, żeby zmierzyć release — plik przywrócony do 29 550 B. Do stałej ochrony: preflight w skrypcie + świeży cache w CI.)*

**B3 — `rustlink.pri` pinuje profil `debug`.**
Qt „release” linkuje 212 MB debugowego staticlibu: brak optymalizacji Rust, zawyżony rozmiar (89 MB → 18 MB po strip), ryzyko mieszania profilów. Fix: parametr `AUDIOTICON_RUST_PROFILE` z `.pro`/env, domyślnie `release`. *Wymaga zgody — plik konfiguracyjny UI.*

**B4 — Brak deploymentu Qt.**
Aplikacja wymaga systemowego Qt 6.12 + pluginów `xcb`/`wayland` + modułów QML. Brak `qt.conf`, brak kopiowanych pluginów, brak AppImage. Na maszynie bez Qt 6.12 (np. Ubuntu 24.04 ma 6.4/6.5) aplikacja **nie wystartuje**.

**B5 — Piper wymaga Pythona z `piper-tts`, którego nie ma w paczce.**
`find_python()` szuka `~/.local/share/audioticon/venv/bin/python3`, potem `VIRTUAL_ENV`, potem systemowego `python3`. Bez `piper-tts` w tym interpreterze synteza padnie z błędem workera. `setup.sh` instaluje przez sieć — sprzeczne z „offline” produktem.

**B6 — Brak modelu w paczce i puste `models_dir`.**
Na tej maszynie `~/.local/share/audioticon/models/` jest pusty → domyślnie fallback 200 ms. Trzeba: dystrybuować model (licencja!) albo jawny krok instalacyjny + czytelny komunikat w UI.

### B. Miękkie (do decyzji przed v1)

**B7 — ONNX Runtime 1.32 nieszypowany.** Kokoro nie zadziała na czystej maszynie; `ORT_DYLIB_PATH` nieustawiony. Albo dostarczyć `libonnxruntime.so.1.32.0` (plan w pkt 6), albo wyłączyć Kokoro z v1 i usunąć obietnicę z UI.

**B8 — FFmpeg jest zewnętrzną zależnością.** Bez `ffmpeg` w PATH padają mastering, M4B, FLAC, MP3 i pomiary głośności. Do decyzji: bundlowanie statycznego ffmpeg (licencja GPL — `--enable-gpl` w obecnym buildzie!) vs. jawny wymóg instalacyjny. **Uwaga licencyjna:** obecny systemowy ffmpeg ma `--enable-gpl` i `--enable-version3`; bundlowanie takiego binarium do komercyjnego produktu wymaga analizy GPL.

**B9 — `scripts/package_release.sh` pakuje tylko CLI.** Ścieżka `target/release/audioticon` jest poprawna dla `audioticon-cli`, ale skrypt nie buduje Qt, nie bundluje Qt/ORT/Python, nie robi `--locked`, używa `zip` bez sprawdzenia obecności, i kopiuje `README/licenses` bez `THIRD_PARTY_NOTICES` dla nowych zależności (Qt, ORT, model).

**B10 — Brak pinów toolchaina.** GCC 16 ujawnił kruchość (cxx), Qt 6.12 jest bardzo świeże, FFmpeg 9 zmienił zachowanie filtrów. Brak `rust-toolchain.toml`, brak minimalnych wersji w preflight.

**B11 — Glue cxx: 3× `-Wmaybe-uninitialized`.** Kosmetyka, fałszywy alarm w kodzie generowanym; nie blokuje. Opcjonalnie wyciszyć tylko dla glue.

**B12 — Testy zależne od środowiska.** Testy realnego Piper są warunkowe (model + venv + ścieżki `/home/jaro/...` w testach); na CI przejdą jako „skip” i nie pokryją syntezy. Do rozdzielenia: testy jednostkowe (zawsze) vs. integracyjne (jawny `AUDIOTICON_TEST_PIPER=1`).

**B13 — Fallback bez modelu raportuje „Finished”.** Produktowo mylące; dodać status „brak modelu głosu” przed pierwszym pakietem.

---

## Podsumowanie liczbowe

| Metryka | Wartość |
|---|---|
| Testy Rust | 174/175 (1 fail: FLAC) |
| Testy Qt | 25/25 (24 testy + init/cleanup) |
| Testy mostu z realnym Piper | 13/13, w tym pełny rozdział + cache hit |
| Build release Rust (zimny) | 31,75 s, 0 błędów |
| Build release Qt | 0 błędów, 0 zmian QML |
| CLI po strip | 11 MB |
| Aplikacja Qt po strip | 18 MB (z debugowym Rustem — górna granica) |
| Blockery twarde | **6** (B1–B6) |
| Blockery miękkie | 7 (B7–B13) |

## Czego nie sprawdzono

- Nie uruchamiano GUI wizualnie (brak zmian QML; smoke tylko `offscreen`, bez oceny wyglądu).
- Nie testowano na czystym systemie bez Qt/FFmpeg (brak VM; wymagałoby instalacji pakietów — zabronione).
- Nie mierzono rozmiaru po przełączeniu `rustlink.pri` na `release` (wymaga zmiany, na którą nie ma zgody).
- Nie weryfikowano licencji modelu `pl_PL-bass-high` ani zgodności GPL przy bundlowaniu ffmpeg — do analizy prawnej.
