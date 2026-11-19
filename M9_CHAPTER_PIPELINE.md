# M9 — Raport: Pełny render rozdziału z cache segmentów

**Data ukończenia:** 2026-11-18  
**Środowisko robocze:** `/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/backend`, `/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/ui`  
**Oryginalne repozytoria:** nietknięte, brak commitów.

---

## 1. Cel i zakres etapu M9

Zaimplementowano pełny potok renderu rozdziału:

1. Render wielu segmentów w kolejce (podział tekstu na zdania).
2. Cache hit/miss dla każdego segmentu (SHA-256 z `audioticon_core::cache`).
3. Ponowne użycie istniejących WAV (`piper_{hash}.wav`).
4. Zachowanie kolejności segmentów i postęp całości (0–90% synteza, 95% łączenie, 100% gotowe).
5. Anulowanie całego renderu (flaga `cancel`, zatrzymanie między segmentami).
6. Bezpieczne połączenie segmentów w jeden WAV (`audioticon_core::stitch_wav_files`).
7. Publikacja `outputPath` rozdziału (`chapter_{job_id}.wav`).
8. Odtwarzanie całego rozdziału przez istniejący `AudioPlayer`/`StudioBridge::playOutput()`.
9. Ponowny render po zmianie tylko jednego zdania (pozostałe segmenty z cache).
10. Test: pierwszy render → zmiana jednego segmentu → drugi render z cache hitami.

Zachowano fallback mock bez modelu (M5, 200 ms ciszy).

## 2. Zmiany w kodzie

### `backend/crates/bridge/src/lib.rs`

- Dodano `split_chapter_segments(text)` — dzieli tekst na zdania (`.`, `!`, `?`, `\n`).
- Zastąpiono `spawn_piper_render` (jeden segment) funkcją `spawn_chapter_render`:
  - pętla po segmentach, synteza lub cache hit,
  - śledzenie `cache_hits`,
  - anulowanie między segmentami,
  - łączenie przez `stitch_wav_files`,
  - wynik: `chapter_{job_id}.wav`.
- `start_job` (ścieżka Piper) rejestruje zadanie z liczbą segmentów i wywołuje `spawn_chapter_render`.
- Dodano test `test_real_piper_chapter_pipeline_cache` (warunkowy, skip bez modelu).

## 3. Weryfikacja

### Rust (`audioticon-bridge`)

```bash
VIRTUAL_ENV=.../piper-venv AUDIOTICON_PIPER_MODEL=.../pl_PL-bass-high.onnx cargo test -p audioticon-bridge
```

- 13 testów, 0 błędów (w tym `test_real_piper_chapter_pipeline_cache`).
- `cargo check --workspace` exit 0.

### Qt / UI

```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./tests
```

- 24 testy, 0 błędów, 0 pominiętych.
- Build produkcyjny `astra-agent-console` exit 0.

## 4. Ścieżki i blokery

- Model: `/home/jaro/Portable-Local-Studio/app/tts-models/piper/pl_PL-bass-high.onnx`
- venv: `/home/jaro/Portable-Local-Studio/app/tts-runtime/piper-venv`
- ORT 1.32.0: `/home/jaro/.hermes/hermes-agent/venv/lib/python3.11/site-packages/onnxruntime/capi/libonnxruntime.so.1.32.0` (używany przez worker Pythona).
- Auto-detekcja modelu pozostaje env-gated (`AUDIOTICON_PIPER_MODEL`); w produkcji wymagane ustawienie zmiennej.

## 5. Ograniczenia

- Natywny Rust Piper (ort + espeak) nie został zaimplementowany (osobny etap).
- Eviction cache (LRU) nie podłączony do potoku rozdziału.
- Anulowanie usuwa tylko stan zadania; częściowe pliki segmentów mogą pozostać w cache (zamierzone — cache).
- `AudioPlayer` odtwarza wynikowy `outputPath` (cały rozdział) bez zmian w UI.

## 6. Podsumowanie

M9 zakończony. Pełny render rozdziału z cache segmentów działa z modelem `pl_bass`; test cache hit po zmianie jednego zdania przechodzi. Wszystkie 24 testy UI i 13 testów Rust przechodzą. Brak commitów.