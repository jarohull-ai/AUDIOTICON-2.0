# M17 — Lokalny katalog modeli i kontrakt FFI

**Zakres:** fundament P1 z `M15_FUNCTION_GAP_AUDIT.md` — wspólny, offline katalog
modeli Piper TTS / Whisper (ASR) / lokalnych LLM / przyszłych modeli głosów oraz
jeden kontrakt FFI do Qt (list / inspect / select / status / clear).

**Czego ten etap nie robi:** nie uruchamia Whispera ani inference LLM, nie rusza
VoiceLab ani klonowania głosu, nie zmienia wyglądu UI. Żadnego HTTP, `curl` ani
pobierania modeli — `models::download_model` pozostaje osobnym, jawnie wywoływanym
API i nie jest wołane z katalogu ani z FFI.

---

## 1. Architektura

```
backend/crates/core/src/model_catalog.rs      ← nowy moduł rdzenia (jedno źródło prawdy)
backend/crates/core/src/lib.rs                ← re-eksport typów katalogu
backend/crates/bridge/src/lib.rs              ← 5 funkcji FFI + DTO + podmiana aktywnego Pipera
ui/src/bridge/AudioticonCore.{h,cpp}          ← jedyny C++ znający cxx (CoreSession::*Model*)
ui/src/bridge/StudioBridge.{h,cpp}            ← fasada QML (models/modelStatus/inspectModel/selectModel/clearModelSelection)
ui/tests/tests.cpp                            ← test kontraktu przez realny staticlib
```

Rust pozostaje rdzeniem domenowym: katalog, klasyfikacja dostępności i persystencja
wyboru żyją w `audioticon-core`. Parser dyrektyw nadal jest po stronie C++.
Nie pojawił się żaden wątek HTTP/WebView.

### Katalog na dysku (XDG)

`ModelCatalog::for_current_user()` → `crate::models::models_dir()`, czyli:

1. `$XDG_DATA_HOME/audioticon/models`, gdy `XDG_DATA_HOME` jest ustawione,
2. w przeciwnym razie `~/.local/share/audioticon/models`.

Podkatalogi są rozdzielone per rodzaj (tworzone przez `ensure_dirs()`):

```
models/
  piper/    *.onnx + *.onnx.json          (Piper TTS)
  whisper/  *.bin + *.json  lub  <nazwa>/model.bin + config.json
  llm/      *.gguf                        (lokalne LLM)
  voices/   *.json                        (przyszłe modele głosów)
```

Plik wyboru leży **obok** katalogu modeli, więc skan nigdy nie zwraca go jako modelu:

```
audioticon/model_selection.json
```

`ModelCatalog::new(root)` pozwala wskazać dowolny root (testy, bundle przenośny).
Katalog o jawnym rootcie **nie** korzysta z `AUDIOTICON_PIPER_MODEL` ani ze starych
płaskich lokalizacji — inaczej globalny model dewelopera „przeciekałby” do
odizolowanego katalogu (dokładnie taki wyciek złapały testy w trakcie prac).

## 2. Stabilne metadane

`CatalogModel` (Rust) / `ModelInfo` (C++) / `ModelInfoDto` (FFI):

| pole | znaczenie |
| --- | --- |
| `id` | stabilne, niezależne od kolejności skanu: `piper/pl_PL-darkman-medium`, `whisper/faster-whisper-small`, `llm/qwen2.5-3b-instruct-q4`, `voices/narrator` |
| `kind` | `piper_tts` / `whisper_asr` / `local_llm` / `voice` |
| `name` | czytelna nazwa wyprowadzona ze stabilnego id |
| `path` | bezwzględna ścieżka pliku wag (dla `voices` — plik JSON) |
| `format` | `onnx` / `gguf` / `bin` / `json` |
| `language` | z sidecara (`language.code`, `language` jako string/lista), dla Piper dodatkowo z nazwy pliku (`pl_PL-…`); puste = nieznany |
| `size_bytes` | rozmiar pliku wag z `fs::metadata` |
| `license` | z sidecara, gdy podana; puste = nieznana (nie zgadujemy) |
| `status` | `available` / `missing_metadata` / `corrupt` |
| `checksum_sha256` | **opcjonalny**: tylko gdy obok leży `<model>.sha256` |
| `detail` | czytelny powód statusu (pusty dla `available`) |

### Zasady statusu (bez fałszywego sukcesu)

* `corrupt` — plik wag nieczytelny, pusty (0 B), obcięty (< 1 KiB) albo niezgodny
  z opcjonalnym checksumem SHA-256,
* `missing_metadata` — wagi są, brakuje konfiguracji: `.onnx.json` dla Piper,
  `config.json`/`<nazwa>.json` dla Whispera,
* `available` — komplet plików i spójny checksum (jeśli podany).

GGUF jest wyjątkiem: metadane ma w środku pliku, więc brak sidecara nie jest
błędem — `language`/`license` pozostają puste. Plik `voices/*.json` musi być
poprawnym JSON-em, inaczej `corrupt`.

Checksum nie jest liczony przy każdym skanie: SHA-256 liczy się **tylko** wtedy,
gdy istnieje sidecar `.sha256`. Duże pliki GGUF nie są hashowane bez powodu.

## 3. Kontrakt FFI (jeden, dla wszystkich rodzajów modeli)

