# M18 — Katalog modeli: presety + lokalny + GitHub/Hugging Face

**Zakres:** rozszerzenie fundamentu z `M17_MODEL_CATALOG_FFI.md` o trzy sposoby
dodawania modeli (presety kurowane, import lokalny, pobranie zdalne na żądanie),
wraz z walidacją, stagingiem, anulowaniem, weryfikacją checksumu i usuwaniem.
Kontrakt FFI z M17 (list / inspect / select / status / clear) pozostał bez zmian.

**Czego ten etap nie robi:** nie dodaje UI, nie implementuje inference Whispera
ani LLM, nie uruchamia VoiceLab. **Nie pobrano żadnego modelu ani zależności.**
Nie wykonano commita.

---

## 1. Architektura

```
backend/crates/core/src/model_sources.rs   ← opis źródeł, presety, dozwolone hosty (bez sieci)
backend/crates/core/src/model_import.rs    ← import lokalny, staging, transport, anulowanie, verify, remove
backend/crates/core/src/model_catalog.rs   ← + status `incomplete`, `.staging`, pliki modelu
backend/crates/bridge/src/lib.rs           ← 9 nowych funkcji FFI + DTO
ui/src/bridge/AudioticonCore.{h,cpp}       ← jedyny C++ znający cxx (nowe struktury + metody)
ui/src/bridge/StudioBridge.{h,cpp}         ← fasada QML (Q_INVOKABLE, bez ekranu)
ui/tests/tests.cpp                         ← test kontraktu przez realny staticlib
```

Podział odpowiedzialności jest celowy: `model_sources` tylko **opisuje**, co
można pobrać; `model_import` jako jedyny wykonuje transfer i operacje na
plikach; `model_catalog` pozostaje jedynym źródłem prawdy o stanie dysku.

### Katalog na dysku

```
<XDG_DATA_HOME>/audioticon/
  models/
    piper/  whisper/  llm/  voices/     ← modele gotowe
    .staging/<import>/intent.json       ← importy w toku (status `incomplete`)
  model_selection.json                  ← wybór z M17
```

Katalog roboczy leży **w** `models/`, ale jako katalog z kropką — skan czyta
tylko pliki w podkatalogach rodzajów, więc `.staging` nigdy nie jest modelem.
`intent.json` jest czytany przez katalog, dzięki czemu przerwany import (także
po restarcie aplikacji) jest widoczny jako `incomplete`, a nie jako „brak modeli”.

## 2. Trzy źródła, jeden kontrakt

| źródło | `kind` | sieć | zgoda | zachowanie |
| --- | --- | --- | --- | --- |
| Presety kurowane | `curated` | nie (samo listowanie) | tak (przy instalacji) | modele znane z Portable Local Studio, tylko opis |
| Import lokalny | `local` | nie | nie | plik / katalog / ZIP, walidacja przed zapisem |
| GitHub Releases | `github_release` | tak, tylko w `download_remote` | tak | konkretny asset wydania |
| Hugging Face | `hugging_face` | tak, tylko w `download_remote` | tak | repo/plik (`resolve/<rev>/<path>`) |

### Presety (zachowane z Portable Local Studio)

`setup-piper.sh` → 5 głosów Piper (PL: gosia, darkman, mc_speech; EN: lessac,
amy), `setup-whisper.sh` → `ggml-base`, `ggml-small`, `setup-llama.sh` → lokalny
LLM Qwen2.5-3B-Instruct Q4_K_M. Każdy preset niesie: `id`, rodzaj, nazwę, autora,
licencję, język, format, rozmiar, `source_reference` (`repo@rewizja`), `origin`
(„Portable Local Studio”) i listę plików z adresami.

Presety **nie deklarują SHA-256**, którego producent nie publikuje — nie
wymyślamy sum. Po pobraniu liczymy własną sumę i zapisujemy ją jako
`<wagi>.sha256`, więc `verify_model` ma punkt odniesienia.

### Import lokalny

* wejście: plik, katalog (do 4 poziomów) albo ZIP,
* dozwolone rozszerzenia per rodzaj: `.onnx`/`.json`, `.bin`/`.json`, `.gguf`,
  `.json`, plus `.sha256`,
* odrzucane: skrypty i binaria (`py`, `sh`, `bat`, `exe`, `dll`, `so`, `jar`,
  `js`, `ps1`, …), dowiązania symboliczne, zduplikowane nazwy,
* nagłówki: GGUF musi zaczynać się od `GGUF`, profil głosu musi być JSON-em,
  plik zaczynający się od `#!`, `\x7fELF` lub `MZ` jest odrzucany,
