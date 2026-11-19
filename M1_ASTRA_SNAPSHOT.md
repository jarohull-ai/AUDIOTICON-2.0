# M1 — Raport snapshotu kopii ASTRA E0–E1

**Data utworzenia:** 2026-11-18  
**Autor:** Antigravity (Audyt i Konsolidacja M1)  
**Cel:** Zabezpieczenie właściwej kopii roboczej ASTRA z etapów E0–E1 przed ryzykiem utraty lub rozjazdu repozytoriów.

---

## 1. Kontekst i identyfikacja problemu

Podczas audytu M0 zidentyfikowano krytyczne ryzyko architektoniczne (R-01):
- **Czysta baza:** `/home/jaro/.openclaw/workspace/ASTRA-UI-Template` znajdowała się na gałęzi `agent-ui` (commit `4252f4f`), ale z zupełnie czystym katalogiem roboczym (brak modułów `src/core/`, brak `core.pri`, brak testów golden).
- **Źródło zmian E0–E1:** `/home/jaro/.config/freebuff-desktop/local-projects/32b70bfb-05a3-46fe-bd12-5e50e0e0f4e7` zawierało kompletną implementację etapów E0 (konfiguracja modułów Qt, ścieżki XDG) oraz E1 (port C++ parsera dyrektyw i verbalizera polskiego, 17 przechodzących testów jednostkowych, w tym testy golden tts zgodne słowo w słowo z Portable Local Studio). Zmiany te były **niezacommitowane**.

Zgodnie z wytycznymi M1:
1. **Żadne pliki w bazach produkcyjnych nie zostały zmodyfikowane, skasowane ani zacommitowane.**
2. Przygotowano pełny, bezpieczny, izolowany snapshot w dedykowanym katalogu `AUDIOTICON 2.0`.

---

## 2. Inwentaryzacja plików E0–E1

Snapshot objął 14 kluczowych plików składających się na etapy E0 i E1:

### A. Pliki projektu i kompilacji (E0)
- `agent-console.pro` (zmodyfikowany: dodano `multimedia sql network` oraz `include(src/core/core.pri)`)
- `src/core/core.pri` (nowy: rejestracja źródeł i nagłówków `libaudioticon_core` dla qmake6)
- `tests/tests.pro` (zmodyfikowany: dołączenie `core.pri` oraz zasobów `golden.qrc`)

### B. Architektura i ustalenia produktowe
- `docs/ARCHITECTURE.md` (nowy: 3 754 bajty, trwale zapisane decyzje projektowe dotyczące Audioticon 2.0, struktury modułów, etapów E0–E5 i wycinka pionowego)

### C. Moduły `src/core` (E0 / E1)
- `src/core/Paths.h` / `src/core/Paths.cpp` (nowe: obsługa ścieżek XDG dla danych, cache i konfiguracji aplikacji)
- `src/core/voice/DirectiveParser.h` / `src/core/voice/DirectiveParser.cpp` (nowe: parser dyrektyw emoji, podział na segmenty, tempo, pauzy, rozdziały i plany akustyczne)
- `src/core/voice/PolishVerbalizer.h` / `src/core/voice/PolishVerbalizer.cpp` (nowe: polski verbalizer liczb, godzin, dat i jednostek)

### D. Zestaw testów i Golden Tests (E1)
- `tests/generate-golden.cjs` (nowy: skrypt generujący referencyjny zestaw testowy z modułu dawcy)
- `tests/data/tts-golden.json` (nowy: 25 865 bajtów, 44 przypadki testowe + paleta dyrektyw)
- `tests/golden.qrc` (nowy: plik zasobów Qt włączający `tts-golden.json` do binarki testowej)
- `tests/tests.cpp` (zmodyfikowany: 17 testów jednostkowych `QVERIFY`/`QCOMPARE`, w tym `directiveGolden` i `directivePaletteMatchesDonor` — 100% PASS)

---

## 3. Lokalizacja snapshotu

Wszystkie pliki zostały bezpiecznie skopiowane z zachowaniem pełnej hierarchii katalogowej do:
```
/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/ASTRA-E0-E1-SNAPSHOT/
```

W katalogu snapshotu wygenerowano również:
- `MANIFEST.json` — maszynowy manifest ze szczegółami każdego pliku (ścieżki, rozmiary, hashe SHA-256).
- `modified_files.patch` — unifikowany patch zmian wprowadzonych w stosunku do commita bazowego `4252f4f`.

