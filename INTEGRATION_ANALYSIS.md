# Audioticon 2.0 — analiza integracji

Data analizy: 2026-11-18

## Źródła

- **Baza:** ../Audioticon-legacy — commit 5c07069, Rust workspace (core + CLI), licencja MIT OR Apache-2.0.
- **Dawca rozwiązań:** ../Portable-Local-Studio — commit 473debf, React/Vite/Tauri frontend + skrypty lokalnych workerów, licencja MIT.

Oba repozytoria zostały pobrane lokalnie i pozostają nietknięte. Ten katalog jest miejscem docelowej adaptacji.

## Pierwsze rozpoznanie

### Mocne strony Audioticon

- Dojrzały silnik domenowy w Ruście.
- Emoji Grammar / parser reżyserii audiobooka.
- Sentence-level SHA-256 cache i crash-resume.
- Wielu dostawców TTS: Piper, Kokoro, Edge, ElevenLabs.
- Normalizacja języka polskiego.
- DSP, akustyka, Foley/SFX, cadence i wielogłosowość.
- Import EPUB/DOCX, VoiceLab, ledger ACX/EBU R128 i eksport audiobooków.
- Istniejący CLI oraz klasyczny Studio Workbench.

### Mocne strony Portable Local Studio

- Nowocześniejszy, modularny interfejs React 19 + Vite + Tauri API.
- Gotowe komponenty: TextToSpeech, SpeechTranscriber, ModelManager, Settings, status sprzętu i tekstowy chat.
- Lokalny, offline-first model uruchamiania workerów.
- Skrypty instalacyjne dla Linux/macOS/Windows oraz obsługa Piper/Kokoro/Whisper.
- Jednolity frontend dla wielu lokalnych silników AI.

## Wstępna decyzja architektoniczna

Nie kopiujemy Audioticon do Portable Local Studio ani odwrotnie. Najbezpieczniejszy kierunek to:

1. **Audioticon pozostaje rdzeniem domenowym** — parser, pipeline audio, cache, mastering, formaty eksportu i CLI.
2. **Frontend Portable Local Studio staje się kandydatem na nową warstwę Studio UI** — po przystosowaniu do API Audioticon zamiast duplikowania logiki audio.
3. **Integracja przez wyraźny kontrakt API** — status renderu, projekty, segmenty/cache, modele/głosy, podgląd waveformu, VoiceLab i eksport.
4. **Workerów nie łączymy mechanicznie** — najpierw ustalimy, które implementacje są lepsze i unikniemy dwóch konkurencyjnych ścieżek TTS.
5. **Modele, klucze BYOK i dane użytkownika pozostają lokalne**; nie przenosimy artefaktów modeli do repozytorium.

## Kandydaci do przeszczepu/adaptacji

- Nawigacja i układ Studio UI z Portable Local Studio.
- Komponenty zarządzania modelami, TTS i transkrypcją — po podpięciu do backendu Audioticon.
- Wzorzec lokalnych workerów i detekcji backendu.
- Konfiguracja platformowa oraz skrypty instalacyjne, po audycie bezpieczeństwa i licencji.
- Z Audioticon do nowego UI: emoji composer, segment cache inspector, VoiceLab, timeline, ledger i eksport.

## Rzeczy do sprawdzenia przed implementacją

- Dokładny kontrakt HTTP/IPC obecnego Audioticon Studio.
- Czy Tauri jest już tylko szkieletem, czy pełnym shell’em desktopowym.
- Duplikaty Piper/Kokoro/Whisper i różnice parametrów.
- Kompatybilność licencji oraz zachowanie wszystkich notice’ów MIT/Apache i third-party.
- Testy CLI, integracyjne i minimalny smoke test renderu bez pobierania dużych modeli.
- Docelowy zakres v2.0: desktop Tauri, web-only, czy oba tryby.

## Zasada pracy

Każdy przeszczep powinien mieć uzasadnienie: problem, źródło rozwiązania, kontrakt integracyjny, test regresyjny i decyzję dotyczącą licencji. Najpierw przygotujemy mapę modułów, dopiero potem będziemy zmieniać kod.

## Decyzje produktowe użytkownika