* brak licencji wymaga **jawnej** akceptacji (`accept_unknown_license`),
* checksum: jeśli obok wag leży `<wagi>.sha256`, jest weryfikowany **przed**
  skopiowaniem,
* zapis jest atomowy: najpierw staging, potem przeniesienie; przy każdym błędzie
  staging jest usuwany, a katalog docelowy nietknięty.

### Pobranie zdalne

1. `inspect_remote_candidate(reference)` — **bez sieci** zwraca źródło, autora,
   licencję, rozmiar, format i listę braków (`blockers`).
2. `download_remote(reference, sha256, size, consent)` — wymaga `consent = true`.
   Domyślnie silnik **nie ma transportu**, więc kończy się jawnym
   `NetworkUnavailable` i nie tworzy nawet stagingu.
3. Po pobraniu: sprawdzenie rozmiaru (gdy podany), SHA-256 (gdy podany albo
   policzony), zapis sidecara, przeniesienie do `models/<kind>/`, usunięcie
   stagingu, ponowna inspekcja katalogu (`available` albo błąd i sprzątanie).
4. Anulowanie: `cancel_import(id)` ustawia flagę; transport przerywa zapis,
   staging jest usuwany, a wynik to `status = "cancelled"` (nie błąd).

Odwołania: `preset:<id>`, `hf:owner/repo@rev#plik`, `gh:owner/repo@tag#asset`,
pełny `https://` na dozwolonym hoście. Dozwolone hosty: `huggingface.co`,
`cdn-lfs*.huggingface.co`, `github.com`, `objects.githubusercontent.com`,
`release-assets.githubusercontent.com`; wyłącznie HTTPS, bez portów i `@`.

## 3. Statusy

| status | znaczenie |
| --- | --- |
| `available` | komplet plików, spójny checksum (jeśli jest) |
| `missing_metadata` | wagi są, brak konfiguracji (Piper `.onnx.json`, Whisper `config.json`) |
| `corrupt` | plik pusty/obcięty, zły checksum, niepoprawny JSON, nieczytelny plik |
| `incomplete` | import/pobranie w toku albo przerwane (`.staging`) |

`CatalogStatus` i `ModelStatusDto` mają teraz licznik `incomplete`. Żadnego z
tych statusów nie da się wybrać jako aktywnego modelu — `select_model` przyjmuje
wyłącznie `available`.

Wyjątek whisper.cpp: `ggml-*.bin` ma metadane w środku pliku, więc brak
`config.json` nie jest błędem (analogicznie do GGUF).

## 4. Kontrakt FFI

```rust
// backend/crates/bridge/src/lib.rs (extern "Rust")
fn list_sources(engine: &BridgeEngine) -> Vec<ModelSourceDto>;
fn list_curated_presets(engine: &BridgeEngine) -> Vec<CuratedPresetDto>;
fn inspect_remote_candidate(engine: &BridgeEngine, reference: &str) -> Result<RemoteCandidateDto>;
fn import_local(engine: &BridgeEngine, path: &str, kind: &str,
                accept_unknown_license: bool) -> Result<ImportResultDto>;
fn download_remote(engine: &BridgeEngine, reference: &str, expected_sha256: &str,
                   expected_size: u64, consent: bool) -> Result<ImportResultDto>;
fn cancel_import(engine: &BridgeEngine, import_id: u64) -> bool;
fn active_import_id(engine: &BridgeEngine) -> u64;   // uchwyt dla cancel z innego wątku
fn verify_model(engine: &BridgeEngine, id: &str) -> Result<VerifyResultDto>;
fn remove_model(engine: &BridgeEngine, id: &str) -> Result<RemoveResultDto>;
```

`ImportResultDto`: `import_id`, `status` (`completed`/`cancelled`), `model_id`,
`model_path`, `model_status`, `message`, `source`.
`VerifyResultDto`: `checked` (czy jest sidecar), `expected_sha256`, `actual_sha256`,
`matches`, `size_bytes`, `message`.
`RemoveResultDto`: `id`, `kind`, `removed_files`, `selection_cleared`.

Po stronie Qt: `audioticon::ui::CoreSession` (`listSources`, `listCuratedPresets`,
`inspectRemoteCandidate`, `importLocal`, `downloadRemote`, `cancelImport`,
`activeImportId`, `verifyModel`, `removeModel`) oraz `Q_INVOKABLE` w
`StudioBridge` (`modelSources`, `curatedPresets`, `inspectRemoteCandidate`,
`importLocalModel`, `downloadRemoteModel`, `cancelImport`, `verifyModel`,
`removeModel`) i sygnał `modelImportFinished(status)`. QML nietknięty — brak UI.

## 5. Bezpieczeństwo

