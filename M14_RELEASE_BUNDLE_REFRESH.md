# Raport M14 — odświeżenie lokalnego release bundle po M13

**Data:** 19 listopada 2026, 05:20–05:43 UTC
**Środowisko:** Omarchy / Arch Linux, kernel `7.2.15-1-omarchy` x86_64, glibc 2.44, 12 rdzeni
**Sesja graficzna:** Wayland (Hyprland 0.56.2, `WAYLAND_DISPLAY=wayland-1`, `DISPLAY=:0`)
**Źródła:** `AUDIOTICON 2.0/backend`, `AUDIOTICON 2.0/ui` (bez modyfikacji oryginalnych repozytoriów)
**Bundle:** `release/audioticon-local-linux`
**Zakres:** odświeżenie artefaktów po M13 (First Run Wizard + Doctor UX). Bez commitów, bez zmian w `Audioticon-legacy`, `ASTRA-UI-Template`, `~/Downloads`.

---

## 1. Podsumowanie

| Element | Stan |
|---|---|
| Rust release (`cargo build --release --locked --workspace`) | OK (target aktualny, bez zmian w rdzeniu — hash CLI identyczny jak w M12) |
| Qt UI release z M13 (`qmake6` + `make -j12`) | OK, `DoctorModel` + `FirstRunWizard` + `DoctorView` wkompilowane w `qrc` |
| `bin/audioticon-studio` | odświeżony, strip, 8 899 032 B (8,5 MiB) |
| `bin/audioticon` | bez zmian (Rust nietknięty w M13), 10 529 584 B (10,0 MiB) |
| `scripts/doctor.sh` | rozszerzony o moduły QML, próbkę FFI i stan First Run (9 sekcji, 12 908 B) |
| `scripts/run-studio.sh` | szybka ścieżka `--probe-ffi`, fallback modelu, katalogi `models`/`voices` (3 948 B) |
| `manifest.json` | przegenerowany (wersja, fingerprint źródeł, SHA-256 wszystkich plików, toolchain) |
| `share/` | licencje MIT/Apache + THIRD_PARTY_NOTICES zachowane, README Release zaktualizowany o First Run/Doctor |
| Doctor | 0 błędów / 0 ostrzeżeń (ze modelem), 0/3 (bez modelu), `--strict` → 2 |
| `--probe-ffi` przez launcher | `audioticon core sample rate: 44100 Hz`, exit 0 |
| CLI smoke | `--version`, `presets`, `models list`, `estimate -i` — exit 0 |
| Testy Rust | **179 / 179 passed** |
| Testy QtTest (offscreen) | **32 / 32 passed** (10 095 ms) |
| Aplikacja w realnej sesji Wayland | uruchomiona, First Run i Doctor sprawdzone wizualnie |
| Bundle | 9 plików, 19 471 746 B (18,6 MiB), bez Qt/FFmpeg/Python/ORT/modeli |

Wizualna kontrola wykryła **jeden realny defekt** (przycięty podtytuł w wierszu „Model Głosu Piper” w kreatorze First Run). Został naprawiony jedną zmianą w QML, po czym bundle przebudowano i ponownie zweryfikowano.

---

## 2. Build

### 2.1 Rust

```bash
cd backend
cargo build --release --locked --workspace
```

`Finished release profile` — kompilacja przyrostowa (brak zmian w `crates/**` po M12/M13). Ostrzeżenia pochodzą wyłącznie z generowanego przez cxx `lib.rs.cc` (`-Wmaybe-uninitialized`) i nie dotyczą naszego kodu. Binarka CLI jest bajtowo identyczna z M12:

```
41391747caaf6d7e89b9fb5b9da50d2718251963cacbedf686c342de50aa21b2  bin/audioticon
```

To dowód, że M13 nie tknęło rdzenia — cała zmiana jest po stronie Qt/QML.

### 2.2 Qt UI