- Audioticon 2.0 ma być prawdziwą lokalną aplikacją desktopową oraz CLI. Nie budujemy webowego UI ani architektury opartej na serwerze przeglądarkowym.
- Zamiast wielkiego dashboardu projektujemy nawigację listową po lewej stronie oraz centralny chat/obszar roboczy, zależny od wybranego modułu.
- Sposób wykorzystania Portable Local Studio pozostaje otwarty do porównania: wybierzemy najlepszą opcję dopiero po analizie gotowego UI i kosztu integracji.
- Pierwszy pionowy wycinek i układ ekranów zostaną doprecyzowane po otrzymaniu linku do prawie gotowego UI.

## Trzecie źródło UI: ASTRA-UI-Template

- Repozytorium pobrane z gałęzi `agent-ui`, nie z `main`. Commit: `4252f4f`.
- Technologia: Qt 6 / Qt Quick / QML / C++ — zgodna z kierunkiem lokalnej aplikacji desktopowej, bez webowego UI.
- Najcenniejszy element: gotowy agent console z listą sesji po lewej, centralnym strumieniowanym transcript/chatem, śladami narzędzi oraz panelem runtime.
- To nie jest gotowy backend; `AgentModel` używa scenariusza demonstracyjnego. Wymienny jest kontrakt modelu, nie cała logika interfejsu.
- Weryfikacja: `./bin/build-agent` zakończyło się powodzeniem i zbudowało `build-agent/astra-agent-console`.
- Wniosek: ASTRA UI jest obecnie lepszym kandydatem na warstwę desktopowego UI Audioticon 2.0 niż webowy frontend Portable Local Studio. Portable Local Studio pozostaje dawcą wybranych workerów, kontraktów i logiki instalacyjnej — do dalszego porównania.

## Koncepcja prawego panelu

Prawy panel powinien być kontekstowym panelem narzędziowym, otwieranym na żądanie, a nie stałym dashboardem. Kandydackie sekcje:

- **Biblioteka / katalog wyjściowy** — wygenerowane WAV/FLAC/MP3/M4B, podgląd, odsłuch, metadane, certyfikaty i szybkie otwarcie lokalizacji.
- **Projekty i materiały** — książki, rozdziały, importowane EPUB/DOCX/TXT, ostatnie sesje i pliki robocze.
- **Modele i głosy** — modele TTS/STT/LLM, głosy, licencje, pochodzenie, dostępność i status pobrania.
- **Ustawienia** — dostawcy, ścieżki, język, audio, cache, skróty, prywatność i ustawienia aplikacji.
- **Kolejka i zadania** — renderowanie, postęp, błędy, możliwość wznowienia/anulowania oraz historia operacji.
- **Inspektor segmentu** — status cache, parametry głosu, waveform, wersje i dogrywki dla aktualnie wybranego zdania.
- **Diagnostyka** — ffmpeg, Piper/Kokoro, urządzenia audio, GPU/CPU/RAM i raport środowiska.

Panel powinien działać jako drawer/inspektor: szerokość regulowana, możliwość przypięcia, jedna sekcja aktywna naraz i zawartość zależna od bieżącego modułu.

## Decyzja zatwierdzona: Rust pozostaje rdzeniem

Użytkownik zaakceptował strategię: nie przepisujemy całego Audioticon do C++. ASTRA/QML będzie natywną powłoką desktopową, a istniejący rdzeń Rust zostanie zintegrowany przez bezpośredni bridge FFI/adapter C++. Zachowujemy CLI, algorytmy i kontrakty zachowania; C++ będzie warstwą hosta Qt, nie drugim silnikiem audio.

Kolejny etap: zaprojektować minimalny, stabilny kontrakt Rust ↔ Qt i dopiero potem połączyć pierwszy pionowy wycinek parser → render Piper → cache → odsłuch.

## M3 — pierwszy bridge i adapter Qt

- Izolowany backend roboczy: `AUDIOTICON 2.0/backend`; oryginalne repo pozostaje nietknięte.
- Rust bridge kompiluje się przez `cargo check --workspace`; testy bridge: 5/5 passed.
- UI worktree: `AUDIOTICON 2.0/ui`; adapter `StudioBridge` i `BridgeObserver` są podpięte do Qt/QML z mock renderem.
- Build ASTRA: sukces. Testy Qt: 17/17 passed przy `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software`. Bez wymuszenia offscreen test runner przerwał się kodem 134 z powodu środowiska graficznego, nie błędu testów.
- Do naprawy przed dalszym etapem: ostrzeżenia generowanego kodu cxx (`-Wmaybe-uninitialized`) oraz brak zapisanego raportu `M2_TOOLCHAIN_FFI_AUDIT.md`; nie blokuje to proof-of-concept, ale wymaga zamknięcia dokumentacyjnego.

