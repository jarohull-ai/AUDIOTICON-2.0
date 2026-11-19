//! M17: wspólny, w pełni lokalny katalog modeli.
//!
//! Katalog jest jedynym źródłem prawdy o tym, jakie modele są zainstalowane na
//! dysku i w jakim są stanie. Obsługuje cztery rodzaje modeli w osobnych
//! podkatalogach:
//!
//! ```text
//! <XDG_DATA_HOME>/audioticon/models/
//!     piper/      Piper TTS        (*.onnx + *.onnx.json)
//!     whisper/    Whisper / ASR    (*.bin + *.json lub <nazwa>/model.bin)
//!     llm/        lokalne LLM      (*.gguf)
//!     voices/     przyszłe modele głosów (*.json)
//! ```
//!
//! Zasady tego modułu:
//!
//! * **Zero sieci.** Moduł nie ma żadnej ścieżki HTTP, `curl` ani pobierania.
//!   `models::download_model` pozostaje osobnym, jawnie wywoływanym API.
//! * **Brak fałszywego sukcesu.** Model bez kompletu plików albo z uszkodzonym
//!   plikiem ma jawny status `missing_metadata` / `corrupt` i nie da się go
//!   wybrać jako aktywnego.
//! * **Stabilne id.** Identyfikator to `<podkatalog>/<nazwa pliku bez rozszerzenia>`,
//!   np. `piper/pl_PL-darkman-medium`; nie zależy od kolejności skanowania.
//! * **Metadane opcjonalne, ale raportowane.** Checksum jest liczony tylko gdy
//!   obok modelu leży plik `<model>.sha256` (duże pliki GGUF nie są hashowane
//!   przy każdym skanie).
//! * **Import w toku jest widoczny.** Niedokończone importy leżą w
//!   `<models>/.staging/<import>/` i są raportowane jako `incomplete`, więc UI
//!   nigdy nie pokazuje połowy modelu jako gotowego.

use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::fs;
use std::path::{Path, PathBuf};
use thiserror::Error;

/// Podkatalog w katalogu danych aplikacji, w którym trzymamy modele.
pub const MODELS_SUBDIR: &str = "models";
/// Nazwa pliku z zapisanym wyborem aktywnego modelu (obok katalogu `models/`).
pub const SELECTION_FILE_NAME: &str = "model_selection.json";
/// Katalog roboczy niedokończonych importów (w środku `models/`, ale jako
/// katalog z kropką — skan modeli czyta tylko pliki, nie katalogi).
pub const STAGING_SUBDIR: &str = ".staging";
/// Opis intencji importu zapisany w katalogu roboczym.
pub const STAGING_INTENT_FILE: &str = "intent.json";
/// Poniżej tego rozmiaru plik wag uznajemy za obcięty/uszkodzony.
const MIN_VALID_WEIGHT_BYTES: u64 = 1024;

/// Rodzaj modelu. Kolejność wariantów wyznacza kolejność w `list_models`.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ModelKind {
    PiperTts,
    WhisperAsr,
    LocalLlm,
    Voice,
}

impl ModelKind {
    pub const ALL: [ModelKind; 4] = [
        ModelKind::PiperTts,
        ModelKind::WhisperAsr,
        ModelKind::LocalLlm,
        ModelKind::Voice,
    ];

    /// Podkatalog na dysku, w którym szukamy modeli tego rodzaju.
    pub fn dir_name(self) -> &'static str {
        match self {
            ModelKind::PiperTts => "piper",
            ModelKind::WhisperAsr => "whisper",
            ModelKind::LocalLlm => "llm",
            ModelKind::Voice => "voices",
        }
    }

    /// Stabilna nazwa rodzaju używana w id, JSON i na granicy FFI.
    pub fn as_str(self) -> &'static str {
        match self {
            ModelKind::PiperTts => "piper_tts",
            ModelKind::WhisperAsr => "whisper_asr",
            ModelKind::LocalLlm => "local_llm",
            ModelKind::Voice => "voice",
        }
    }

    pub fn parse(value: &str) -> Option<Self> {
        match value.trim().to_lowercase().as_str() {
            "piper_tts" | "piper" => Some(ModelKind::PiperTts),
            "whisper_asr" | "whisper" | "asr" => Some(ModelKind::WhisperAsr),
            "local_llm" | "llm" => Some(ModelKind::LocalLlm),
            "voice" | "voices" => Some(ModelKind::Voice),
            _ => None,
        }
    }
}

/// Format pliku z wagami / opisem modelu.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ModelFormat {
    Onnx,
    Gguf,
    Binary,
    Json,
}

impl ModelFormat {
    pub fn as_str(self) -> &'static str {
        match self {
            ModelFormat::Onnx => "onnx",
            ModelFormat::Gguf => "gguf",
            ModelFormat::Binary => "bin",
            ModelFormat::Json => "json",
        }
    }

    pub fn parse(value: &str) -> Option<Self> {
        match value.trim().to_lowercase().as_str() {
            "onnx" => Some(ModelFormat::Onnx),
            "gguf" => Some(ModelFormat::Gguf),
            "bin" | "binary" => Some(ModelFormat::Binary),
            "json" => Some(ModelFormat::Json),
            _ => None,
        }
    }
}

/// Status dostępności modelu. To jedyne miejsce, w którym zapada decyzja
/// "czy tym modelem można renderować".
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ModelAvailability {
    /// Komplet plików na dysku, plik wag wygląda na cały.
    Available,
    /// Wagi są, ale brakuje pliku konfiguracji (np. `.onnx.json`).
    MissingMetadata,
    /// Plik istnieje, ale jest pusty/obcięty albo nie zgadza się z checksumem.
    Corrupt,
    /// Import lub pobieranie jeszcze trwa (albo zostało przerwane).
    Incomplete,
}