```bash
cd ui/build-release
qmake6 ../agent-console.pro CONFIG+=release && make -j12
strip --strip-unneeded -o bin/audioticon-studio ../../ui/build-release/astra-agent-console
```

`qmake` został uruchomiony ponownie (nie tylko `make`), bo `agent-console.pro` od M12 zyskał `src/doctor/DoctorModel.cpp` i `src/doctor/DoctorModel.h`. W logu widać kompilację `DoctorModel.o`, `moc_DoctorModel.o` oraz przegenerowanie `qrc_resources.cpp` (zasoby QML). Linkowanie: `-laudioticon_bridge` z `backend/target/release` (statyczna biblioteka Rust). 0 błędów, 0 ostrzeżeń kompilatora z naszych plików.

Binar QML (`FirstRunWizard.qml`, `DoctorView.qml`, `NavRail.qml`, `ContextInspector.qml`, `StudioView.qml`, `AgentMain.qml`) jest w `src/resources.qrc`, więc bundle pozostaje jednym plikiem wykonywalnym — nie ma katalogu QML do kopiowania.

### 2.3 Skład bundle

```
release/audioticon-local-linux/
├── bin/audioticon                 10 529 584 B  41391747…aa21b2
├── bin/audioticon-studio           8 899 032 B  5e481f85…e3befa
├── scripts/doctor.sh                  12 908 B  add1c948…c16ef1
├── scripts/run-studio.sh               3 948 B  e5129f79…68797
├── share/LICENSE-APACHE               10 173 B
├── share/LICENSE-MIT                   1 073 B
├── share/THIRD_PARTY_NOTICES.md        3 838 B
├── share/README-RELEASE.md             5 491 B  b249ceaa…ff122
└── manifest.json                       5 699 B  b49328bd…6c1f35d
```

`find . -type f | wc -l` = **9**. Brak Qt, FFmpeg/ffprobe, Pythona, ORT, modeli, venv, `target/`, `build-*` — zgodnie z polityką `artifact_policy` w manifeście (wszystkie `bundles_*: false`).

---

## 3. Zmiany w skryptach bundle

### 3.1 `doctor.sh` — nowe kontrole (nadal 9 sekcji)

Sekcja `[1/9] Qt 6` rozszerzona o dwie kontrole wynikające z M13:

1. **Moduły QML** — wyszukiwanie `QtQuick/Controls` w `QML_IMPORT_PATH`, `QML2_IMPORT_PATH`, `/usr/lib/qt6/qml`, `/usr/lib64/qt6/qml`, `/usr/lib/x86_64-linux-gnu/qt6/qml`. Brak → `[FAIL]` z podpowiedzią `qt6-declarative`. Cały interfejs M13 (First Run, Doctor) to QML Quick Controls 2, więc to realna zależność runtime, wcześniej niekontrolowana.
2. **Rdzeń Rust przez FFI** — `QT_QPA_PLATFORM=offscreen bin/audioticon-studio --probe-ffi`, oczekiwane dokładnie `audioticon core sample rate: 44100 Hz` i exit 0; inaczej `[FAIL]`. Dzięki temu doctor weryfikuje także statyczne linkowanie Rust, a nie tylko `ldd`.

Sekcja `[9/9]` rozszerzona o katalog konfiguracji i stan kreatora:

- `$XDG_CONFIG_HOME/astra-ui-agent` dołączony do testu zapisu (QSettings zapisuje tam `first_run_completed` / `inspection_mode` / `active_model_path` / `language`);
- `[ OK ] First Run: ukończony (…conf)` gdy `first_run_completed=true`, inaczej `note` (kreator pokaże się przy starcie). To czysto informacyjne — nie wpływa na kod wyjścia.

Zaktualizowano też `--help`. Wynik na tej maszynie (z realnym Piperem):