## Audyt toolchainu M2 — zamknięty

Raport `M2_TOOLCHAIN_FFI_AUDIT.md` został zapisany i zweryfikowany. Potwierdza wykonalność cxx + staticlib + Qt 6/C++17 bez modyfikowania legacy core. Kluczowe ustalenia: ORT 1.24.2 nie obsługuje API 31; lokalne biblioteki ORT 1.32.0 obsługują je, ale są zależnością kruchą i nie mogą być podstawą dystrybucji. Tokio nie może blokować GUI; v1 powinien zwracać job_id i używać pollingu stanu co około 50 ms. Parser pozostaje po stronie C++ E1, a granica FFI zaczyna się na gotowych segmentach/syntezie, żeby nie utrzymywać dwóch parserów.

Następny test bramkowy M4: jedna funkcja `core_version() -> 44100` przechodząca przez cargo → staticlib → qmake → binarkę Qt. Bez modeli i bez pobierania ORT.

## M4 — dowód linkowania Rust → Qt zaliczony

`core_version() -> 44100` przeszedł pełny łańcuch Rust/cxx staticlib → qmake6 → binarka Qt. Niezależna próba `astra-agent-console --probe-ffi` zakończyła się kodem 0 i zwróciła `audioticon core sample rate: 44100 Hz`. Testy bridge: 6/6; testy Qt: 18/18; screenshot smoke test: OK. Artefakty debugowe są duże (staticlib około 207 MB, binarka około 87 MB), więc nie należy ich wersjonować; przed dystrybucją potrzebne będą release/strip. M4 nie pobiera modeli ani nie uruchamia ORT.

Kolejny etap M5: Tokio runtime w tle, rejestr jobów, atrapowy render ciszy 200 ms, polling stanu przez Qt `QTimer`, anulowanie zadania.

## M5 — specyfikacja układu UI

Specyfikacja `M5_UI_LAYOUT_SPEC.md` została zapisana i zweryfikowana. Przyjęty układ: lewa szyna sześciu modułów, centralny obszar pracy oraz prawy kontekstowy drawer/inspektor z siedmioma sekcjami. MVP ogranicza implementację do `StudioView.qml`, `NavRail.qml` i `ContextInspector.qml`, zachowując `feed`, `i18n` i niezależny `studio`.

Korekta względem skrótu raportu: tekst → dyrektywy pozostaje po stronie C++ E1, zgodnie z decyzją o jednym parserze. FFI nie powinien ponownie wystawiać `parseDirectedText`; pierwszy bridge obsługuje syntezę, cache, status i PCM/WAV.

## M5 — plan implementacji UI zatwierdzony

`M5_UI_IMPLEMENTATION_PLAN.md` istnieje i został zweryfikowany. Implementacja MVP obejmuje `NavRail.qml`, `StudioView.qml`, `ContextInspector.qml`, bez naruszania istniejącego `transcriptView`, `feed` ani `i18n`. Plan przewiduje minimum 18 istniejących testów plus testy przełączania modułów, mock-renderu i integracji segmentów. Kod UI nie jest jeszcze zmieniany, aby nie kolidować z trwającym M5 runtime Freebuffa.

## M5 — runtime jobów zaliczony

M5 połączył prawdziwy Rust bridge z `StudioBridge`: jeden tokio runtime w tle, rejestr jobów, `start_job`, `job_state`, `cancel_job`, polling Qt co 50 ms i atrapowy WAV 200 ms. Weryfikacja niezależna: raport istnieje, QtTest **20/20 passed**, w tym start/progress/finish i cancel, a buildy aplikacji i testów są dostępne. Nie ma jeszcze zmian QML, prawdziwego Piper ani modeli. Następny etap to M6: podpięcie trzech widoków UI do działającego `StudioBridge`.

## M6 — UI MVP zaliczone