impl ModelAvailability {
    pub fn as_str(self) -> &'static str {
        match self {
            ModelAvailability::Available => "available",
            ModelAvailability::MissingMetadata => "missing_metadata",
            ModelAvailability::Corrupt => "corrupt",
            ModelAvailability::Incomplete => "incomplete",
        }
    }

    pub fn is_available(self) -> bool {
        matches!(self, ModelAvailability::Available)
    }
}

/// Jeden model widziany przez katalog.
///
/// Wszystkie pola poza `status`/`detail` są stabilne między skanami; `id` jest
/// kluczem używanym przez FFI i plik wyboru.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct CatalogModel {
    pub id: String,
    pub kind: ModelKind,
    pub name: String,
    pub path: PathBuf,
    pub format: ModelFormat,
    pub language: Option<String>,
    pub size_bytes: u64,
    pub license: Option<String>,
    pub status: ModelAvailability,
    pub checksum_sha256: Option<String>,
    /// Czytelny powód statusu (pusty dla `Available`).
    pub detail: String,
}

impl CatalogModel {
    pub fn is_available(&self) -> bool {
        self.status.is_available()
    }
}

/// Zapisany wybór aktywnego modelu (persystowany w `model_selection.json`).
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct ModelSelection {
    pub id: String,
    pub kind: ModelKind,
    pub path: PathBuf,
    /// Znacznik czasu wyboru; celowo bez zależności od chrono.
    pub selected_at_utc: String,
}

/// Podsumowanie katalogu — to, co widzi UI, zanim zapyta o pojedynczy model.
#[derive(Debug, Clone, PartialEq, Default)]
pub struct CatalogStatus {
    pub root: PathBuf,
    pub root_exists: bool,
    pub total: u32,
    pub available: u32,
    pub unavailable: u32,
    /// Liczba modeli, których import/pobieranie nie zostało ukończone.
    pub incomplete: u32,
    /// Wybrany model, jeśli wybór nadal wskazuje na dostępny plik.
    pub active: Option<ModelSelection>,
    /// Wybrany model, nawet jeśli zniknął z dysku (do raportowania błędu).
    pub selected: Option<ModelSelection>,
}

impl CatalogStatus {
    pub fn message(&self) -> String {
        if self.total == 0 && self.selected.is_none() {
            if self.incomplete > 0 {
                return format!(
                    "Brak gotowych modeli: {} nieukończonych importów w {} (dokończ albo anuluj import)",
                    self.incomplete,
                    self.root.display()
                );
            }
            return format!(
                "Brak modeli w {} (dodaj model do piper/, whisper/, llm/ lub voices/)",
                self.root.display()
            );
        }
        if self.incomplete > 0 {
            return format!(
                "{} dostępnych z {} modeli; {} importów nieukończonych",
                self.available, self.total, self.incomplete
            );
        }
        match (&self.selected, &self.active) {
            (Some(sel), Some(_)) => format!(
                "Aktywny model: {} ({} dostępnych z {})",
                sel.id, self.available, self.total
            ),
            (Some(sel), None) => format!(
                "Wybrany model {} nie jest już dostępny ({} dostępnych z {})",
                sel.id, self.available, self.total
            ),
            (None, _) if self.available == 0 => format!(
                "Brak dostępnego modelu: {} wpisów, wszystkie niekompletne lub uszkodzone",
                self.total
            ),
            (None, _) => format!(
                "Nie wybrano modelu ({} dostępnych z {})",
                self.available, self.total
            ),
        }
    }
}

/// Zapisana intencja importu w `<models>/.staging/<import>/intent.json`.
///
/// Katalog czyta ten plik, więc przerwany import jest widoczny po restarcie
/// jako `incomplete`, a nie jako „brak modeli”.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct StagingIntent {
    pub import_id: u64,
    /// Docelowe, stabilne id modelu (`llm/qwen2.5-3b-instruct-q4`).
    pub id: String,
    pub kind: String,
    pub format: String,
    /// Nazwy plików, które powstaną w katalogu rodzaju po ukończeniu importu.
    pub files: Vec<String>,
    /// Czytelne źródło: `curated:<id>`, `local:<ścieżka>`, `hf:...`, `gh:...`.
    pub source: String,
    pub started_at_utc: String,
}

#[derive(Error, Debug)]
pub enum CatalogError {
    #[error("Model '{0}' nie istnieje w lokalnym katalogu")]
    NotFound(String),

    #[error("Model '{id}' nie jest dostępny (status: {status}, {detail})")]
    NotAvailable {
        id: String,
        status: &'static str,
        detail: String,
    },

    #[error("Nie można zapisać wyboru modelu: {0}")]
    Persistence(String),
}

/// Katalog modeli wskazujący na konkretny katalog główny.
///
/// Wszystkie operacje przyjmują jawne ścieżki, więc testy nie dotykają
/// konfiguracji użytkownika, a UI może wskazać inny root (np. przenośny bundle).
#[derive(Debug, Clone)]
pub struct ModelCatalog {
    root: PathBuf,
    /// Czy wolno szukać modeli w starych, płaskich lokalizacjach.
    /// Włączone tylko dla katalogu domyślnego użytkownika.
    legacy_fallback: bool,
}

impl ModelCatalog {
    pub fn new(root: impl Into<PathBuf>) -> Self {
        Self {
            root: root.into(),
            legacy_fallback: false,
        }
    }

    /// Katalog bieżącego użytkownika: `$XDG_DATA_HOME/audioticon/models`
    /// (a bez XDG `~/.local/share/audioticon/models`).
    pub fn for_current_user() -> Self {
        Self {
            root: crate::models::models_dir(),
            legacy_fallback: true,
        }
    }

    pub fn root(&self) -> &Path {
        &self.root
    }

    pub fn uses_legacy_fallback(&self) -> bool {
        self.legacy_fallback
    }

    pub fn kind_dir(&self, kind: ModelKind) -> PathBuf {
        self.root.join(kind.dir_name())
    }