---

## 4. Manifest integralności (Źródło → Cel → Hashe SHA-256)

Wszystkie sumy kontrolne SHA-256 zostały zweryfikowane binarnie (100% zgodności między źródłem a snapshotem):

| Status | Ścieżka relatywna | Rozmiar | Hash SHA-256 (snapshot) | Match |
|---|---|---|---|---|
| **MODIFIED** | `agent-console.pro` | 400 B | `c08bcb684e52857418289658dbbc087961cb2820610ddb9e2c65a58572b84fa4` | ✅ Tak |
| **NEW** | `docs/ARCHITECTURE.md` | 3 754 B | `17dc3c9ad3ed28bc5f22e70e9f6bfab6e336be653f86807baab1b4fbdfd2d095` | ✅ Tak |
| **NEW** | `src/core/core.pri` | 313 B | `0dbe0bcbf1a5b8ae1d14695e994fa471e4119d53c7c4ae6fa875e533d1bcf5a2` | ✅ Tak |
| **NEW** | `src/core/Paths.h` | 618 B | `65c4311b68840b2a758782a201b1cbda889fa6d8d9b1585fdfd21ba7eaec88c1` | ✅ Tak |
| **NEW** | `src/core/Paths.cpp` | 877 B | `2a0536fbe349e548d82d4957e1dfd4f6cbf105e19277f979148d4869bb9f5efd` | ✅ Tak |
| **NEW** | `src/core/voice/PolishVerbalizer.h` | 522 B | `8cbf2f0afa7d4f82662c1bbd6aeeb4b8da1e1e07590d0b00109ae9b43d7890a5` | ✅ Tak |
| **NEW** | `src/core/voice/PolishVerbalizer.cpp` | 20 667 B | `6ae015e1c5eb93a8eec3d7798380b2d6a5c2d3b44b80e4fb96aee0632d431c4f` | ✅ Tak |
| **NEW** | `src/core/voice/DirectiveParser.h` | 1 561 B | `423530030960ea5eb5b2323e20e83b8b1db7ff10a8b9e6cc86d529f79caef89d` | ✅ Tak |
| **NEW** | `src/core/voice/DirectiveParser.cpp` | 13 644 B | `b22c0f671e326e3c0e181e18d09f7a731efcbaee5ef3ee8b417f7bfa9d0f28e5` | ✅ Tak |
| **NEW** | `tests/data/tts-golden.json` | 25 865 B | `0f5f846d577df0f04eb20b22a009e992167d30f760da681121d51f28b49911e8` | ✅ Tak |
| **NEW** | `tests/generate-golden.cjs` | 3 703 B | `f311f00a4810bf2092c2b64d8feee7f9c8942b109b0b1fe910b8cf83aa1d5792` | ✅ Tak |
| **NEW** | `tests/golden.qrc` | 129 B | `e8d53c3e151e367876a39209ea14bc74d41fa63bc58b29df99b33a7fca57c617` | ✅ Tak |
| **MODIFIED** | `tests/tests.cpp` | 16 555 B | `0e2da3e530ea69d273e8f8eb586ef8e858db4f4bc8fb3f538ee5903bfe02efc2` | ✅ Tak |
| **MODIFIED** | `tests/tests.pro` | 379 B | `242e5c745a666ea374c44e99f66085a6dbd4c062a4d3ce427a1f592231e67041` | ✅ Tak |

---

## 5. Podsumowanie stanu technicznego

1. **Testy jednostkowe:** W katalogu `build-tests` źródła wykonano weryfikację zestawu testów. Wynik: **17 passed, 0 failed** (w tym testy `directiveGolden` sprawdzające zgodność 44 przypadków dyrektyw i normalizacji tekstu oraz testy `directivePaletteMatchesDonor`).
2. **Bezpieczeństwo baz roboczych:**
   - W repozytorium `/home/jaro/.openclaw/workspace/ASTRA-UI-Template` nie dokonano żadnych zmian (status czysty).
   - W roboczej kopii `/home/jaro/.config/freebuff-desktop/...` nie wykonano commitów ani usunięć plików — stan pozostał nienaruszony.
3. **Gotowość:** Kod E0–E1 jest trwale zabezpieczony w przestrzeni `AUDIOTICON 2.0/ASTRA-E0-E1-SNAPSHOT` i może posłużyć jako wzorzec referencyjny lub baza testowa do integracji z docelowym bridge'em Rust ↔ Qt.