M6 wdrożyło NavRail, StudioView, ContextInspector, przełączanie Studio/Czat, akcję „Przeczytaj w studiu”, skróty i prawy drawer. Weryfikacja niezależna: `M6_UI_IMPLEMENTATION.md` istnieje, build jest dostępny, a QtTest w trybie offscreen zakończył się wynikiem **23/23 passed**. Screenshot `/tmp/audioticon-m6-preview.png` potwierdza układ lewej szyny, centralnego warsztatu, prawego panelu i dolnego StateStrip bez oczywistych błędów renderowania.

## M7 — Piper dla jednego segmentu zaliczony

M7 znalazło lokalny model `pl_PL-bass-high.onnx` (114 MB), nie pobierając niczego. Rust bridge wykonuje realną syntezę jednego segmentu przez Piper, zapisuje WAV i obsługuje cache hit; fallback mock pozostaje dostępny bez modelu. Weryfikacja: `cargo test -p audioticon-bridge` **12/12 passed**, QtTest **23/23 passed**. Ograniczenie na kolejny etap: `outputPath` jest publikowany, ale odtwarzanie przez `QAudioSink` nie jest jeszcze podłączone. Build nadal emituje ostrzeżenia z generowanego glue cxx `-Wmaybe-uninitialized`; nie blokują testów, ale trzeba je ograniczyć przed release.

## M8 — odtwarzanie audio zaliczone

M8 dodało `AudioPlayer` oparty o `QAudioSink` i `QBuffer`, parser WAV, play/pause/resume/stop/seek, pasek transportu oraz odsłuch segmentów. Weryfikacja niezależna: raport istnieje, build aplikacji działa, a QtTest w trybie offscreen zakończył się wynikiem **24/24 passed**, w tym `audioPlayerPlaybackCycle`. Pełny przepływ pojedynczego segmentu jest teraz dostępny: tekst → Piper → cache → WAV → odtwarzanie.

## M9 — kierunek premium UI zatwierdzony

`M9_UI_VISUAL_DIRECTION.md` został zapisany i zweryfikowany. Najważniejsze decyzje przed implementacją: usunąć duży LLM Governor ze Studio, odzyskać przestrzeń dla timeline, naprawić kolizję edytora z paletą emoji, zastąpić surowe HIT/MISS płytkami LED, dodać mastering audio strip LUFS/True Peak, waveform/scrubber i branding Director Workstation. Implementację polish UI odkładamy do momentu przełączenia Antygravity na najlepszy model.

## M9 — pipeline rozdziału zaliczony

M9 dodało wielosegmentowy render rozdziału, cache hit/miss, anulowanie, łączenie WAV i ponowny render po zmianie jednego zdania. Weryfikacja niezależna: raport istnieje, `cargo test -p audioticon-bridge` **13/13 passed**, QtTest **24/24 passed**, build aplikacji działa. Nadal pozostają ostrzeżenia generowanego glue cxx oraz brak release packaging; funkcjonalny pipeline rozdziału jest gotowy.

## M9 — premium UI wdrożone

M9 Premium UI zostało wdrożone w roboczym UI. Usunięto LLM Governor ze Studio, naprawiono separację edytora i palety emoji, dodano Mastering Strip LUFS/Peak, LED HIT/MISS, mini-waveformy i waveform scrubber oraz branding Director Workstation. Weryfikacja niezależna: raport istnieje, build działa, QtTest **24/24 passed**, a `/tmp/audioticon-m9-preview.png` potwierdza spójny układ premium bez oczywistych błędów renderowania.

## Plan tapet i przezroczystości Glass

W `~/Downloads` znaleziono 15 kandydatów: 8 filmów MP4 oraz 7 obrazów statycznych. Tapety dodamy jako opcjonalną warstwę tła za szkłem, nie jako element dominujący. Ustawienia powinny obejmować: tapeta wyłączona/statyczna/filmowa, przezroczystość szkła, opacity tapety, przyciemnienie, blur, nasycenie, winietę oraz pauzę animacji.

Duże MP4 (około 15–233 MB) nie powinny trafić do repozytorium ani binarki. Będą lokalnymi opcjonalnymi assetami użytkownika; do dystrybucji można później przygotować lekkie preview. Statyczne obrazy można skopiować do `ui/assets/wallpapers/` po wyborze zestawu i sprawdzeniu praw do redystrybucji.

## M10 — tapety Glass i przezroczystość zaliczone