    /// Plik z wyborem leży obok katalogu modeli, więc skanowanie `models/`
    /// nigdy nie zwraca go jako modelu.
    pub fn selection_path(&self) -> PathBuf {
        match self.root.parent() {
            Some(parent) if !parent.as_os_str().is_empty() => parent.join(SELECTION_FILE_NAME),
            _ => self.root.join(SELECTION_FILE_NAME),
        }
    }

    /// Katalog roboczy importów (`.staging`), nie jest modelem.
    pub fn staging_root(&self) -> PathBuf {
        self.root.join(STAGING_SUBDIR)
    }

    /// Tworzy `models/` i cztery podkatalogi. Idempotentne, offline.
    pub fn ensure_dirs(&self) -> std::io::Result<()> {
        fs::create_dir_all(&self.root)?;
        for kind in ModelKind::ALL {
            fs::create_dir_all(self.kind_dir(kind))?;
        }
        Ok(())
    }

    /// Skanuje wszystkie podkatalogi. Kolejność: rodzaj, potem id.
    pub fn scan(&self) -> Vec<CatalogModel> {
        let mut out = Vec::new();
        for kind in ModelKind::ALL {
            out.extend(self.scan_kind(kind));
        }
        let incomplete = self.scan_incomplete();
        // Gotowy model wygrywa z duplikatem wpisu z katalogu roboczego.
        for entry in incomplete {
            if !out.iter().any(|m| m.id == entry.id) {
                out.push(entry);
            }
        }
        out.sort_by(|a, b| a.id.cmp(&b.id));
        out
    }

    /// Lista modeli, opcjonalnie zawężona do jednego rodzaju.
    pub fn list(&self, kind: Option<ModelKind>) -> Vec<CatalogModel> {
        match kind {
            Some(k) => self.scan_kind(k),
            None => self.scan(),
        }
    }

    pub fn inspect(&self, id: &str) -> Option<CatalogModel> {
        let kind = Self::kind_from_id(id)?;
        self.scan_kind(kind).into_iter().find(|m| m.id == id)
    }

    // ---- wybór aktywnego modelu -------------------------------------------

    pub fn active(&self) -> Option<ModelSelection> {
        self.load_selection()
            .filter(|sel| self.inspect(&sel.id).is_some_and(|m| m.is_available()))
    }

    pub fn selected(&self) -> Option<ModelSelection> {
        self.load_selection()
    }