* brak serwera HTTP i WebView; jedyne wyjście w sieć to `RemoteTransport`,
* domyślny silnik nie ma transportu → pobranie zawsze kończy się jawnym błędem,
* pobranie bez `consent = true` jest błędem `ConsentRequired` (transport nie jest
  nawet wołany),
* źródła spoza presetów wymagają zadeklarowanego SHA-256 **i** rozmiaru,
* nazwy plików: bez `/`, `\`, `..`, znaków kontrolnych, bez wiodącej kropki,
* ZIP: `enclosed_name` + odrzucenie każdego wpisu ze ścieżką; limit rozmiaru
  wpisu i liczby plików,
* pobrane/zaimportowane pliki nigdy nie są uruchamiane — w module nie ma
  `Command::new` (pilnuje tego test źródłowy),
* anulowanie i każdy błąd usuwają staging; `remove_model` czyści też wybór,
* `remove_model` przyjmuje wyłącznie `kind/nazwa` i usuwa pliki tylko z katalogu
  rodzaju.

## 6. Testy

Rust (`cd backend && cargo test --workspace`): **221 passed, 0 failed**
(23 bridge + 34 cli + 164 core). Wcześniej 195 → +26 (22 core, 4 bridge).

Core — `model_sources`: 4 źródła bez sieci dla curated/local, komplet presetów
Portable Local Studio, podgląd kandydata z autorem/licencją/rozmiarem, odrzucenie
niebezpiecznych odwołań, allowlista hostów, brak klienta sieci w źródle.

Core — `model_import`:
* presety bez pobierania (brak katalogu i stagingu),
* import lokalny Pipera + persystencja w nowej instancji,
* odrzucony zły format, skrypt w katalogu, zły nagłówek GGUF,
* checksum mismatch (lokalny i po pobraniu) → brak plików docelowych,
* path traversal w ZIP (`../` oraz ścieżka zagnieżdżona) → odrzucone,
* brak zgody = zero wywołań transportu,
* brak sieci = jawny błąd, brak stagingu,
* pobranie z transportem testowym → `available` + zapisany checksum + `verify`,
* anulowanie w trakcie (wątek + `cancel`) → `cancelled`, pusty katalog,
* źródło spoza presetów bez sumy/rozmiaru → odrzucone,
* nieznana licencja → wymaga jawnej akceptacji,
* `incomplete` z `intent.json` → widoczne, nie da się wybrać,
* `verify` + `remove` (czyszczenie wyboru, ochrona id).

Bridge: listowanie źródeł/presetów bez instalacji, import lokalny przez FFI i po
restarcie silnika, odrzucenie złego formatu, zgoda + brak sieci, źródło bez
checksumu, pobranie przez transport testowy + `verify`, mismatch i czysty katalog,
`cancel_import` dla nieznanego id.

Qt (`ui/build-tests/tests`, offscreen): **38 passed, 0 failed** (było 37).
Nowy `Tests::modelImportContract` sprawdza przez realny staticlib: 4 źródła,
preset z autorem/licencją/rozmiarem i `not_installed`, brak pobrania przy
listowaniu, podgląd kandydata, odrzucenie złego odwołania, brak zgody, brak
sieci, import lokalny + persystencja w nowej sesji, `verify`, path traversal w
`remove`, usunięcie modelu, odrzucenie `.txt` i niezgodnego checksumu.

## 7. Czego nie weryfikowano

* **Nie wykonano żadnego realnego pobrania.** Klient HTTPS nie został dodany
  (wymagałby nowej zależności); potok pobierania jest testowany transportem
  w pamięci. To świadoma granica etapu: domyślny silnik zwraca
  `NetworkUnavailable`, więc nie ma ryzyka cichego transferu.
* Nie testowano archiwów TAR/TAR.GZ (obsługiwany jest ZIP); PLS używa `.tar.gz`
  dla binariów whisper.cpp, ale binaria nie są modelami i nie są importowane.
* Nie sprawdzano modeli > 1 GiB (checksum liczony jest strumieniowo, więc pamięć
  nie rośnie, ale nie zmierzono czasu na dużym GGUF).
* Nie klikano UI — ekran „Modele” nie powstał; QML nietknięty.
* Nie zmieniano `release/` ani bundla; brak commitów.

## 8. Następne kroki (poza M18)

1. Klient HTTPS dla `RemoteTransport` (jedna zależność) + postęp procentowy.
2. Ekran „Modele” korzystający z `StudioBridge.modelSources()/curatedPresets()`.
3. Manifest checksum publikowany razem z presetami (żeby `.sha256` był regułą).
4. Obsługa TAR.GZ i podpisywanie manifestów źródeł.