```
[1/9] Qt 6 …           [ OK ] ld.so, Qt Core 6.12.0, plugin xcb/wayland, moduły QML, rdzeń Rust 44100 Hz
[2/9] FFmpeg …         [ OK ] n9.0.2, loudnorm, FLAC, AAC
[3/9] ffprobe …        [ OK ] n9.0.2
[4/9] Python 3 …       [ OK ] venv piper-venv (3.12.16)
[5/9] piper-tts …      [ OK ] moduł 'piper'
[6/9] model głosu …    [ OK ] pl_PL-bass-high.onnx (109M) + .onnx.json
[7/9] ORT …            [ OK ] libonnxruntime.so.1.32.0
[8/9] audio …          [ OK ] ALSA (13 pcm), PipeWire, pactl
[9/9] XDG …            [ OK ] data/outputs/models/cache/config + stan First Run
błędy: 0   ostrzeżenia: 0   → GOTOWE
```

Bez modelu/silnika (środowisko czyste): 0 błędów / 3 ostrzeżenia (piper-tts, model, ORT), exit 0; `--strict` → exit 2. Zachowanie z M12 bez regresji.

### 3.2 `run-studio.sh`

- **`--probe-ffi` bez sesji graficznej** — launcher nie wymaga już pluginu platformy ani `ldd`-owej bramki, gdy pierwszym argumentem jest `--probe-ffi`; `main.cpp` obsługuje tę próbkę *przed* `QGuiApplication`, więc działa headless i na maszynie bez Wayland/X11.
- **Fallback modelu** — po liście preferowanych `pl_PL-*` launcher wybiera dowolny `*.onnx` z `$XDG_DATA_HOME/audioticon/models` (ten sam fallback co Doctor i doctor.sh).
- **Katalogi** — `mkdir -p` obejmuje teraz także `models/` i `voices/` (zgodnie z `Paths::voicesDir()`).

Reszta bez zmian: `exec` na natywne Qt, bez serwera HTTP i przeglądarki, ścieżki liczone od położenia skryptu (bundle jest przenośny).

### 3.3 `manifest.json`

Generowany skryptem (nowe pola względem M12):

- `milestone: "M14 (refresh after M13 Doctor UX)"`, świeże `built_at`;
- `binaries[].features: ["first_run_wizard","doctor_view","inspection_mode_without_tts"]` dla GUI;
- `files[]` — teraz **obiekty z SHA-256 i rozmiarem dla każdego pliku** (w M12 była mieszanka stringów), więc integralność całego bundle można sprawdzić bez dodatkowych narzędzi;
- `system_requirements.qt_qml_modules: ["QtQuick.Controls","QtQuick.Layouts"]`;
- `verification`: doctor (9 sekcji), `ffi_probe` przez launcher, `cli_smoke`, `gui_visual`;
- `source_state.source_fingerprint_sha256` = `86eb3a52145ecc8bca690e5d07cb6c331f3631cbf7d876f1e5a692aa06feddc3` (97 plików; zakres jak w M12, więc fingerprint jest porównywalny między kamieniami). Repo nadal nie ma HEAD (`no_commits: true`), więc odcisk treści jest jedynym identyfikatorem stanu źródeł;
- `qmake_required_after_install: false` — potwierdzone działaniem (patrz M12) i niezmienione.

---

## 4. Weryfikacja funkcjonalna

### 4.1 `--probe-ffi` przez launcher

```
$ ./scripts/run-studio.sh --probe-ffi
audioticon core sample rate: 44100 Hz
PROBE_EXIT=0
```

### 4.2 CLI smoke (binarki z bundle)

| Komenda | Wynik |
|---|---|
| `bin/audioticon --version` | `audioticon 1.0.0`, exit 0 |
| `bin/audioticon presets` | 32 linie, exit 0 |
| `bin/audioticon models list` | 152 linie, exit 0 |
| `bin/audioticon estimate -i /tmp/m14-script.txt` | 1 segment, 180 znaków, ~$0.0540, exit 0 |