    /// Wybiera model i utrwala wybór. Model musi istnieć i być `Available`;
    /// wybór jest odczytywany z powrotem z dysku, żeby zapis nie mógł
    /// zakończyć się "sukcesem" bez utrwalenia.
    pub fn select(&self, id: &str) -> Result<ModelSelection, CatalogError> {
        let model = self
            .inspect(id)
            .ok_or_else(|| CatalogError::NotFound(id.to_string()))?;
        if !model.is_available() {
            return Err(CatalogError::NotAvailable {
                id: id.to_string(),
                status: model.status.as_str(),
                detail: model.detail.clone(),
            });
        }

        let selection = ModelSelection {
            id: model.id.clone(),
            kind: model.kind,
            path: model.path.clone(),
            selected_at_utc: utc_timestamp(),
        };
        let path = self.selection_path();
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent)
                .map_err(|err| CatalogError::Persistence(format!("{}: {err}", parent.display())))?;
        }
        let json = serde_json::to_vec_pretty(&selection)
            .map_err(|err| CatalogError::Persistence(err.to_string()))?;
        fs::write(&path, json)
            .map_err(|err| CatalogError::Persistence(format!("{}: {err}", path.display())))?;

        // Zapis bez potwierdzenia to nie zapis: czytamy to, co faktycznie leży.
        match self.load_selection() {
            Some(saved) if saved.id == selection.id && saved.path == selection.path => {
                Ok(selection)
            }
            _ => Err(CatalogError::Persistence(format!(
                "wybór nie został utrwalony w {}",
                path.display()
            ))),
        }
    }

    /// Usuwa utrwalony wybór. Zwraca `true`, jeśli plik wyboru istniał.
    pub fn clear_selection(&self) -> Result<bool, CatalogError> {
        let path = self.selection_path();
        match fs::remove_file(&path) {
            Ok(()) => Ok(true),
            Err(err) if err.kind() == std::io::ErrorKind::NotFound => Ok(false),
            Err(err) => Err(CatalogError::Persistence(format!("{}: {err}", path.display()))),
        }
    }

    pub fn status(&self) -> CatalogStatus {
        let models = self.scan();
        let available = models.iter().filter(|m| m.is_available()).count() as u32;
        let incomplete = models
            .iter()
            .filter(|m| m.status == ModelAvailability::Incomplete)
            .count() as u32;
        let total = models.len() as u32;
        let selected = self.load_selection();
        let active = selected
            .clone()
            .filter(|sel| self.inspect(&sel.id).is_some_and(|m| m.is_available()));
        CatalogStatus {
            root: self.root.clone(),
            root_exists: self.root.is_dir(),
            total,
            available,
            unavailable: total.saturating_sub(available),
            incomplete,
            active,
            selected,
        }
    }

    fn load_selection(&self) -> Option<ModelSelection> {
        let path = self.selection_path();
        let content = fs::read(&path).ok()?;
        serde_json::from_slice::<ModelSelection>(&content).ok()
    }

    fn kind_from_id(id: &str) -> Option<ModelKind> {
        let (dir, _rest) = id.split_once('/')?;
        ModelKind::parse(dir)
    }

    // ---- skanowanie poszczególnych rodzajów --------------------------------

    fn scan_kind(&self, kind: ModelKind) -> Vec<CatalogModel> {
        match kind {
            ModelKind::PiperTts => self.scan_piper(),
            ModelKind::WhisperAsr => self.scan_whisper(),
            ModelKind::LocalLlm => self.scan_llm(),
            ModelKind::Voice => self.scan_voices(),
        }
    }

    /// Niedokończone importy zapisane w `<models>/.staging/*/intent.json`.
    fn scan_incomplete(&self) -> Vec<CatalogModel> {
        let mut out = Vec::new();
        let Ok(entries) = fs::read_dir(self.staging_root()) else {
            return out;
        };
        for entry in entries.flatten() {
            let dir = entry.path();
            if !dir.is_dir() {
                continue;
            }
            let Ok(raw) = fs::read_to_string(dir.join(STAGING_INTENT_FILE)) else {
                continue;
            };
            let Ok(intent) = serde_json::from_str::<StagingIntent>(&raw) else {
                continue;
            };
            let (Some(kind), Some(format)) =
                (ModelKind::parse(&intent.kind), ModelFormat::parse(&intent.format))
            else {
                continue;
            };

            let size_bytes = intent
                .files
                .iter()
                .map(|name| {
                    let part = dir.join(format!("{name}.part"));
                    let full = dir.join(name);
                    fs::metadata(&part)
                        .or_else(|_| fs::metadata(&full))
                        .map(|m| m.len())
                        .unwrap_or(0)
                })
                .sum();

            let stem = intent
                .id
                .rsplit_once('/')
                .map(|(_, rest)| rest)
                .unwrap_or(intent.id.as_str());
            out.push(CatalogModel {
                id: intent.id.clone(),
                kind,
                name: display_name(stem),
                path: dir.clone(),
                format,
                language: None,
                size_bytes,
                license: None,
                status: ModelAvailability::Incomplete,
                checksum_sha256: None,
                detail: format!(
                    "import nieukończony ({}); usuń albo pobierz ponownie",
                    intent.source
                ),
            });
        }
        out
    }

    /// Pliki należące do modelu: wagi, konfiguracja i checksumy.
    ///
    /// Import i usuwanie korzystają z tej samej listy, żeby nigdy nie usuwać
    /// plików spoza katalogu rodzaju i nie zgadywać nazw sidecarów.
    pub fn model_files(&self, id: &str) -> Option<Vec<PathBuf>> {
        let model = self.inspect(id)?;
        Some(model_companion_files(&model))
    }

    fn scan_piper(&self) -> Vec<CatalogModel> {
        let dir = self.kind_dir(ModelKind::PiperTts);
        let mut out = Vec::new();
        let Ok(entries) = fs::read_dir(&dir) else {
            return out;
        };

        for entry in entries.flatten() {
            let path = entry.path();
            if !path.is_file() || !has_extension(&path, "onnx") {
                continue;
            }
            let Some(stem) = file_stem(&path) else {
                continue;
            };

            let config_path = dir.join(format!("{stem}.onnx.json"));
            let config = fs::read_to_string(&config_path).ok();
            let (size, read_error) = weight_size(&path);

            let language = config
                .as_deref()
                .and_then(parse_language)
                .or_else(|| language_from_stem(&stem));
            let license = config.as_deref().and_then(parse_license);
            let checksum = read_checksum_sidecar(&path);

            let (status, detail) = classify(WeightFile {
                size,
                read_error,
                checksum_expected: checksum.clone(),
                checksum_actual: checksum.as_ref().map(|_| sha256_file(&path)),
                config_present: config_path.is_file(),
                config_required: true,
            });

            out.push(CatalogModel {
                id: format!("{}/{stem}", ModelKind::PiperTts.dir_name()),
                kind: ModelKind::PiperTts,
                name: display_name(&stem),
                path,
                format: ModelFormat::Onnx,
                language,
                size_bytes: size,
                license,
                status,
                checksum_sha256: checksum,
                detail,
            });
        }
        out
    }

    fn scan_whisper(&self) -> Vec<CatalogModel> {
        let dir = self.kind_dir(ModelKind::WhisperAsr);
        let mut out = Vec::new();
        let Ok(entries) = fs::read_dir(&dir) else {
            return out;
        };

        for entry in entries.flatten() {
            let path = entry.path();

            // Wariant A: <nazwa>/model.bin + <nazwa>/config.json
            if path.is_dir() {
                let weights = path.join("model.bin");
                if !weights.is_file() {
                    continue;
                }
                let Some(name) = path.file_name().and_then(|s| s.to_str()) else {
                    continue;
                };
                let config_path = path.join("config.json");
                out.push(self.whisper_entry(&weights, name, &config_path));
                continue;
            }

            // Wariant B: <nazwa>.bin + <nazwa>.json
            if has_extension(&path, "bin") {
                if let Some(stem) = file_stem(&path) {
                    let config_path = dir.join(format!("{stem}.json"));
                    out.push(self.whisper_entry(&path, &stem, &config_path));
                }
            }
        }
        out
    }

    fn whisper_entry(&self, weights: &Path, name: &str, config_path: &Path) -> CatalogModel {
        let config = fs::read_to_string(config_path).ok();
        let (size, read_error) = weight_size(weights);
        let checksum = read_checksum_sidecar(weights);
        // Modele whisper.cpp (`ggml-*.bin`) mają metadane w środku pliku, więc
        // brak config.json nie jest brakiem metadanych — tak samo jak GGUF.
        let config_required = !name.starts_with("ggml-");
        let (status, detail) = classify(WeightFile {
            size,
            read_error,
            checksum_expected: checksum.clone(),
            checksum_actual: checksum.as_ref().map(|_| sha256_file(weights)),
            config_present: config_path.is_file(),
            config_required,
        });

        CatalogModel {
            id: format!("{}/{name}", ModelKind::WhisperAsr.dir_name()),
            kind: ModelKind::WhisperAsr,
            name: display_name(name),
            path: weights.to_path_buf(),
            format: ModelFormat::Binary,
            language: config.as_deref().and_then(parse_language),
            size_bytes: size,
            license: config.as_deref().and_then(parse_license),
            status,
            checksum_sha256: checksum,
            detail,
        }
    }

    fn scan_llm(&self) -> Vec<CatalogModel> {
        let dir = self.kind_dir(ModelKind::LocalLlm);
        let mut out = Vec::new();
        let Ok(entries) = fs::read_dir(&dir) else {
            return out;
        };

        for entry in entries.flatten() {
            let path = entry.path();
            if !path.is_file() || !has_extension(&path, "gguf") {
                continue;
            }
            let Some(stem) = file_stem(&path) else {
                continue;
            };
            // GGUF niesie metadane wewnątrz pliku, więc brak sidecara nie jest
            // błędem — język i licencja po prostu pozostają nieznane.
            let sidecar_path = dir.join(format!("{stem}.json"));
            let sidecar = fs::read_to_string(&sidecar_path).ok();
            let (size, read_error) = weight_size(&path);
            let checksum = read_checksum_sidecar(&path);
            let (status, detail) = classify(WeightFile {
                size,
                read_error,
                checksum_expected: checksum.clone(),
                checksum_actual: checksum.as_ref().map(|_| sha256_file(&path)),
                config_present: true,
                config_required: false,
            });

            out.push(CatalogModel {
                id: format!("{}/{stem}", ModelKind::LocalLlm.dir_name()),
                kind: ModelKind::LocalLlm,
                name: display_name(&stem),
                path,
                format: ModelFormat::Gguf,
                language: sidecar.as_deref().and_then(parse_language),
                size_bytes: size,
                license: sidecar.as_deref().and_then(parse_license),
                status,
                checksum_sha256: checksum,
                detail,
            });
        }
        out
    }

    fn scan_voices(&self) -> Vec<CatalogModel> {
        let dir = self.kind_dir(ModelKind::Voice);
        let mut out = Vec::new();
        let Ok(entries) = fs::read_dir(&dir) else {
            return out;
        };

        for entry in entries.flatten() {
            let path = entry.path();
            if !path.is_file() || !has_extension(&path, "json") {
                continue;
            }
            let Some(stem) = file_stem(&path) else {
                continue;
            };
            let raw = fs::read_to_string(&path).ok();
            let parsed: Option<serde_json::Value> =
                raw.as_deref().and_then(|text| serde_json::from_str(text).ok());

            let (status, detail) = match (&raw, &parsed) {
                (None, _) => (
                    ModelAvailability::Corrupt,
                    "nie można odczytać pliku profilu głosu".to_string(),
                ),
                (Some(_), None) => (
                    ModelAvailability::Corrupt,
                    "plik nie jest poprawnym JSON-em".to_string(),
                ),
                (Some(_), Some(_)) => (ModelAvailability::Available, String::new()),
            };

            out.push(CatalogModel {
                id: format!("{}/{stem}", ModelKind::Voice.dir_name()),
                kind: ModelKind::Voice,
                name: display_name(&stem),
                path: path.clone(),
                format: ModelFormat::Json,
                language: raw.as_deref().and_then(parse_language),
                size_bytes: fs::metadata(&path).map(|m| m.len()).unwrap_or(0),
                license: raw.as_deref().and_then(parse_license),
                status,
                checksum_sha256: read_checksum_sidecar(&path),
                detail,
            });
        }
        out
    }
}