```rust
// backend/crates/bridge/src/lib.rs (extern "Rust")
fn list_models(engine: &BridgeEngine) -> Vec<ModelInfoDto>;
fn inspect_model(engine: &BridgeEngine, id: &str) -> Result<ModelInfoDto>;
fn select_model(engine: &BridgeEngine, id: &str) -> Result<ModelInfoDto>;
fn model_status(engine: &BridgeEngine) -> ModelStatusDto;
fn clear_model_selection(engine: &BridgeEngine) -> Result<()>;
```

`ModelStatusDto` niesie: `catalog_root`, `root_exists`, `total`, `available`,
`unavailable`, `active_id`, `active_kind`, `active_path`, `active_status`,
`piper_model_path`, `message`.

Po stronie Qt ten sam kontrakt to `audioticon::ui::CoreSession`:

```cpp
QVector<ModelInfo> listModels() const;
std::optional<ModelInfo> inspectModel(const QString &id) const;
std::optional<ModelInfo> selectModel(const QString &id, QString *errorOut = nullptr);
ModelCatalogStatus modelStatus() const;
bool clearModelSelection(QString *errorOut = nullptr);
```

oraz fasada QML w `StudioBridge` (`models()`, `modelStatus()`, `inspectModel(id)`,
`selectModel(id)`, `clearModelSelection()`, `activeModelId`, `modelError`,
sygnał `modelSelectionChanged`). QML nie został zmieniony — to tylko wystawienie
kontraktu, bez finalnego wyglądu.

### Semantyka wyboru

* `select_model` przyjmuje **wyłącznie** model `available`; `corrupt` i
  `missing_metadata` kończą się błędem i nie zmieniają poprzedniego wyboru.
* Wybór jest zapisywany do `model_selection.json` i **odczytywany z powrotem**;
  brak potwierdzenia odczytu = błąd zapisu (nie ma „sukcesu bez utrwalenia”).
* `clear_model_selection` usuwa plik wyboru i ponownie rozwiązuje silnik Pipera
  z tego, co katalog faktycznie oferuje.
* `model_status().active_status` jest `unavailable`, gdy zapisany wybór wskazuje
  na plik, który zniknął — UI dostaje jawny błąd, a nie ciszę.

## 4. Piper flow i B13

* Rozwiązywanie modelu Pipera: `AUDIOTICON_PIPER_MODEL` → zapisany wybór →
  pierwszy `available` w `models/piper/` → stare płaskie lokalizacje
  (`models/pl_PL-*.onnx`, tylko dla katalogu użytkownika).
* Wybór modelu `piper_tts` podmienia aktywny `PiperBridge` (pole pod `Mutex`,
  bo cxx daje tylko `&BridgeEngine`).
* Bez modelu `start_job` nadal rejestruje zadanie i kończy je `Failed` z
  „Brak modelu głosu” — **nigdy** `Finished`. Mock wyłącznie przez
  `AUDIOTICON_MOCK_RENDER=1/true` (lub jawny `new_mock()` w testach).

## 5. Testy

Rust (`cd backend && cargo test --workspace`): **195 passed, 0 failed**
(19 bridge + 34 cli + 142 core; w tym 12 nowych katalogu i 4 nowe bridge).

Katalog (`model_catalog.rs`):
* pusty katalog → 0 modeli, brak aktywnego wyboru, jasny komunikat,
* poprawny model Piper → stabilne id/metadane/rozmiar/język/licencja, deterministyczny skan,
* uszkodzony plik (0 B, obcięty, zły checksum) → `corrupt`, nie da się wybrać,
* brak metadanych → `missing_metadata` (odrębny od `corrupt`),
* wybór modelu → zapis, odczyt przez nową instancję, nadpisanie poprzedniego,
* persystencja → `model_selection.json`; czyszczenie usuwa plik,
* wybór nieistniejącego modelu nie psuje poprzedniego wyboru,
* usunięcie wybranego modelu → `active` puste, `selected` zostaje, komunikat o niedostępności,
* Whisper/LLM/voices w jednym katalogu, osobne podkatalogi, brak sieci w źródle modułu.

Bridge: pusta lista + `unavailable`, wybór aktywuje i utrwala Pipera, odrzucenie
`corrupt`/`missing_metadata`, B13 (`Failed`, nie `Finished`), zniknięcie wybranego pliku.

Qt (`ui/build-tests/tests`, offscreen): **37 passed, 0 failed** — nowy
`Tests::modelCatalogFfiContract` sprawdza przez realny staticlib: pusty katalog,
`ModelCatalogStatus`, odrzucenie nieistniejącego/uszkodzonego modelu, brak
fałszywego `Finished` bez modelu, wybór poprawnego modelu, persystencję w nowej
sesji i czyszczenie wyboru. Zmienne `XDG_DATA_HOME`/`AUDIOTICON_MOCK_RENDER` są
przywracane przez `ScopedEnv` nawet przy nieudanej asercji.

## 6. Czego nie weryfikowano

* Nie uruchamiano prawdziwego Whispera ani LLM — w tym etapie nie ma inference,
  testy dowodzą tylko wykrywania i klasyfikacji plików.
* Nie klikano UI: katalog nie ma jeszcze ekranu; QML nietknięty.
* Nie testowano na modelu GGUF > 1 GiB (świadomie brak hashowania bez sidecara).
* Nie zmieniano bundla ani `release/`; nie wykonano żadnego commita.

## 7. Następne kroki (poza M17)

1. Ekran „Modele” w UI korzystający z `StudioBridge.models()/selectModel()`.
2. Manifest checksum publikowany razem z modelami (żeby `.sha256` był regułą, nie opcją).
3. Whisper/LLM jako konsumenci tego samego katalogu (bez zmian kontraktu FFI).