(Uwaga: `estimate` przyjmuje `-i/--input`, nie `--text` — poprawiona składnia względem pierwszego podejścia w M12.)

### 4.3 Testy Rust

```bash
VIRTUAL_ENV=…/piper-venv AUDIOTICON_PIPER_MODEL=…/pl_PL-bass-high.onnx \
  cargo test --release --locked --workspace
```

```
bridge  15 passed   cli 34 passed   core 130 passed   (razem 179, 0 failed)
CARGO_TEST_EXIT=0
```

### 4.4 QtTest (offscreen)

```bash
cd ui/build-tests && make -j12
env -u VIRTUAL_ENV -u AUDIOTICON_PIPER_MODEL \
  QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./tests
```

```
Totals: 32 passed, 0 failed, 0 skipped, 0 blacklisted, 10095ms
```

Uruchomione **bez** `VIRTUAL_ENV` i `AUDIOTICON_PIPER_MODEL` — to nadal wymagane, bo dwa testy przepływu zadań mockują Piper i przy obecnym modelu dają inne wyniki (znane znalezisko R1 z M12, poza zakresem M14). W składzie 32 testów są wszystkie testy M13: `firstRunWizardFlow`, `doctorMissingModelReportsStatus`, `doctorCompleteEnvironmentWithModel`, `doctorInspectionModeWithoutTts`, `doctorUiModuleAndOffscreenScreenshot`.

---

## 5. Uruchomienie w realnej sesji Wayland i kontrola wizualna

Aplikacja została uruchomiona z bundle przez `scripts/run-studio.sh` (nie offscreen), w sesji Hyprland 0.56.2 na Waylandzie. Aby nie dotykać konfiguracji użytkownika, oba przebiegi użyły tymczasowych katalogów `XDG_CONFIG_HOME`/`XDG_DATA_HOME` (`/tmp/m14cfg*`, `/tmp/m14data`) — w ten sposób uzyskano też „świeży” stan First Run.

**Przebieg 1 — First Run (czysty profil).** Okno 1896×1030 na monitorze 2 (`grim -g "3724,38 1896x1030"`). Kreator pojawił się automatycznie: nagłówek `AUDIOTICON 2.0 · KONFIGURACJA ŚRODOWISKA I MODELI GŁOSU`, checklist 5 podsystemów z diodami i etykietami `[ OK ]` / `[ BRAK SILNIKA ]` / model w stanie błędu, przycisk „Wskaż plik...”, ramka „ℹ ZASADA PRYWATNOŚCI…”, przyciski `🩺 Otwórz Doctor`, `⚙ Uruchom bez TTS (Tryb Reżyserski)`, `🚀 Rozpocznij pracę` (nieaktywny — środowisko niegotowe). Nagłówek aplikacji pokazuje czerwoną plakietkę „Brak modelu głosu”. Tło pod scrimem renderuje się poprawnie (Studio, waveform, ścieżki, transport).

**Znaleziony defekt (naprawiony).** W wierszu „Model Głosu Piper” podtytuł `Brak modelu głosu w ~/.local/share/audioticon/models` był przycięty dolną krawędzią wiersza — `implicitHeight: 46` nie mieścił dwóch linii tekstu (11 px DemiBold + 9 px monospace + spacing), a dodatkowo tekst nie miał `Layout.fillWidth`, więc nie mógł się elidować. Poprawka w `ui/src/qml/FirstRunWizard.qml`:

```diff
                 Rectangle {
+                    implicitHeight: 58
+                    Layout.minimumHeight: 58
…
                             Text {
+                                Layout.fillWidth: true
                                 text: … : qsTr("Brak modelu głosu w ~/.local/share/audioticon/models")
```

`Layout.minimumHeight` blokuje dodatkowo ściśnięcie wiersza przez `ColumnLayout`, gdy karta jest ciasna (przy mniejszym oknie 1100×734 przycięcie było wyraźniejsze). Po poprawce wiersz ma pełny podtytuł z marginesem — potwierdzone zrzutem.