// ---- pomocnicze -----------------------------------------------------------

/// Wszystkie pliki modelu, liczone wyłącznie w obrębie jego katalogu rodzaju.
pub fn model_companion_files(model: &CatalogModel) -> Vec<PathBuf> {
    let mut files = vec![model.path.clone()];
    let Some(dir) = model.path.parent() else {
        return files;
    };
    let Some(stem) = file_stem(&model.path) else {
        return files;
    };

    match model.kind {
        ModelKind::PiperTts => files.push(dir.join(format!("{stem}.onnx.json"))),
        ModelKind::WhisperAsr | ModelKind::LocalLlm => {
            files.push(dir.join(format!("{stem}.json")))
        }
        ModelKind::Voice => {}
    }

    // Sidecar checksum leży przy pliku wag: `<wagi>.sha256`.
    let mut sidecar = model.path.clone().into_os_string();
    sidecar.push(".sha256");
    files.push(PathBuf::from(sidecar));

    // Dla układu katalogowego Whispera (`<nazwa>/model.bin`) config leży obok.
    if model.kind == ModelKind::WhisperAsr && stem == "model" {
        files.push(dir.join("config.json"));
    }
    files
}

struct WeightFile {
    size: u64,
    read_error: Option<std::io::ErrorKind>,
    checksum_expected: Option<String>,
    checksum_actual: Option<String>,
    config_present: bool,
    config_required: bool,
}

fn classify(f: WeightFile) -> (ModelAvailability, String) {
    match f.read_error {
        Some(kind) => {
            return (
                ModelAvailability::Corrupt,
                format!("nie można odczytać pliku wag ({kind:?})"),
            )
        }
        None if f.size == 0 => {
            return (
                ModelAvailability::Corrupt,
                "plik wag jest pusty (0 B)".to_string(),
            )
        }
        None if f.size < MIN_VALID_WEIGHT_BYTES => {
            return (
                ModelAvailability::Corrupt,
                format!("plik wag jest obcięty ({} B)", f.size),
            )
        }
        None => {}
    }

    if f.config_required && !f.config_present {
        return (
            ModelAvailability::MissingMetadata,
            "brak pliku konfiguracji modelu".to_string(),
        );
    }

    if let (Some(expected), Some(actual)) = (&f.checksum_expected, &f.checksum_actual) {
        if !expected.eq_ignore_ascii_case(actual) {
            return (
                ModelAvailability::Corrupt,
                "checksum SHA-256 nie zgadza się z plikiem .sha256".to_string(),
            );
        }
    }

    (ModelAvailability::Available, String::new())
}

fn weight_size(path: &Path) -> (u64, Option<std::io::ErrorKind>) {
    match fs::metadata(path) {
        Ok(meta) if meta.is_file() => (meta.len(), None),
        Ok(_) => (0, Some(std::io::ErrorKind::InvalidInput)),
        Err(err) => (0, Some(err.kind())),
    }
}

fn has_extension(path: &Path, ext: &str) -> bool {
    path.extension()
        .and_then(|s| s.to_str())
        .is_some_and(|s| s.eq_ignore_ascii_case(ext))
}

