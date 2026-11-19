# M19 — UI Katalogu Modeli (Model Hub)

**Zakres:** natywny moduł **Model Hub / Katalog modeli** w Qt Quick / QML (`ui/`) osadzony w powłoce Director Workstation, działający wyłącznie nad istniejącym kontraktem backendowym M17 + M18 (bez zmian w Rust, FFI i `AudioticonCore`).

**Czego ten etap nie robi:** nie modyfikuje `backend/`, nie zmienia kontraktu FFI, nie uruchamia pobierania automatycznego ani w tle, nie wprowadza serwera HTTP ani WebView, nie dodaje nowych placeholderów, nie implementuje inferencji Whispera ani czatu LLM. Nie wykonano commita.

---

## 1. Zmienione i dodane pliki (`ui/`)

- [`ui/src/qml/ModelHubView.qml`](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/ModelHubView.qml) — główny moduł Model Hub (sekcje, filtry, pasek statusu aktywnego modelu, baner staging/postęp/anulowanie/weryfikacja, 4 modale Glass).
- [`ui/src/qml/ModelCard.qml`](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/ModelCard.qml) — karta modelu (wszystkie wymagane metadane, statusy i akcje w układzie `Flow`).
- [`ui/src/qml/NavRail.qml`](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/NavRail.qml) — pozycja `📦 Modele` (`Ctrl+8`).
- [`ui/src/qml/AgentMain.qml`](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/AgentMain.qml) — integracja widoku `"models"`, skrót `Ctrl+8`, klikalna pigułka aktywnego modelu w nagłówku (spójna z `studio.activeModelId`).
- [`ui/src/bridge/StudioBridge.h`](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/bridge/StudioBridge.h) / [`ui/src/bridge/StudioBridge.cpp`](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/bridge/StudioBridge.cpp) — rozszerzenie fasady QML (bez zmian w `AudioticonCore` ani Rust) o właściwości stanu importu (`importBusy`, `importStage`, `importMessage`, `importError`, `currentImportId`), `verifyModelDetailed`, `activeImportId`, `cancelActiveImport`, `startImportLocal`, `startDownloadRemote`, `resetImportState`, `urlToLocalPath` oraz licznik `incomplete` w `modelStatus()`.
- [`ui/src/resources.qrc`](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/resources.qrc) — rejestracja `ModelHubView.qml` i `ModelCard.qml`.
- [`ui/src/i18n/en.json`](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/i18n/en.json) / [`ui/src/i18n/pl.json`](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/i18n/pl.json) — 100% pokrycia tłumaczeń EN/PL (**503 / 503 klucze**).
- [`ui/tests/tests.cpp`](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/tests/tests.cpp) — 2 nowe testy integracyjne UI M19 + 4 zrzuty ekranu.

---

## 2. Realizacja wymagań M19

1. **Filtry typów modeli:** `Wszystkie` (`all`), `Piper / TTS` (`piper_tts`), `Whisper / ASR` (`whisper_asr`), `LLM` (`local_llm`), `Głosy` (`voice`).
2. **Sekcje źródeł:** `Polecane / Curated`, `Lokalne`, `GitHub` (`CONSENT`), `Hugging Face` (`CONSENT`).
3. **Karta modelu (`ModelCard.qml`):**
   - nazwa, typ, język, format, rozmiar, licencja, autor, status (`available`, `missing_metadata`, `corrupt`, `incomplete`, `not_installed`), znacznik aktywnego modelu (`AKTYWNY`), źródło, wymagany runtime (`Piper ONNX Runtime`, `whisper.cpp / GGML`, `llama.cpp (GGUF)`, `Audioticon Voice Profile`).
   - modele o statusie `corrupt`, `missing_metadata` i `incomplete` mają zablokowany przycisk `Wybierz aktywny` oraz wyróżnioną ramkę z komunikatem diagnostycznym.
4. **Akcje:** `Wybierz aktywny` / `Wyczyść aktywny`, `Szczegóły` (modal Glass z pełnymi metadanymi z `inspectModel`), `Importuj plik / katalog` (modal z `FileDialog` + `FolderDialog`), `Importuj ZIP` (modal z filtrem `*.zip`), `Sprawdź` (`verifyModelDetailed` + baner SHA-256), `Usuń` (modal potwierdzenia Glass), `Dodaj z GitHub / Hugging Face` (inspekcja offline → modal zgody).
5. **Modal Glass jawnej zgody (`remoteConsentModal`):**
   - prezentuje źródło, autora, licencję, rozmiar, pełną listę plików z URL i rozmiarem, sumę kontrolną SHA-256 (lub informację o obliczeniu po pobraniu), listę blokerów oraz widoczne ostrzeżenie o połączeniu sieciowym.
   - przycisk `Potwierdź i pobierz` pozostaje zablokowany aż do jawnego zaznaczenia checkboxa zgody (oraz podania SHA-256 i rozmiaru dla źródeł spoza presetów).
6. **Postęp, anulowanie, staging i błędy:**
   - baner `importStatusBanner` pokazuje nieokreślony pasek postępu i przycisk `Anuluj import` podczas stagingu/walidacji.
   - sukces (`ZWERYFIKOWANO I ZAINSTALOWANO`) pojawia się wyłącznie wtedy, gdy backend zwróci `status == "completed"` **oraz** `modelStatus == "available"`.
   - anulowanie czyści stan/staging (w tym sprząta wyścig przy bardzo szybkim lokalnym imporcie) i prezentuje `IMPORT ANULOWANY`.
   - błędy (np. brak transportu sieciowego w domyślnym silniku) są wyświetlane jawnie bez fałszywego sukcesu.

---

## 3. Wyniki testów i zrzuty ekranu

- **QtTest (`ui/build-tests/tests`, offscreen):** **40 passed, 0 failed, 0 skipped, 0 blacklisted (18 011 ms)** — wszystkie 38 istniejących testów + 2 nowe testy M19:
  - `Tests::modelHubUiEmptyCuratedAndConsentModal` — pusty katalog, presety curated bez pobierania i bez `.staging`, filtry typów, modal jawnej zgody (blokada bez checkboxa, jawny błąd sieci po potwierdzeniu, brak fałszywego sukcesu).
  - `Tests::modelHubLocalStatesCancelAndScreenshots` — 4 stany lokalne (`available`, `corrupt`, `missing_metadata`, `incomplete`), blokada wyboru dla uszkodzonych/niekompletnych, aktywny model w nagłówku i pasku statusu, `verifyModelDetailed`, import anulowany bez pozostawienia modelu, zrzuty ekranu.
- **Build `astra-agent-console`:** sukces (0 błędów, 0 ostrzeżeń).
- **Weryfikacja `backend/`:** 0 zmodyfikowanych plików.
- **Zrzuty ekranu:**
  - [`/tmp/audioticon-m19-modelhub-780x560.png`](file:///tmp/audioticon-m19-modelhub-780x560.png)
  - [`/tmp/audioticon-m19-modelhub-1180x850.png`](file:///tmp/audioticon-m19-modelhub-1180x850.png)
  - [`/tmp/audioticon-m19-modelhub-1920x1080.png`](file:///tmp/audioticon-m19-modelhub-1920x1080.png)
  - [`/tmp/audioticon-m19-consent-1180x850.png`](file:///tmp/audioticon-m19-consent-1180x850.png)