**Przebieg 2 — Doctor.** Po przebudowie i podmianie binarki: profil z `first_run_completed=true`, skrót `Ctrl+7` wysłany do okna (`wtype` po `hl.dsp.focus`) — moduł Doctor otworzył się w realnej sesji. Widok: status `WYKRYTO BRAKI W ŚRODOWISKU (1 błędów, 1 ostrzeżeń)`, pasek akcji (Kopiuj pełny raport / Odśwież stan / Wskaż model z dysku / Włącz tryb bez TTS / Resetuj kreator), 6 kafelków: Qt 6.12.0 + platforma `wayland` + FFI 44100 Hz `[OK]`, audio (Rysen HD Audio Controller) `[OK]`, FFmpeg n9.0.2 + loudnorm/FLAC/AAC `[OK]`, Piper `[WARN]`, model `[FAIL]`, ORT `[OPCJONALNY]`, oraz tabela XDG pokazująca realne ścieżki `…/tmp/m14data/audioticon/...` z `[ Zapis OK ]`. To potwierdza, że Doctor czyta `XDG_DATA_HOME`/`XDG_CACHE_HOME`, a nie ścieżki zahardkodowane.

Zrzuty (poza bundle, w `release/`):

- `release/m14-firstrun-wayland.png` — First Run po poprawce (1896×1030)
- `release/m14-firstrun-wayland-before-fix.png` — stan przed poprawką (przycięty podtytuł, 1100×734)
- `release/m14-doctor-wayland.png` — Doctor w realnej sesji Wayland

W czasie przebiegu 2 zrzucono też wariant, w którym kreator jest modalnie nad widokiem Doctora — to zachowanie zgodne z projektem (kreator blokuje pracę do decyzji użytkownika), a `Ctrl+7` pod nim poprawnie przełącza moduł.

**Czego nie sprawdzono wizualnie:** kliknięć myszą w poszczególne przyciski kreatora (brak narzędzia do syntetycznego wskaźnika w sesji; użyto skrótu klawiszowego i QSettings), wyboru modelu przez `FileDialog`, kopiowania raportu do schowka oraz wyglądu przy skali HiDPI innej niż bieżąca. Ścieżki te są pokryte testami QtTest (`firstRunWizardFlow`, `doctor*`) w trybie offscreen.

---

## 6. Uwagi i znaleziska (poza zakresem M14)

- **R1 — testy QtTest wrażliwe na środowisko.** Przy ustawionym `VIRTUAL_ENV`/`AUDIOTICON_PIPER_MODEL` dwa testy przepływu zadań dają inne wyniki (mock Pipera kontra realny). Testy nadal trzeba uruchamiać w czystym env; docelowo warto je rozdzielić.
- **R2 — CLI i SIGPIPE.** `audioticon models list | head` kończy się paniką 101 (domyślna obsługa SIGPIPE w Ruście). Nie dotyczy GUI ani doctora.
- **R3 — Doctor: tekst ścieżki modelu w kafelku 5.** Podtytuł błędu w `DoctorView.qml` ma zahardkodowane `~/.local/share/audioticon/models`, podczas gdy druga linia pokazuje realną ścieżkę z `XDG_DATA_HOME`. Pod XDG override tekst bywa mylący. Poprawka to użycie `activeDoctor.xdgModelsDir` w `qsTr`-owym komunikacie — nie zrobiona w M14, aby nie rozszerzać odświeżania bundle o zmiany UX.
- **R4 — brak wpływu na bundle.** `manifest.json` nie zawiera jeszcze sum kontrolnych `bin/*` w formie umożliwiającej jednokomendowe `sha256sum -c`; sumy są w polach `sha256` (można wygenerować plik kontrolny prostym `jq`/Pythonem).
- **R5 — glibc 2.44.** Binarki są dynamiczne względem systemowego glibc; na starszym glibc (np. 2.39) nie wystartują. Bez zmian względem M12.