fn file_stem(path: &Path) -> Option<String> {
    path.file_stem()
        .and_then(|s| s.to_str())
        .map(|s| s.to_string())
        .filter(|s| !s.is_empty())
}

fn display_name(stem: &str) -> String {
    let spaced = stem.replace('_', " ").replace('-', " ");
    let mut chars = spaced.chars();
    match chars.next() {
        Some(first) => first.to_uppercase().collect::<String>() + chars.as_str(),
        None => stem.to_string(),
    }
}

fn parse_language(json: &str) -> Option<String> {
    let value: serde_json::Value = serde_json::from_str(json).ok()?;
    let node = value.get("language")?;
    match node {
        serde_json::Value::String(s) if !s.trim().is_empty() => Some(s.trim().to_string()),
        serde_json::Value::Object(map) => map
            .get("code")
            .and_then(|v| v.as_str())
            .or_else(|| map.get("language").and_then(|v| v.as_str()))
            .map(|s| s.to_string()),
        serde_json::Value::Array(items) => {
            let codes: Vec<&str> = items.iter().filter_map(|v| v.as_str()).collect();
            if codes.is_empty() {
                None
            } else {
                Some(codes.join(","))
            }
        }
        _ => None,
    }
}

fn parse_license(json: &str) -> Option<String> {
    let value: serde_json::Value = serde_json::from_str(json).ok()?;
    value
        .get("license")
        .and_then(|v| v.as_str())
        .map(|s| s.trim().to_string())
        .filter(|s| !s.is_empty())
}

fn language_from_stem(stem: &str) -> Option<String> {
    // Piper nazywa pliki `<kod>-<nazwa>-<jakosc>.onnx`, np. pl_PL-darkman-medium.
    let code = stem.split('-').next()?;
    let looks_like_code = code.contains('_') || code.len() == 2;
    looks_like_code.then(|| code.to_string())
}

fn read_checksum_sidecar(path: &Path) -> Option<String> {
    let mut sidecar = path.as_os_str().to_os_string();
    sidecar.push(".sha256");
    let content = fs::read_to_string(PathBuf::from(sidecar)).ok()?;
    let token = content.split_whitespace().next()?.to_string();
    is_sha256_hex(&token).then_some(token.to_lowercase())
}

fn is_sha256_hex(value: &str) -> bool {
    value.len() == 64 && value.chars().all(|c| c.is_ascii_hexdigit())
}

fn sha256_file(path: &Path) -> String {
    let mut hasher = Sha256::new();
    match fs::read(path) {
        Ok(bytes) => hasher.update(&bytes),
        Err(_) => return String::new(),
    }
    hex::encode(hasher.finalize())
}

fn utc_timestamp() -> String {
    use std::time::{SystemTime, UNIX_EPOCH};
    let secs = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs();
    format!("{secs}-UTC")
}

#[cfg(test)]
mod tests {
    use super::*;

    struct TempRoot {
        dir: PathBuf,
    }

    impl TempRoot {
        fn new(name: &str) -> Self {
            let dir = std::env::temp_dir().join(format!(
                "audioticon-m17-{name}-{}",
                std::process::id()
            ));
            let _ = fs::remove_dir_all(&dir);
            fs::create_dir_all(&dir).expect("katalog tymczasowy");
            Self { dir }
        }

        fn catalog(&self) -> ModelCatalog {
            ModelCatalog::new(self.dir.join("audioticon").join("models"))
        }

        fn path(&self) -> &Path {
            &self.dir
        }
    }

    impl Drop for TempRoot {
        fn drop(&mut self) {
            let _ = fs::remove_dir_all(&self.dir);
        }
    }

    fn write_piper(root: &TempRoot, stem: &str, weight_bytes: usize, with_config: bool) -> PathBuf {
        let catalog = root.catalog();
        catalog.ensure_dirs().unwrap();
        let dir = catalog.kind_dir(ModelKind::PiperTts);
        let onnx = dir.join(format!("{stem}.onnx"));
        fs::write(&onnx, vec![0x08u8; weight_bytes]).unwrap();
        if with_config {
            fs::write(
                dir.join(format!("{stem}.onnx.json")),
                r#"{"language":{"code":"pl_PL"},"license":"CC0-1.0"}"#,
            )
            .unwrap();
        }
        onnx
    }

    #[test]
    fn empty_catalog_reports_no_models_and_no_active_selection() {
        let root = TempRoot::new("empty");
        let catalog = root.catalog();

        assert!(!catalog.root().exists());
        assert!(catalog.scan().is_empty());

        let status = catalog.status();
        assert_eq!(status.total, 0);
        assert_eq!(status.available, 0);
        assert!(status.active.is_none());
        assert!(status.message().contains("Brak modeli"));

        // Skan nie tworzy katalogów — to robi jawnie ensure_dirs().
        catalog.ensure_dirs().unwrap();
        assert!(catalog.kind_dir(ModelKind::PiperTts).is_dir());
        assert!(catalog.kind_dir(ModelKind::WhisperAsr).is_dir());
        assert!(catalog.kind_dir(ModelKind::LocalLlm).is_dir());
        assert!(catalog.kind_dir(ModelKind::Voice).is_dir());
        assert!(catalog.scan().is_empty());
    }

