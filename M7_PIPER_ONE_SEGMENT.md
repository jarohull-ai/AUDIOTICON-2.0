# M7 — Raport: prawdziwa synteza Piper dla jednego segmentu

**Data utworzenia:** 2026-11-18  
**Środowisko robocze:** `/home/jaro/.openclaw/workspace/AUDIOTICON 2.0` (tylko `backend` + `ui`)  
**Stan oryginalnych repozytoriów:** `Audioticon-legacy` oraz `ASTRA-UI-Template` nietknięte.

---

## 1. Cel i zakres

Wdrożenie prawdziwej syntezy Piper (jeden segment) zamiast makiety 200 ms ciszy:

1. Znaleziono lokalny model `pl_PL-bass-high.onnx`.
2. Model nie był pobierany — użyto istniejącego pliku.
3. ORT 1.32.0 dostępny przez `ORT_DYLIB_PATH` (testowo, wzorzec z `kokoro.rs`).
4. Podłączono `PiperBridge` (audioticon-core) przez istniejący Rust bridge (`spawn_piper_render`).
5. Synteza jednego zdania do WAV/PCM.
6. Status joba + obsługa błędów.
7. Cache pojedynczego segmentu (hash tekstu → stabilna nazwa pliku, cache hit).
8. Ścieżka wynikowa dostępna dla odtworzenia przez `StudioBridge` (`outputPath`).

---

## 2. Model i środowisko

- **Model:** `/home/jaro/Portable-Local-Studio/app/tts-models/piper/pl_PL-bass-high.onnx`  
  (rozmiar 114 204 024 B, config `pl_PL-bass-high.onnx.json`)
- **Python z piper:** `/home/jaro/Portable-Local-Studio/app/tts-runtime/piper-venv`  
  (piper + espeakbridge.so + espeak-ng-data, bundled)
- **ORT 1.32.0:** `/home/jaro/.hermes/hermes-agent/venv/lib/python3.11/site-packages/onnxruntime/capi/libonnxruntime.so.1.32.0`  
  (dostępny przez `ORT_DYLIB_PATH`; używany testowo, Piper używa worker Python)
- **ffmpeg:** wymagany (konwersja PCM 44.1 kHz, 1 kanał).

Aktywacja: ustaw `VIRTUAL_ENV=/home/jaro/Portable-Local-Studio/app/tts-runtime/piper-venv`  
oraz `AUDIOTICON_PIPER_MODEL=/home/jaro/Portable-Local-Studio/app/tts-models/piper/pl_PL-bass-high.onnx`.

---

## 3. Zmiany w kodzie

### `backend/crates/bridge/src/lib.rs`

- `BridgeEngine` zyskał pole `piper: Option<Arc<PiperBridge>>` (None gdy brak modelu).
- `BridgeEngine::new()` rozwiązuje model przez `resolve_piper_model()` (env `AUDIOTICON_PIPER_MODEL` → `models_dir()`); brak modelu ⇒ fallback do makiety M5 (GUI i testy bez modelu działają).
- Nowa funkcja `spawn_piper_render(...)`:
  - etap Synthesizing, 1 segment,
  - cache hit po hashu tekstu (`piper_{hash}.wav`) — pomija synteze,
  - wywołanie `PiperBridge::synthesize` (realny Piper),
  - błędy → stage Failed + `error_message`,
  - anulowanie → usunięcie częściowego pliku.
- `start_job()` wybiera ścieżkę Piper gdy model dostępny, inaczej mock.
- Dodano `has_piper_model()` / `piper_model_path()` (diagnostyka/testy).
- Nowy test warunkowy `test_real_piper_synthesis_one_segment` (skip bez modelu/python).

### Zachowanie M5

- `spawn_mock_render` zachowany (fallback).
- 12 testów Rust: 11 z M5 + 1 nowy warunkowy (wszystkie przechodzą).
- Test `test_job_flow_queued_progress_finished` zgeneralizowano (rozmiar pliku > 44 B zamiast sztywnych 200 ms) — działa dla obu ścieżek.

### Qt

- Brak zmian wymaganych: `StudioBridge` polluje `outputPath` (już obsługiwane).
- 23 testy Qt przechodzą bez zmian.

---

## 4. Weryfikacja

- `cargo check --workspace` → exit 0.
- `cargo test -p audioticon-bridge` → **12 passed, 0 failed** (w tym realny Piper + cache hit).
- `qmake6 tests.pro && make` → exit 0.
- `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./tests` → **23 passed, 0 failed**.

---

## 5. Blockery / ograniczenia

- **Odtworzenie przez QAudioSink:** brak istniejącego `QAudioSink` w UI; ścieżka wynikowa (`outputPath`) jest publikowana i gotowa do podłączenia odtwarzacza w kolejnym etapie (M8). Nie zaimplementowano nowego odtwarzacza w ramach M7 (poza zakresem minimalnym).
- **ORT 1.32.0:** nie jest używany bezpośrednio przez Piper (worker Python). Wzorzec `ORT_DYLIB_PATH` z `kokoro.rs` pozostaje do testowania ORT; ewentualne użycie natywnego Rust Pipera (bez Pythona) to osobny etap.
- **Auto-detekcja modelu:** domyślnie brak (env-gated), aby testy bez modelu używały makiety. Produkcja wymaga ustawienia `AUDIOTICON_PIPER_MODEL`.
- Brak commitów (zgodnie z wymogiem).

---

## 6. Następny krok (M8)

- Auto-detekcja modelu + python w `models_dir`/`VIRTUAL_ENV`.
- Odtwarzacz QAudioSink w QML (przycisk ▶ na ścieżce `outputPath`).
- Natywny Rust Piper (ort + espeak-ng) bez subprocess, jeśli wymagane.
- Eviction cache + per-segment progress dla wielu segmentów.