---

## 7. Ograniczenia i rzeczy niezweryfikowane

1. **Czysta maszyna bez Qt** — nie testowano; doctor i launcher mają ścieżki `FAIL`, pokryte testem `qmake-shim` w M12, ale nie na realnym systemie bez Qt.
2. **Starszy glibc / inna dystrybucja** — nie testowano (R5).
3. **HiDPI i inne rozdzielczości** — tylko bieżąca sesja 1896×1030 (skala 1) oraz okno 1100×734 przed poprawką.
4. **Realna synteza głosu z GUI w tej sesji** — nie uruchamiana; sprawdzono tylko stan „brak modelu” (ścieżka First Run/Doctor), a pełny pipeline TTS jest pokryty testami Rust (130 testów core, m.in. z realnym Piperem) i QtTest.
5. **Interakcje myszą** — patrz sekcja 5 (brak narzędzia do syntetycznych kliknięć; użyto skrótów klawiszowych).

---

## 8. Zgodność z wymaganiami zadania

| # | Wymaganie | Status |
|---|---|---|
| 1 | Zbuduj Rust release | ✅ `cargo build --release --locked --workspace`, exit 0 |
| 2 | Zbuduj Qt UI release z First Run i Doctor | ✅ `DoctorModel`/QML M13 w binarce (`FirstRunWizard`, `DoctorView` w `qrc`) |
| 3 | Odśwież `bin/audioticon-studio` | ✅ 8 899 032 B, `5e481f85…e3befa` |
| 4 | Odśwież doctor.sh, launcher, manifest i SHA-256 | ✅ nowe kontrole + manifest z sumami każdego pliku |
| 5 | Zachowaj licencje i README | ✅ `LICENSE-MIT`, `LICENSE-APACHE`, `THIRD_PARTY_NOTICES.md`, `README-RELEASE.md` (zaktualizowany o sekcję First Run/Doctor) |
| 6 | Nie dodawaj Qt/FFmpeg/Python/ORT/modeli | ✅ 9 plików, wszystkie `bundles_*: false` |
| 7 | Uruchom doctor | ✅ 0/0 (z modelem), 0/3 (bez), `--strict` → 2 |
| 8 | Uruchom `--probe-ffi` przez launcher | ✅ 44100 Hz, exit 0 |
| 9 | CLI smoke test | ✅ `--version`, `presets`, `models list`, `estimate` |
| 10 | Pełne testy Rust i QtTest | ✅ 179/179 i 32/32 |
| 11 | Uruchom aplikację w realnej sesji Wayland/X11 | ✅ Hyprland 0.56.2 / Wayland, okno 1896×1030 |
| 12 | Sprawdź First Run i Doctor wizualnie | ✅ 3 zrzuty, 1 defekt znaleziony i naprawiony |
| — | Nie modyfikuj oryginalnych repozytoriów | ✅ zmiana tylko w `ui/src/qml/FirstRunWizard.qml` (nasz kod) |
| — | Nie commituj | ✅ brak commitów |
| — | Nie zmieniaj plików w `Downloads` | ✅ |

---

## 9. Ścieżki

- Bundle: `/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/release/audioticon-local-linux`
- Manifest: `…/release/audioticon-local-linux/manifest.json`
- Zrzuty: `…/release/m14-firstrun-wayland.png`, `…/release/m14-firstrun-wayland-before-fix.png`, `…/release/m14-doctor-wayland.png`
- Logi budowy i testów: `/tmp/m14-make*.log`, `/tmp/m14-qmake*.log`, `/tmp/m14-cargo-test.log`, `/tmp/m14-qt-test2.log`
- Raport: `/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/M14_RELEASE_BUNDLE_REFRESH.md`