    #[test]
    fn valid_piper_model_is_listed_with_stable_metadata() {
        let root = TempRoot::new("valid-piper");
        let onnx = write_piper(&root, "pl_PL-darkman-medium", 4096, true);
        let catalog = root.catalog();

        let models = catalog.scan();
        assert_eq!(models.len(), 1);
        let model = &models[0];
        assert_eq!(model.id, "piper/pl_PL-darkman-medium");
        assert_eq!(model.kind, ModelKind::PiperTts);
        assert_eq!(model.format, ModelFormat::Onnx);
        assert_eq!(model.path, onnx);
        assert_eq!(model.language.as_deref(), Some("pl_PL"));
        assert_eq!(model.license.as_deref(), Some("CC0-1.0"));
        assert_eq!(model.size_bytes, 4096);
        assert_eq!(model.status, ModelAvailability::Available);
        assert!(model.detail.is_empty());
        assert!(model.checksum_sha256.is_none());

        // Skan jest deterministyczny i nie zmienia id.
        assert_eq!(catalog.scan(), models);
        assert_eq!(catalog.list(Some(ModelKind::PiperTts)), models);
        assert!(catalog.list(Some(ModelKind::LocalLlm)).is_empty());
        assert_eq!(catalog.inspect("piper/pl_PL-darkman-medium"), Some(model.clone()));
        assert!(catalog.inspect("piper/nie-ma").is_none());
        assert!(catalog.inspect("piper").is_none());
    }

    #[test]
    fn corrupt_weight_file_is_never_available() {
        let root = TempRoot::new("corrupt");
        let catalog = root.catalog();
        catalog.ensure_dirs().unwrap();
        let dir = catalog.kind_dir(ModelKind::PiperTts);

        // Pusty plik.
        fs::write(dir.join("pl_PL-empty-medium.onnx"), b"").unwrap();
        fs::write(dir.join("pl_PL-empty-medium.onnx.json"), "{}").unwrap();
        // Obcięty plik (poniżej progu).
        fs::write(dir.join("pl_PL-short-medium.onnx"), vec![1u8; 16]).unwrap();
        fs::write(dir.join("pl_PL-short-medium.onnx.json"), "{}").unwrap();

        let models = catalog.scan();
        let empty = models.iter().find(|m| m.id == "piper/pl_PL-empty-medium").unwrap();
        assert_eq!(empty.status, ModelAvailability::Corrupt);
        assert!(empty.detail.contains("pusty"));

        let short = models.iter().find(|m| m.id == "piper/pl_PL-short-medium").unwrap();
        assert_eq!(short.status, ModelAvailability::Corrupt);
        assert!(short.detail.contains("obcięty"));

        assert_eq!(catalog.status().available, 0);
        assert_eq!(catalog.status().unavailable, 2);
        assert!(matches!(
            catalog.select("piper/pl_PL-empty-medium").unwrap_err(),
            CatalogError::NotAvailable { .. }
        ));
        assert!(catalog.active().is_none());
    }

    #[test]
    fn missing_metadata_is_reported_separately_from_corruption() {
        let root = TempRoot::new("no-meta");
        write_piper(&root, "pl_PL-gosia-medium", 4096, false);
        let catalog = root.catalog();

        let model = catalog.inspect("piper/pl_PL-gosia-medium").unwrap();
        assert_eq!(model.status, ModelAvailability::MissingMetadata);
        assert!(model.detail.contains("konfiguracji"));
        // Język nadal da się wywnioskować z nazwy pliku.
        assert_eq!(model.language.as_deref(), Some("pl_PL"));
        assert!(catalog.select("piper/pl_PL-gosia-medium").is_err());
        assert_eq!(catalog.status().available, 0);
    }

    #[test]
    fn optional_checksum_sidecar_is_verified() {
        let root = TempRoot::new("checksum");
        let onnx = write_piper(&root, "pl_PL-darkman-medium", 2048, true);
        let catalog = root.catalog();
        let expected = sha256_file(&onnx);

        fs::write(
            root.path()
                .join("audioticon/models/piper/pl_PL-darkman-medium.onnx.sha256"),
            format!("{expected}  pl_PL-darkman-medium.onnx\n"),
        )
        .unwrap();
        let model = catalog.inspect("piper/pl_PL-darkman-medium").unwrap();
        assert_eq!(model.status, ModelAvailability::Available);
        assert_eq!(model.checksum_sha256.as_deref(), Some(expected.as_str()));

        // Podmiana zawartości bez aktualizacji sidecara = uszkodzenie.
        fs::write(&onnx, vec![0x09u8; 2048]).unwrap();
        let corrupted = catalog.inspect("piper/pl_PL-darkman-medium").unwrap();
        assert_eq!(corrupted.status, ModelAvailability::Corrupt);
        assert!(corrupted.detail.contains("checksum"));

        // Niepoprawny format sidecara jest ignorowany (checksum opcjonalny).
        fs::write(
            root.path()
                .join("audioticon/models/piper/pl_PL-darkman-medium.onnx.sha256"),
            "not-a-checksum\n",
        )
        .unwrap();
        let model = catalog.inspect("piper/pl_PL-darkman-medium").unwrap();
        assert!(model.checksum_sha256.is_none());
        assert_eq!(model.status, ModelAvailability::Available);
    }

    #[test]
    fn selection_is_persisted_and_survives_a_new_catalog_instance() {
        let root = TempRoot::new("selection");
        write_piper(&root, "pl_PL-darkman-medium", 4096, true);
        let catalog = root.catalog();
        let selection = catalog.select("piper/pl_PL-darkman-medium").unwrap();
        assert_eq!(selection.kind, ModelKind::PiperTts);
        assert!(catalog.selection_path().is_file());

        let reopened = ModelCatalog::new(catalog.root().to_path_buf());
        assert_eq!(reopened.selected().unwrap().id, "piper/pl_PL-darkman-medium");
        assert_eq!(reopened.active().unwrap().path, selection.path);
        assert_eq!(reopened.status().available, 1);

        // Kolejny wybór nadpisuje poprzedni, a nie dokleja.
        write_piper(&root, "pl_PL-gosia-medium", 4096, true);
        reopened.select("piper/pl_PL-gosia-medium").unwrap();
        assert_eq!(reopened.active().unwrap().id, "piper/pl_PL-gosia-medium");
    }