M10 dodało natywne tapety statyczne i filmowe, tryby none/static/video/rotate, QSettings dla opacity/glass/blur/overlay/saturation/vignette/reduced motion oraz wybór lokalnych assetów. Skopiowano tylko dwa statyczne obrazy; duże MP4 pozostają poza repo i są wykrywane lokalnie. Weryfikacja niezależna: raport istnieje, trzy screenshoty 780×560, 1180×850 i 1920×1080 są dostępne, a QtTest zakończył się wynikiem **25/25 passed**. Screenshot standardowy potwierdza subtelną tapetę pod czytelnym szkłem.

## Audyt release M10-B — blocker list

Raport `M10_RELEASE_AUDIT.md` istnieje i zawiera pomiary release. Stan: Rust workspace **174/175** (jedyny fail: eksport FLAC na FFmpeg 9), Qt **25/25**. Twarde blokery przed pierwszym pakietem: B1 filtr FLAC/loudnorm, B2 ochrona przed pustym lokalnym `cxx.h`, B3 `rustlink.pri` nadal linkuje debug zamiast release, B4 brak deploymentu Qt, B5 brak Pythona/piper-tts w paczce, B6 brak modelu w paczce lub jawnego instalatora. Miękkie: ORT, FFmpeg/GPL, pakowanie, pinowanie toolchaina, warningi glue i rozdzielenie testów środowiskowych. Nie wykonujemy jeszcze bundlowania ani zmian licencyjnych.

## M11 — techniczne blokery release usunięte

M11 naprawiło eksport FLAC/MP3/M4B dla FFmpeg 9, linkowanie release Rust staticlib, preflight pustego `cxx.h` oraz fałszywy sukces fallbacku bez modelu. Niezależna weryfikacja: Rust `cargo test --workspace --no-fail-fast` **179 passed, 0 failed**, QtTest **27/27 passed**. Release binarki UI mają około 16 MB zamiast 89 MB debug. Pozostały decyzje dystrybucyjne B4–B8: deployment Qt, Python/Piper, model, ORT i FFmpeg/licencje.

## M12 — release UX opisane

`M12_RELEASE_UX_SPEC.md` został zapisany i zweryfikowany. Specyfikacja obejmuje first-run wizard, Doctor, brakujące zależności, wybór modelu, tryb inspekcji bez TTS, informacje licencyjne, lokalność/offline i brak fałszywego Finished. Implementację UX wykonamy dopiero po ukończeniu technicznego bundle M12, aby nie kolidować z jego ścieżkami i manifestem.

## M12 — lokalny release bundle zaliczony

Bundle Omarchy/Linux istnieje w `release/audioticon-local-linux/`, zawiera 9 plików i około 18,5 MiB po strip. Zawiera CLI, Studio, doctor, launcher, licencje i manifest; nie zawiera Qt, FFmpeg, Python, ORT, modeli ani build artifacts. Niezależna weryfikacja: `doctor.sh` działa z 0 błędów i 3 oczekiwanymi ostrzeżeniami opcjonalnych funkcji, a `--probe-ffi` zwraca 44100 Hz. Następny etap: implementacja First Run/Doctor UX w aplikacji.

## M13 — First Run i Doctor UX zaliczone

M13 dodało natywny FirstRunWizard, pełny Doctor, diagnostykę Qt/audio/FFmpeg/Piper/modelu/ORT/XDG, tryb inspekcji bez TTS, wybór modelu i kopiowanie raportu. Weryfikacja niezależna: raport i dwa screenshoty istnieją, build release UI działa, a QtTest zakończył się wynikiem **32/32 passed**. Następny krok: przebudować bundle M12 z aktualnym UI i wykonać test smoke na realnej sesji Wayland/X11.

## M15 — reset kierunku i audyt funkcjonalny

Neon potwierdził 13 odstępstw produktowych i przygotował reset kierunku w `M15_PRODUCT_DIRECTION_RESET.md`. Freebuff potwierdził pomiarami: pionowy overflow First Run przy 780 px, martwy przełącznik EN, 198/244 brakujących tłumaczeń, częściową przezroczystość, brak UI wyboru tapety, 4/7 placeholderowych modułów oraz brak kontraktu FFI dla katalogu modeli, Whisper i lokalnego LLM. M15 zamknięte analitycznie; następny etap to M16 P0 UX/stabilność, bez finalnego visual pass.
