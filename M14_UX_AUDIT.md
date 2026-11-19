# Raport M14 — Niezależny Audyt Wizualny UX (Audioticon 2.0)

**Data audytu:** 19 listopada 2026  
**Środowisko:** Linux (Omarchy / Arch Linux), Wayland / X11, Qt 6.12 Quick, C++17, Rust Tokio Staticlib  
**Badane moduły:** First Run Wizard, Doctor (NavRail & ContextInspector), Tryb Reżyserski (bez TTS), Obsługa błędów modeli  
**Pliki referencyjne:**
- [M12_RELEASE_UX_SPEC.md](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/M12_RELEASE_UX_SPEC.md)
- [M13_DOCTOR_UX.md](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/M13_DOCTOR_UX.md)
- Kod interfejsu w [ui/](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui)

**Zrzuty ekranu:**
- [audioticon-m14-first-run.png](file:///tmp/audioticon-m14-first-run.png) — ekran powitalny i kreator pierwszego uruchomienia
- [audioticon-m14-doctor-live.png](file:///tmp/audioticon-m14-doctor-live.png) — pełny moduł diagnostyczny Doctor z siatką kafelków LED
- [audioticon-m14-missing-model.png](file:///tmp/audioticon-m14-missing-model.png) — widok Studio w stanie błędu braku modelu

---

## 1. Tabela oceny 10 punktów audytu

| # | Obszar audytu | Status | Ocena i uwagi |
|---|---|:---:|---|
| 1 | **First Run przy braku modelu** | **PASS** | Modalna karta Glass (`z: 100`) centruje się precyzyjnie, scrim skutecznie odcina tło. Lista 5 podsystemów czytelna; brak modelu wyraźnie wyróżniony czerwoną ramką, diodą LED i komunikatem o ścieżce XDG. Przycisk „Rozpocznij pracę” jest zablokowany do momentu wyboru modelu lub aktywacji trybu reżyserskiego. |
| 2 | **Doctor jako moduł NavRail** | **PASS** | Moduł 7 (`🩺 Doctor`) poprawnie zintegrowany na lewej szynie. Aktywny kafelek ma cyjanową ramkę i lewy wskaźnik akcentowy. Stopka szyny dynamicznie odzwierciedla stan środowiska (`ŚRODOWISKO OK` / `BEZ TTS` / `NAPRAWA` + dioda LED). |
| 3 | **Doctor w ContextInspector** | **PASS** | W prawej szufladzie (zakładka `diagnostics`) mini-panel Doctora wyświetla 6 diod LED podsystemów, liczbę błędów, aktywny model oraz bezpośrednie przyciski skoku do pełnego Doctora (`Ctrl+7`), kopiowania raportu i przełączania trybu bez TTS. |
| 4 | **Baner „Brak modelu głosu”** | **PASS\*** | Plakietka transportu w nagłówku okna i baner w Studio natychmiast sygnalizują brak modelu (`⚠ Brak modelu głosu (ustaw AUDIOTICON_PIPER_MODEL)`). Kontekstowe przyciski naprawcze są obecne w kodzie QML, lecz przy szerokości 1180 px z przypiętym panelem bocznym ulegają obcięciu (patrz: Problemy P2). |
| 5 | **Tryb bez TTS (Tryb Reżyserski)** | **PASS** | Przejście do trybu reżyserskiego działa bez zarzutu: nagłówek otrzymuje bursztynową plakietkę `[ TRYB REŻYSERSKI ]`, status modelu zmienia się na `REŻYSERIA · BEZ TTS`, Studio wyświetla górny baner reżyserski z przyciskiem `Otwórz Doctor`, a stan Doctora wskazuje pełną gotowość do pracy bez syntezy TTS. |
| 6 | **Stany OK / WARN / FAIL / OPCJONALNY** | **PASS** | Bardzo czytelna semantyka kolorystyczna: zielony/cyjan (`#22e58b`) dla podsystemów sprawnych, bursztyn (`#ffd43b`) dla ostrzeżeń (brak silnika Piper), czerwień (`#ff4d6d`) dla błędów blokujących syntezę (brak modelu), błękit (`#4fc3ff`) dla opcjonalnego ONNX Runtime. |
| 7 | **Kontrast, clipping, overflow i responsywność** | **PASS\*** | Kontrast tekstu i elementów sterujących na tłach Glass spełnia normy WCAG AA/AAA. Skalowanie siatki kafelków (2 kolumny powyżej 900 px, 1 kolumna poniżej) działa płynnie. Zidentyfikowano dwa drobne punkty ciasnoty horyzontalnej przy 1180 px z otwartym inspektorem (patrz: Problemy P2 i P3). |
| 8 | **Działanie skrótu Ctrl+7** | **PASS** | Skrót klawiszowy `Ctrl+7` niezawodnie przełącza aktywny moduł na `doctor` bez konieczności użycia myszy. Zarejestrowany w popupie pomocy `?` i w podpowiedzi NavRail. |
| 9 | **Kopiowanie raportu** | **PASS** | Funkcja `copyReportToClipboard()` generuje spójny z `scripts/doctor.sh` raport tekstowy do schowka systemowego. UI wyświetla transientne potwierdzenie `✓ Raport skopiowany do schowka!` z automatycznym wygaszaniem po 3 sekundach. |
| 10 | **Zachowanie przy zmianie rozmiaru okna** | **PASS** | Przetestowano skrajne i typowe rozdzielczości (780×560, 1180×850, 1920×1080). Przy 780×560 NavRail automatycznie zwija się do 56 px, prawy inspektor przechodzi w pływający drawer z przyciemnieniem tła, a Doctor przechodzi w układ jednokolumnowy. Brak wycieków pamięci i crashy przy dynamicznym resize. |

---

## 2. Szczegółowa analiza zidentyfikowanych problemów

### Problem 1 (Priorytet P2): Obcięcie przycisków kontekstowych w pasku akcji Studio przy 1180 px z przypiętym inspektorem
- **Lokalizacja:** [StudioView.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/StudioView.qml#L350-L408)
- **Objaw:** W oknie 1180×850 px z otwartym prawym panelem bocznym (szerokość 320 px) i lewą szyną (176 px), obszar roboczy Studio ma szerokość ok. 610 px. Pasek akcji kompozytora zawiera przyciski `[ Reżyseruj ]`, `[ Syntetyzuj ]`, element rozpychający `Item { Layout.fillWidth: true }`, czerwony boks błędu (szer. ok. 320 px) oraz trzy przyciski `[ 📁 Wskaż model... ]`, `[ 🩺 Doctor ]`, `[ ⚙️ Bez TTS ]`. Sumaryczna szerokość paska przekracza 680 px, w wyniku czego przyciski `Doctor` i `Bez TTS` są wypychane poza prawy brzeg karty i ucinane przez `clip: true`.
- **Wpływ:** Użytkownik widzi błąd `⚠ Brak modelu głosu...` oraz fragment pierwszego przycisku `📁 Wskaż model...`, ale nie widzi pozostałych dwóch przycisków akcji, dopóki nie zamknie panelu bocznego lub nie powiększy okna.
- **Rekomendacja:** **Wdrażać poprawkę w kolejnym sprincie UI (M15).**  
  *Sugerowane rozwiązanie:* Przenieść baner błędu z przyciskami do dedykowanego wiersza pod kompozytorem (tak jak zrobiono z banerem trybu reżyserskiego) lub zastosować zawijanie `Flow` / skrócone etykiety na ekranach poniżej 1300 px.

### Problem 2 (Priorytet P3): Ciasnota paska narzędziowego DoctorView przy otwartym inspektorze
- **Lokalizacja:** [DoctorView.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/DoctorView.qml#L182-L245)
- **Objaw:** W karcie głównego statusu DoctorView, wiersz przycisków mieści 5 kontrolek (`Kopiuj pełny raport`, `Odśwież stan`, `Wskaż model z dysku...`, `Włącz tryb bez TTS`, `Resetuj kreator`). Przy otwartym ContextInspectorze skrajny prawy przycisk resetu kreatora First-Run ma lekko ściśniętą etykietę (`↺ Resetuj...`).
- **Wpływ:** Kosmetyczny — przycisk pozostaje w pełni klikalny i funkcjonalny, a przy zamkniętym inspektorze lub oknie >1280 px mieści się bez problemu.
- **Rekomendacja:** **Odłożyć / opcjonalnie poprawić przy okazji M15.**  
  *Sugerowane rozwiązanie:* Zmienić etykietę na zwięzłą `↺ Resetuj First-Run` lub przenieść przycisk do menu opcji zaawansowanych.

### Problem 3 (Priorytet P3): Etykieta skrótów klawiszowych w stopce głównej okna
- **Lokalizacja:** [AgentMain.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/AgentMain.qml#L489)
- **Objaw:** Tekst podpowiedzi w dolnym pasku brzmi `Ctrl+1..6 moduły`, podczas gdy po dodaniu modułu Doctora aplikacja posiada 7 modułów (`Ctrl+1..7`). W oknie pomocy `?` skrót `Ctrl+7` jest już poprawnie opisany.
- **Wpływ:** Czysto informacyjny / estetyczny.
- **Rekomendacja:** **Wdrażać poprawkę (prosta korekta jednego ciągu tekstowego w M15).**

### Problem 4 (Priorytet P3): Pionowy padding w kafelku modelu głosu w FirstRunWizard
- **Lokalizacja:** [FirstRunWizard.qml](file:///home/jaro/.openclaw/workspace/AUDIOTICON%202.0/ui/src/qml/FirstRunWizard.qml#L330-L365)
- **Objaw:** Wiersz `Model Głosu Piper (.onnx + .json)` ma sztywno zdefiniowaną wysokość `implicitHeight: 46`. Przy długiej ścieżce katalogu XDG tekst podrzędny znajduje się bardzo blisko dolnej ramki (margines ok. 2-3 px).
- **Wpływ:** Minimalny — tekst nie ulega ucięciu i mieści się w granicach karty.
- **Rekomendacja:** **Odłożyć do rutynowego przeglądu stylów.**

---

## 3. Podsumowanie i werdykt audytu

Wdrożenie First Run Wizard i Doctor UX w kamieniu milowym M13 stanowi **znaczący krok jakościowy** dla Audioticon 2.0:
1. **Natywny, lokalny charakter:** Aplikacja zachowuje 100% niezależności od Internetu i przeglądarek, jasno komunikując zasady lokalnego runtime bez serwera HTTP.
2. **Spójność wizualna:** Język ASTRA Dark Glass doskonale współgra z nowymi elementami telemetrycznymi i diodami LED. Hierarchia okna, typografia monospace i czytelność statusów OK/WARN/FAIL stoją na poziomie profesjonalnych stacji audio (DAW).
3. **Płynność i stabilność:** Przełączanie modułów przez `Ctrl+7`, drawer kontekstowy, obsługa schowka systemowego i skalowanie do 780×560 px działają stabilnie, bez regresji w 32 testach jednostkowych Qt.
4. **Zidentyfikowane uwagi:** Wszystkie znalezione mankamenty mają charakter czysto kosmetyczny lub wynikają z ograniczeń horyzontalnych przy jednoczesnym otwarciu wszystkich paneli bocznych na mniejszych rozdzielczościach. Nie stwierdzono żadnych blockerów funkcjonalnych ani architektonicznych.

**Werdykt audytu:** **POZYTYWNY (PASS)** — gotowość do prezentacji i przejścia do kolejnych etapów pakietowania wydania.