    #[test]
    fn clearing_selection_removes_the_persisted_choice() {
        let root = TempRoot::new("clear");
        write_piper(&root, "pl_PL-darkman-medium", 4096, true);
        let catalog = root.catalog();
        assert!(!catalog.clear_selection().unwrap(), "nie było czego usuwać");

        catalog.select("piper/pl_PL-darkman-medium").unwrap();
        assert!(catalog.clear_selection().unwrap());
        assert!(catalog.selected().is_none());
        assert!(catalog.active().is_none());
        assert!(!catalog.selection_path().exists());
        assert!(!catalog.clear_selection().unwrap());
    }

    #[test]
    fn selecting_unknown_model_keeps_the_previous_choice() {
        let root = TempRoot::new("unknown");
        write_piper(&root, "pl_PL-darkman-medium", 4096, true);
        let catalog = root.catalog();
        catalog.select("piper/pl_PL-darkman-medium").unwrap();

        assert!(matches!(
            catalog.select("piper/does-not-exist").unwrap_err(),
            CatalogError::NotFound(_)
        ));
        assert!(matches!(
            catalog.select("unknown/thing").unwrap_err(),
            CatalogError::NotFound(_)
        ));
        assert_eq!(catalog.active().unwrap().id, "piper/pl_PL-darkman-medium");
    }

    #[test]
    fn selection_of_a_deleted_model_is_reported_as_unavailable() {
        let root = TempRoot::new("deleted");
        let onnx = write_piper(&root, "pl_PL-darkman-medium", 4096, true);
        let catalog = root.catalog();
        catalog.select("piper/pl_PL-darkman-medium").unwrap();

        fs::remove_file(&onnx).unwrap();
        assert!(catalog.active().is_none(), "usunięty model nie może być aktywny");
        assert_eq!(catalog.selected().unwrap().id, "piper/pl_PL-darkman-medium");

        let status = catalog.status();
        assert!(status.active.is_none());
        assert!(status.selected.is_some());
        assert!(status.message().contains("nie jest już dostępny"));
    }

    #[test]
    fn whisper_llm_and_voice_models_share_one_catalog() {
        let root = TempRoot::new("all-kinds");
        let catalog = root.catalog();
        catalog.ensure_dirs().unwrap();

        // Whisper w układzie katalogowym.
        let whisper_dir = catalog.kind_dir(ModelKind::WhisperAsr).join("faster-whisper-small");
        fs::create_dir_all(&whisper_dir).unwrap();
        fs::write(whisper_dir.join("model.bin"), vec![7u8; 2048]).unwrap();
        fs::write(
            whisper_dir.join("config.json"),
            r#"{"language":["pl","en"],"license":"MIT"}"#,
        )
        .unwrap();

        // Lokalny LLM: GGUF bez sidecara ma być dostępny.
        fs::write(
            catalog.kind_dir(ModelKind::LocalLlm).join("qwen2.5-3b-instruct-q4.gguf"),
            vec![3u8; 4096],
        )
        .unwrap();

        // Przyszły model głosu: poprawny i uszkodzony JSON.
        fs::write(
            catalog.kind_dir(ModelKind::Voice).join("narrator.json"),
            r#"{"language":"pl","license":"CC0-1.0"}"#,
        )
        .unwrap();
        fs::write(
            catalog.kind_dir(ModelKind::Voice).join("broken.json"),
            "{not json",
        )
        .unwrap();

        let models = catalog.scan();
        assert_eq!(models.len(), 4);

        let whisper = catalog.inspect("whisper/faster-whisper-small").unwrap();
        assert_eq!(whisper.status, ModelAvailability::Available);
        assert_eq!(whisper.format, ModelFormat::Binary);
        assert_eq!(whisper.language.as_deref(), Some("pl,en"));
        assert_eq!(whisper.license.as_deref(), Some("MIT"));

        let llm = catalog.inspect("llm/qwen2.5-3b-instruct-q4").unwrap();
        assert_eq!(llm.status, ModelAvailability::Available);
        assert_eq!(llm.format, ModelFormat::Gguf);
        assert!(llm.language.is_none());

        let voice = catalog.inspect("voices/narrator").unwrap();
        assert_eq!(voice.status, ModelAvailability::Available);
        assert_eq!(voice.format, ModelFormat::Json);
        assert_eq!(voice.license.as_deref(), Some("CC0-1.0"));

        let broken = catalog.inspect("voices/broken").unwrap();
        assert_eq!(broken.status, ModelAvailability::Corrupt);

        let status = catalog.status();
        assert_eq!(status.total, 4);
        assert_eq!(status.available, 3);

        // Każdy rodzaj ma własny katalog.
        for kind in ModelKind::ALL {
            assert!(catalog.kind_dir(kind).starts_with(catalog.root()));
            assert_ne!(catalog.kind_dir(kind), catalog.root());
        }
        assert_ne!(
            catalog.kind_dir(ModelKind::PiperTts),
            catalog.kind_dir(ModelKind::WhisperAsr)
        );
    }

    #[test]
    fn whisper_without_config_is_missing_metadata() {
        let root = TempRoot::new("whisper-noconf");
        let catalog = root.catalog();
        catalog.ensure_dirs().unwrap();
        fs::write(
            catalog.kind_dir(ModelKind::WhisperAsr).join("tiny.bin"),
            vec![1u8; 2048],
        )
        .unwrap();

        let model = catalog.inspect("whisper/tiny").unwrap();
        assert_eq!(model.status, ModelAvailability::MissingMetadata);
    }

    #[test]
    fn catalog_module_has_no_network_path() {
        // Kontrakt offline: katalog nie może zależeć od HTTP ani wywoływać curl.
        // Sprawdzamy to na źródle modułu (test jest tańszy niż analiza procesu).
        let source = include_str!("model_catalog.rs");
        let production = source.split("#[cfg(test)]").next().unwrap_or(source);
        for forbidden in [
            "reqwest",
            "ureq",
            "urllib",
            "http://",
            "https://",
            "std::net",
            "TcpStream",
            "Command::new",
        ] {
            assert!(
                !production.contains(forbidden),
                "moduł katalogu nie może zawierać '{forbidden}'"
            );
        }
    }
}
