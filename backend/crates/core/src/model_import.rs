//! M18: dodawanie modeli — presety, import lokalny i pobieranie na żądanie.
//!
//! Trzy ścieżki, jeden kontrakt:
//!
//! 1. **Presety kurowane** (`model_sources::builtin_presets`) — tylko opis;
//!    nic nie jest pobierane, dopóki użytkownik jawnie tego nie zażąda.
//! 2. **Import lokalny** — plik, katalog albo ZIP; walidacja formatu, metadanych,
//!    licencji i opcjonalnego checksumu. Pliki z modelu **nigdy** nie są
//!    uruchamiane: dozwolone są wyłącznie rozszerzenia modeli, a skrypty i
//!    binaria wykonywalne są odrzucane.
//! 3. **Pobranie zdalne** — GitHub Releases / Hugging Face, wyłącznie przez
//!    `RemoteTransport` i wyłącznie z `ImportConsent::Explicit`. Domyślnie
//!    transportu nie ma, więc w tej wersji żadna ścieżka nie otwiera sieci
//!    sama z siebie; brak transportu to jawny błąd, nie ciche pobieranie.
//!
//! Zasady bezpieczeństwa:
//!
//! * pobieranie i import zapisują do `<models>/.staging/<import>/`, więc
//!   niedokończony model jest widoczny jako `incomplete` i nigdy jako gotowy,
//! * każda nazwa pliku jest walidowana (bez `/`, `\`, `..`, znaków kontrolnych),
//! * ZIP jest rozpakowywany z odrzuceniem wpisów wychodzących poza katalog,
//! * SHA-256 jest liczony po pobraniu; niezgodność usuwa cały staging,
//! * anulowanie usuwa staging i nie zostawia plików docelowych.

use crate::model_catalog::{
    CatalogModel, ModelAvailability, ModelCatalog, ModelFormat, ModelKind, StagingIntent,
    STAGING_INTENT_FILE,
};
use crate::model_sources::{
    describe_remote_reference, is_allowed_download_url, RemoteCandidate, SourceError,
};
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::fs::{self, File};
use std::io::{Read, Write};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use thiserror::Error;

/// Poniżej tego rozmiaru plik wag jest obcięty (spójne z katalogiem).
const MIN_WEIGHT_BYTES: u64 = 1024;
/// Górny limit pojedynczego pliku rozpakowywanego z ZIP (ochrona przed bombą).
const MAX_ARCHIVE_ENTRY_BYTES: u64 = 4096 * 1024 * 1024;
/// Maksymalna liczba plików w jednym imporcie.
const MAX_SOURCE_FILES: usize = 16;

// ============================================================================
// Zgoda, status, błędy
// ============================================================================

/// Jawna zgoda użytkownika. Pobranie bez niej jest błędem, nie „cichym trybem”.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ImportConsent {
    NotGiven,
    Explicit,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ImportStatus {
    Completed,
    Cancelled,
    Failed,
}

impl ImportStatus {
    pub fn as_str(self) -> &'static str {
        match self {
            ImportStatus::Completed => "completed",
            ImportStatus::Cancelled => "cancelled",
            ImportStatus::Failed => "failed",
        }
    }
}

/// Wynik jednego importu/pobrania.
#[derive(Debug, Clone, PartialEq)]
pub struct ImportOutcome {
    pub import_id: u64,
    pub status: ImportStatus,
    /// Model po ukończeniu (None dla anulowania/błędu).
    pub model: Option<CatalogModel>,
    pub message: String,
    pub source: String,
}

#[derive(Error, Debug)]
pub enum ImportError {
    #[error("pobranie wymaga jawnego potwierdzenia użytkownika (brak zgody)")]
    ConsentRequired,

    #[error("nieznane odwołanie do źródła: {0}")]
    UnknownReference(String),

    #[error("brak włączonego transportu sieciowego: {0}")]
    NetworkUnavailable(String),

    #[error("pobieranie zostało anulowane przez użytkownika")]
    Cancelled,

    #[error("sieć: {0}")]
    Network(String),

    #[error("checksum SHA-256 nie zgadza się: oczekiwano {expected}, otrzymano {actual}")]
    ChecksumMismatch { expected: String, actual: String },

    #[error("rozmiar pliku nie zgadza się: oczekiwano {expected} B, otrzymano {actual} B")]
    SizeMismatch { expected: u64, actual: u64 },

    #[error("niedozwolony format pliku: {0}")]
    ForbiddenFormat(String),

    #[error("niedozwolona nazwa lub ścieżka: {0}")]
    UnsafePath(String),

    #[error("model '{0}' już istnieje w katalogu (usuń go przed ponownym importem)")]
    AlreadyExists(String),

    #[error("źródło zdalne nie ma kompletnych metadanych: {0}")]
    UnverifiedSource(String),

    #[error("walidacja modelu nie powiodła się: {0}")]
    InvalidModel(String),

    #[error("licencja modelu jest nieznana — wymagana jawna akceptacja")]
    LicenseUnknown,

    #[error("model '{0}' nie istnieje w katalogu")]
    NotFound(String),

    #[error("błąd we/wy: {0}")]
    Io(String),
}

impl From<std::io::Error> for ImportError {
    fn from(err: std::io::Error) -> Self {
        ImportError::Io(err.to_string())
    }
}

impl From<SourceError> for ImportError {
    fn from(err: SourceError) -> Self {
        ImportError::UnknownReference(err.to_string())
    }
}

/// Błąd transportu. `Cancelled` przerywa transfer i sprząta staging.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum TransportError {
    Network(String),
    HttpStatus(u16),
    Cancelled,
    Io(String),
}

impl From<TransportError> for ImportError {
    fn from(err: TransportError) -> Self {
        match err {
            TransportError::Cancelled => ImportError::Cancelled,
            TransportError::Network(msg) => ImportError::Network(msg),
            TransportError::HttpStatus(code) => ImportError::Network(format!("HTTP {code}")),
            TransportError::Io(msg) => ImportError::Io(msg),
        }
    }
}

// ============================================================================
// Transport sieciowy (jedyne miejsce, które może czytać bajty z sieci)
// ============================================================================

/// Warstwa transportu dla pobrań zdalnych.
///
/// Implementacja produkcyjna (HTTPS) jest świadomie poza tym etapem: M18 nie
/// dodaje zależności HTTP i nie wykonuje żadnego transferu. Rdzeń dostarcza
/// pełny, przetestowany potok (staging, walidacja, checksum, anulowanie), a
/// transport jest wstrzykiwany tylko wtedy, gdy aplikacja go dostarczy.
pub trait RemoteTransport: Send + Sync {
    /// Pobiera `url` i przekazuje kolejne fragmenty do `sink`.
    fn fetch(
        &self,
        url: &str,
        sink: &mut dyn FnMut(&[u8]) -> Result<(), TransportError>,
    ) -> Result<(), TransportError>;
}

// ============================================================================
// Rejestr importów i anulowanie
// ============================================================================

/// Rejestr trwających importów: pozwala anulować pobranie z innego wątku.
#[derive(Debug, Default)]
pub struct ImportRegistry {
    next: AtomicU64,
    active: Mutex<Vec<(u64, Arc<AtomicBool>)>>,
}

impl ImportRegistry {
    pub fn reserve(&self) -> u64 {
        self.next.fetch_add(1, Ordering::SeqCst) + 1
    }

    fn register(&self, id: u64) -> Arc<AtomicBool> {
        let token = Arc::new(AtomicBool::new(false));
        if let Ok(mut active) = self.active.lock() {
            active.push((id, Arc::clone(&token)));
        }
        token
    }

    fn release(&self, id: u64) {
        if let Ok(mut active) = self.active.lock() {
            active.retain(|(other, _)| *other != id);
        }
    }

    /// Ustawia flagę anulowania. `false`, gdy import już się zakończył.
    pub fn cancel(&self, id: u64) -> bool {
        if let Ok(active) = self.active.lock() {
            if let Some((_, token)) = active.iter().find(|(other, _)| *other == id) {
                token.store(true, Ordering::SeqCst);
                return true;
            }
        }
        false
    }

    /// Id najstarszego trwającego importu (0 = brak) — uchwyt dla `cancel`.
    pub fn active_import_id(&self) -> u64 {
        self.active
            .lock()
            .ok()
            .and_then(|active| active.iter().map(|(id, _)| *id).min())
            .unwrap_or(0)
    }
}

// ============================================================================
// Żądania i wyniki
// ============================================================================

/// Żądanie pobrania zdalnego. `consent` musi być `Explicit`.
#[derive(Debug, Clone, PartialEq)]
pub struct RemoteDownloadRequest {
    pub reference: String,
    /// Oczekiwany SHA-256 (wymagany dla źródeł spoza presetów).
    pub expected_sha256: Option<String>,
    /// Oczekiwany rozmiar w bajtach (0 = weź deklarowany przez źródło).
    pub expected_size: u64,
    pub consent: ImportConsent,
}

impl RemoteDownloadRequest {
    pub fn new(reference: impl Into<String>) -> Self {
        Self {
            reference: reference.into(),
            expected_sha256: None,
            expected_size: 0,
            consent: ImportConsent::NotGiven,
        }
    }

    pub fn with_consent(mut self) -> Self {
        self.consent = ImportConsent::Explicit;
        self
    }

    pub fn with_expected_sha256(mut self, sha256: impl Into<String>) -> Self {
        self.expected_sha256 = Some(sha256.into());
        self
    }

    pub fn with_expected_size(mut self, size: u64) -> Self {
        self.expected_size = size;
        self
    }
}

/// Wynik weryfikacji checksumu.
#[derive(Debug, Clone, PartialEq)]
pub struct VerifyResult {
    pub id: String,
    pub kind: ModelKind,
    pub status: ModelAvailability,
    /// Czy było z czym porównywać (czy istnieje sidecar `.sha256`).
    pub checked: bool,
    pub expected_sha256: Option<String>,
    pub actual_sha256: Option<String>,
    /// `Some(true/false)` tylko gdy `checked`.
    pub matches: Option<bool>,
    pub size_bytes: u64,
    pub message: String,
}

/// Wynik usunięcia modelu.
#[derive(Debug, Clone, PartialEq)]
pub struct RemoveResult {
    pub id: String,
    pub kind: ModelKind,
    pub removed_files: Vec<PathBuf>,
    pub selection_cleared: bool,
}

// ============================================================================
// Importer
// ============================================================================

/// Jedyny punkt wejścia do dodawania i usuwania modeli.
pub struct ModelImporter {
    catalog: ModelCatalog,
    transport: Option<Arc<dyn RemoteTransport>>,
    registry: Arc<ImportRegistry>,
}

impl ModelImporter {
    /// Importer bez sieci: pobrania kończą się jawnym `NetworkUnavailable`.
    pub fn new(catalog: ModelCatalog) -> Self {
        Self {
            catalog,
            transport: None,
            registry: Arc::new(ImportRegistry::default()),
        }
    }

    /// Importer z wstrzykniętym transportem (testy, przyszły klient HTTPS).
    pub fn with_transport(catalog: ModelCatalog, transport: Arc<dyn RemoteTransport>) -> Self {
        Self {
            catalog,
            transport: Some(transport),
            registry: Arc::new(ImportRegistry::default()),
        }
    }

    pub fn catalog(&self) -> &ModelCatalog {
        &self.catalog
    }

    pub fn has_network(&self) -> bool {
        self.transport.is_some()
    }

    pub fn registry(&self) -> &Arc<ImportRegistry> {
        &self.registry
    }

    pub fn cancel(&self, import_id: u64) -> bool {
        self.registry.cancel(import_id)
    }

    pub fn active_import_id(&self) -> u64 {
        self.registry.active_import_id()
    }

    // ---- import lokalny ---------------------------------------------------

    /// Importuje **jeden** model z pliku, katalogu lub archiwum ZIP.
    ///
    /// `kind = None` pozwala wywnioskować rodzaj z rozszerzenia. Import jest
    /// atomowy: przy każdym błędzie staging jest usuwany, a katalog docelowy
    /// pozostaje nietknięty.
    pub fn import_local(
        &self,
        source: &Path,
        kind: Option<ModelKind>,
        accept_unknown_license: bool,
    ) -> Result<CatalogModel, ImportError> {
        let import_id = self.registry.reserve();
        let staging = self
            .catalog
            .staging_root()
            .join(format!("local-{import_id:06}"));
        let result = self.import_local_inner(source, kind, accept_unknown_license, import_id, &staging);
        if result.is_err() {
            cleanup_dir(&staging);
        }
        result
    }

    fn import_local_inner(
        &self,
        source: &Path,
        kind: Option<ModelKind>,
        accept_unknown_license: bool,
        import_id: u64,
        staging: &Path,
    ) -> Result<CatalogModel, ImportError> {
        let meta = fs::symlink_metadata(source)
            .map_err(|err| ImportError::UnsafePath(format!("{}: {err}", source.display())))?;
        if meta.file_type().is_symlink() {
            return Err(ImportError::UnsafePath(format!(
                "{} jest dowiązaniem symbolicznym",
                source.display()
            )));
        }

        fs::create_dir_all(staging.join("out"))?;
        let work_in = staging.join("in");

        // 1. Zbierz pliki: plik, katalog albo ZIP rozpakowany do `in/`.
        let files: Vec<(PathBuf, String)> = if meta.is_dir() {
            collect_dir(source)?
        } else if is_zip(source) {
            fs::create_dir_all(&work_in)?;
            extract_zip_safe(source, &work_in)?;
            collect_dir(&work_in)?
        } else {
            vec![(source.to_path_buf(), file_name_of(source)?)]
        };
        if files.is_empty() {
            return Err(ImportError::InvalidModel(
                "źródło nie zawiera żadnego pliku modelu".to_string(),
            ));
        }

        let kind = match kind {
            Some(kind) => kind,
            None => infer_kind(&files).ok_or_else(|| {
                ImportError::ForbiddenFormat(
                    "nie można rozpoznać rodzaju modelu (oczekiwano .onnx, .bin, .gguf lub .json)"
                        .to_string(),
                )
            })?,
        };
        ensure_kind_files(kind, &files)?;

        // 2. Nazwa docelowa i pliki towarzyszące.
        let layout = resolve_layout(kind, source, &files)?;
        for (_, name) in &layout.targets {
            validate_file_name(name)?;
        }
        if layout.targets.is_empty() {
            return Err(ImportError::InvalidModel(
                "brak pliku wag w źródle".to_string(),
            ));
        }

        // 3. Walidacja zawartości i checksumu **przed** skopiowaniem.
        let weights_source = &files[layout.weights_index].0;
        validate_weight_header(kind, weights_source)?;
        let weights_size = fs::metadata(weights_source)?.len();
        if weights_size < MIN_WEIGHT_BYTES {
            return Err(ImportError::InvalidModel(format!(
                "plik wag jest za mały/obcięty ({weights_size} B)"
            )));
        }
        verify_source_checksum(&files, &layout.weights_name)?;

        // 4. Licencja: brak danych wymaga jawnej akceptacji.
        let license = layout
            .license_source
            .as_ref()
            .and_then(|path| fs::read_to_string(path).ok())
            .and_then(|text| extract_license(&text));
        if license.is_none() && !accept_unknown_license {
            return Err(ImportError::LicenseUnknown);
        }

        // 5. Intencja importu + kopie robocze, potem atomowe przeniesienie.
        write_intent(
            staging,
            StagingIntent {
                import_id,
                id: layout.id.clone(),
                kind: kind.as_str().to_string(),
                format: layout.format.as_str().to_string(),
                files: layout.targets.iter().map(|(_, name)| name.clone()).collect(),
                source: format!("local:{}", source.display()),
                started_at_utc: utc_timestamp(),
            },
        )?;

        for (source_path, target_name) in &layout.targets {
            let target = staging.join("out").join(target_name);
            fs::copy(source_path, &target)?;
        }
        let target_names: Vec<String> = layout.targets.iter().map(|(_, name)| name.clone()).collect();
        commit_staging(staging, &self.catalog, kind, &target_names)?;

        let model = self.catalog.inspect(&layout.id).ok_or_else(|| {
            ImportError::InvalidModel(format!("model '{}' nie pojawił się w katalogu", layout.id))
        })?;
        if !model.is_available() {
            cleanup_targets(&self.catalog, kind, &target_names);
            return Err(ImportError::InvalidModel(format!(
                "{}: {}",
                model.status.as_str(),
                model.detail
            )));
        }

        cleanup_dir(&staging.join("in"));
        cleanup_dir(&staging.join("out"));
        cleanup_dir(staging);
        Ok(model)
    }

    // ---- pobranie zdalne --------------------------------------------------

    /// Pobiera model **wyłącznie** po jawnej zgodzie i tylko gdy transport
    /// sieciowy został wstrzyknięty. Każdy błąd usuwa staging.
    pub fn download(&self, request: &RemoteDownloadRequest) -> Result<ImportOutcome, ImportError> {
        if request.consent != ImportConsent::Explicit {
            return Err(ImportError::ConsentRequired);
        }

        let candidate = describe_remote_reference(&request.reference)?;
        if candidate.files.is_empty() {
            return Err(ImportError::UnverifiedSource(
                "źródło nie opisuje żadnego pliku".to_string(),
            ));
        }
        for file in &candidate.files {
            validate_file_name(&file.file_name)?;
            if !is_allowed_download_url(&file.url) {
                return Err(ImportError::UnsafePath(format!(
                    "adres nie jest na liście dozwolonych hostów: {}",
                    file.url
                )));
            }
        }

        let expected_sha = match request.expected_sha256.as_deref() {
            Some(value) => Some(normalize_sha256(value)?),
            None => candidate
                .weights_file()
                .and_then(|file| file.sha256.clone())
                .map(|value| normalize_sha256(&value))
                .transpose()?,
        };

        // Rozmiar deklarowany przez preset jest przybliżony i służy tylko do
        // pokazania w UI; twardo egzekwujemy wyłącznie rozmiar podany w żądaniu.
        let expected_size = request.expected_size;

        if !candidate.curated {
            if expected_sha.is_none() {
                return Err(ImportError::UnverifiedSource(
                    "pobranie spoza presetów wymaga zadeklarowanego SHA-256".to_string(),
                ));
            }
            if expected_size == 0 {
                return Err(ImportError::UnverifiedSource(
                    "pobranie spoza presetów wymaga zadeklarowanego rozmiaru".to_string(),
                ));
            }
        }

        let transport = self.transport.as_ref().ok_or_else(|| {
            ImportError::NetworkUnavailable(
                "aplikacja nie dostarczyła klienta HTTPS; żadne dane nie zostały pobrane"
                    .to_string(),
            )
        })?;

        let import_id = self.registry.reserve();
        let token = self.registry.register(import_id);
        let staging = self
            .catalog
            .staging_root()
            .join(format!("{import_id:06}-{}", slug(&candidate.id)));

        let outcome = self.download_inner(
            &candidate,
            request,
            transport,
            &token,
            import_id,
            &staging,
            expected_sha.as_deref(),
            expected_size,
        );
        self.registry.release(import_id);
        match outcome {
            Ok(model) => Ok(ImportOutcome {
                import_id,
                status: ImportStatus::Completed,
                model: Some(model),
                message: format!("zainstalowano {} ({})", candidate.id, candidate.name),
                source: candidate.source_reference.clone(),
            }),
            Err(ImportError::Cancelled) => {
                cleanup_dir(&staging);
                Ok(ImportOutcome {
                    import_id,
                    status: ImportStatus::Cancelled,
                    model: None,
                    message: "pobieranie anulowane; pliki tymczasowe usunięte".to_string(),
                    source: candidate.source_reference.clone(),
                })
            }
            Err(err) => {
                cleanup_dir(&staging);
                Err(err)
            }
        }
    }

    #[allow(clippy::too_many_arguments)]
    fn download_inner(
        &self,
        candidate: &RemoteCandidate,
        request: &RemoteDownloadRequest,
        transport: &Arc<dyn RemoteTransport>,
        token: &Arc<AtomicBool>,
        import_id: u64,
        staging: &Path,
        expected_sha: Option<&str>,
        expected_size: u64,
    ) -> Result<CatalogModel, ImportError> {
        fs::create_dir_all(staging.join("out"))?;
        write_intent(
            staging,
            StagingIntent {
                import_id,
                id: candidate.id.clone(),
                kind: candidate.kind.as_str().to_string(),
                format: candidate.format.as_str().to_string(),
                files: candidate.file_names(),
                source: request.reference.clone(),
                started_at_utc: utc_timestamp(),
            },
        )?;

        let mut total_written = 0u64;
        for file in &candidate.files {
            if token.load(Ordering::SeqCst) {
                return Err(ImportError::Cancelled);
            }
            let part = staging.join("out").join(format!("{}.part", file.file_name));
            let mut output = File::create(&part)?;
            let mut written = 0u64;
            {
                let mut sink = |chunk: &[u8]| -> Result<(), TransportError> {
                    if token.load(Ordering::SeqCst) {
                        return Err(TransportError::Cancelled);
                    }
                    output
                        .write_all(chunk)
                        .map_err(|err| TransportError::Io(err.to_string()))?;
                    written += chunk.len() as u64;
                    Ok(())
                };
                transport.fetch(&file.url, &mut sink)?;
            }
            output.sync_all().ok();

            if token.load(Ordering::SeqCst) {
                return Err(ImportError::Cancelled);
            }
            if file.size_bytes > 0 && written != file.size_bytes && candidate.curated {
                // Deklarowany rozmiar presetu jest przybliżony — nie odrzucamy
                // kompletnego pliku tylko dlatego, że producent podał inny.
            }
            total_written += written;
        }

        if expected_size > 0 && total_written != expected_size {
            return Err(ImportError::SizeMismatch {
                expected: expected_size,
                actual: total_written,
            });
        }

        // Checksum po pobraniu: brak zadeklarowanej sumy = liczymy i zapisujemy
        // własną (punkt odniesienia dla `verify`), zadeklarowana = musi się zgadzać.
        let weights = candidate
            .weights_file()
            .ok_or_else(|| ImportError::InvalidModel("brak pliku wag".to_string()))?;
        let weights_part = staging.join("out").join(format!("{}.part", weights.file_name));
        let actual = sha256_file(&weights_part)?;
        if let Some(expected) = expected_sha {
            if !expected.eq_ignore_ascii_case(&actual) {
                return Err(ImportError::ChecksumMismatch {
                    expected: expected.to_string(),
                    actual,
                });
            }
        }

        let kind_dir = self.catalog.kind_dir(candidate.kind);
        fs::create_dir_all(&kind_dir)?;
        let names: Vec<String> = candidate.file_names();
        for name in &names {
            if kind_dir.join(name).exists() {
                return Err(ImportError::AlreadyExists(candidate.id.clone()));
            }
        }

        for file in &candidate.files {
            let part = staging.join("out").join(format!("{}.part", file.file_name));
            let target = kind_dir.join(&file.file_name);
            fs::rename(&part, target)?;
        }
        write_checksum_sidecar(&kind_dir.join(&weights.file_name), &actual)?;
        cleanup_dir(staging);

        let model = self
            .catalog
            .inspect(&candidate.id)
            .ok_or_else(|| ImportError::InvalidModel(candidate.id.clone()))?;
        if !model.is_available() {
            cleanup_targets(&self.catalog, candidate.kind, &names);
            return Err(ImportError::InvalidModel(format!(
                "{}: {}",
                model.status.as_str(),
                model.detail
            )));
        }
        Ok(model)
    }

    // ---- weryfikacja i usuwanie -------------------------------------------

    /// Przelicza SHA-256 modelu i porównuje z zapisanym sidecarem.
    pub fn verify(&self, id: &str) -> Result<VerifyResult, ImportError> {
        let model = self
            .catalog
            .inspect(id)
            .ok_or_else(|| ImportError::NotFound(id.to_string()))?;
        if model.status == ModelAvailability::Incomplete || model.path.is_dir() {
            return Ok(VerifyResult {
                id: model.id,
                kind: model.kind,
                status: model.status,
                checked: false,
                expected_sha256: None,
                actual_sha256: None,
                matches: None,
                size_bytes: model.size_bytes,
                message: "import nieukończony — brak pliku do weryfikacji".to_string(),
            });
        }

        let actual = sha256_file(&model.path)?;
        let expected = model.checksum_sha256.clone();
        let matches = expected
            .as_deref()
            .map(|value| value.eq_ignore_ascii_case(&actual));
        Ok(VerifyResult {
            id: model.id,
            kind: model.kind,
            status: model.status,
            checked: expected.is_some(),
            expected_sha256: expected,
            actual_sha256: Some(actual),
            matches,
            size_bytes: model.size_bytes,
            message: match matches {
                Some(true) => "checksum zgodny".to_string(),
                Some(false) => "checksum NIE zgadza się z zapisanym sidecarem".to_string(),
                None => "brak zapisanego checksumu — obliczono tylko sumę bieżącą".to_string(),
            },
        })
    }

    /// Usuwa model z dysku (wagi, konfigurację, checksumy). Nie rusza innych
    /// modeli ani plików spoza katalogu rodzaju.
    pub fn remove(&self, id: &str) -> Result<RemoveResult, ImportError> {
        validate_catalog_id(id)?;
        let model = self
            .catalog
            .inspect(id)
            .ok_or_else(|| ImportError::NotFound(id.to_string()))?;

        let selection_cleared = self
            .catalog
            .selected()
            .map(|sel| sel.id == id)
            .unwrap_or(false);
        if selection_cleared {
            self.catalog
                .clear_selection()
                .map_err(|err| ImportError::Io(err.to_string()))?;
        }

        if model.status == ModelAvailability::Incomplete {
            cleanup_dir(&model.path);
            return Ok(RemoveResult {
                id: model.id,
                kind: model.kind,
                removed_files: vec![model.path],
                selection_cleared,
            });
        }

        let kind_dir = self.catalog.kind_dir(model.kind);
        let mut removed = Vec::new();

        // Układ katalogowy Whispera: usuwamy cały podkatalog modelu.
        if model.path.parent() != Some(kind_dir.as_path()) {
            if let (Some(dir), Some(parent)) = (model.path.parent(), model.path.parent()) {
                if parent.parent() == Some(kind_dir.as_path()) {
                    cleanup_dir(dir);
                    removed.push(dir.to_path_buf());
                    return Ok(RemoveResult {
                        id: model.id,
                        kind: model.kind,
                        removed_files: removed,
                        selection_cleared,
                    });
                }
            }
        }

        for file in self.catalog.model_files(&model.id).unwrap_or_default() {
            let inside = file.parent() == Some(kind_dir.as_path());
            if inside && file.is_file() {
                if fs::remove_file(&file).is_ok() {
                    removed.push(file);
                }
            }
        }
        Ok(RemoveResult {
            id: model.id,
            kind: model.kind,
            removed_files: removed,
            selection_cleared,
        })
    }
}

// ============================================================================
// Pomocnicze: ścieżki i nazwy
// ============================================================================

fn file_name_of(path: &Path) -> Result<String, ImportError> {
    let name = path
        .file_name()
        .and_then(|s| s.to_str())
        .ok_or_else(|| ImportError::UnsafePath(path.display().to_string()))?
        .to_string();
    validate_file_name(&name)?;
    Ok(name)
}

/// Nazwa pliku bez ścieżki — jedyna forma, jaką przyjmujemy z zewnątrz.
pub fn validate_file_name(name: &str) -> Result<(), ImportError> {
    let bad = name.is_empty()
        || name == "."
        || name == ".."
        || name.starts_with('.')
        || name.starts_with('/')
        || name.contains('/')
        || name.contains('\\')
        || name.chars().any(|c| c.is_control())
        || !name
            .chars()
            .all(|c| c.is_ascii_alphanumeric() || matches!(c, '.' | '_' | '-' | '+'));
    if bad {
        return Err(ImportError::UnsafePath(format!(
            "niedozwolona nazwa pliku: '{name}'"
        )));
    }
    Ok(())
}

/// Id katalogu (`kind/nazwa`) bez prób wyjścia z katalogu.
pub fn validate_catalog_id(id: &str) -> Result<(), ImportError> {
    let (kind, name) = id
        .split_once('/')
        .ok_or_else(|| ImportError::UnsafePath(format!("id bez rodzaju: '{id}'")))?;
    if ModelKind::parse(kind).is_none() {
        return Err(ImportError::UnsafePath(format!("nieznany rodzaj w id: '{id}'")));
    }
    validate_file_name(name)
}

fn is_zip(path: &Path) -> bool {
    path.extension()
        .and_then(|s| s.to_str())
        .is_some_and(|ext| ext.eq_ignore_ascii_case("zip"))
}

fn forbidden_extension(ext: &str) -> bool {
    matches!(
        ext,
        "py" | "pyc"
            | "sh"
            | "bash"
            | "zsh"
            | "bat"
            | "cmd"
            | "ps1"
            | "exe"
            | "dll"
            | "so"
            | "dylib"
            | "jar"
            | "js"
            | "mjs"
            | "cjs"
            | "hta"
            | "scr"
            | "com"
            | "msi"
            | "vbs"
            | "rb"
            | "pl"
            | "php"
            | "lua"
            | "app"
            | "dmg"
            | "pkg"
            | "elf"
    )
}

fn extension_of(name: &str) -> String {
    name.rsplit_once('.')
        .map(|(_, ext)| ext.to_lowercase())
        .unwrap_or_default()
}

fn collect_dir(root: &Path) -> Result<Vec<(PathBuf, String)>, ImportError> {
    let mut out = Vec::new();
    collect_dir_inner(root, 0, &mut out)?;
    if out.len() > MAX_SOURCE_FILES {
        return Err(ImportError::InvalidModel(format!(
            "źródło ma {} plików (limit {MAX_SOURCE_FILES})",
            out.len()
        )));
    }
    Ok(out)
}

fn collect_dir_inner(
    dir: &Path,
    depth: usize,
    out: &mut Vec<(PathBuf, String)>,
) -> Result<(), ImportError> {
    if depth > 4 {
        return Err(ImportError::UnsafePath(
            "zbyt głęboka struktura katalogów".to_string(),
        ));
    }
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let path = entry.path();
        let meta = fs::symlink_metadata(&path)?;
        if meta.file_type().is_symlink() {
            return Err(ImportError::UnsafePath(format!(
                "{} jest dowiązaniem symbolicznym",
                path.display()
            )));
        }
        if meta.is_dir() {
            collect_dir_inner(&path, depth + 1, out)?;
            continue;
        }
        let name = file_name_of(&path)?;
        let ext = extension_of(&name);
        if forbidden_extension(&ext) {
            return Err(ImportError::ForbiddenFormat(format!(
                "pliki wykonywalne i skrypty nie są importowane: {name}"
            )));
        }
        if out.iter().any(|(_, existing)| *existing == name) {
            return Err(ImportError::UnsafePath(format!(
                "zduplikowana nazwa pliku w źródle: {name}"
            )));
        }
        out.push((path, name));
    }
    Ok(())
}

/// Bezpieczne rozpakowanie ZIP: tylko nazwy bez ścieżek, bez `..`, z limitem
/// rozmiaru. Wpisy katalogowe są pomijane.
fn extract_zip_safe(archive: &Path, dest: &Path) -> Result<(), ImportError> {
    let file = File::open(archive)?;
    let mut zip = zip::ZipArchive::new(file)
        .map_err(|err| ImportError::ForbiddenFormat(format!("nieprawidłowy ZIP: {err}")))?;

    for index in 0..zip.len() {
        let mut entry = zip
            .by_index(index)
            .map_err(|err| ImportError::ForbiddenFormat(err.to_string()))?;
        if entry.is_dir() {
            continue;
        }
        let raw_name = entry
            .name()
            .map_err(|err| ImportError::ForbiddenFormat(format!("nieprawidłowa nazwa wpisu ZIP: {err}")))?
            .to_string();
        let enclosed = entry
            .enclosed_name()
            .ok_or_else(|| ImportError::UnsafePath(format!("wpis ZIP wychodzi z katalogu: {raw_name}")))?;
        let name = enclosed
            .file_name()
            .and_then(|s| s.to_str())
            .ok_or_else(|| ImportError::UnsafePath(raw_name.clone()))?
            .to_string();
        if raw_name.contains('/') || raw_name.contains('\\') {
            return Err(ImportError::UnsafePath(format!(
                "wpis ZIP zawiera ścieżkę: {raw_name}"
            )));
        }
        validate_file_name(&name)?;
        if forbidden_extension(&extension_of(&name)) {
            return Err(ImportError::ForbiddenFormat(format!(
                "archiwum zawiera skrypt lub plik wykonywalny: {name}"
            )));
        }
        if entry.size() > MAX_ARCHIVE_ENTRY_BYTES {
            return Err(ImportError::ForbiddenFormat(format!(
                "wpis ZIP jest za duży: {name}"
            )));
        }

        let target = dest.join(&name);
        if target.exists() {
            return Err(ImportError::UnsafePath(format!(
                "zduplikowany wpis ZIP: {name}"
            )));
        }
        let mut output = File::create(&target)?;
        let mut limited = (&mut entry).take(MAX_ARCHIVE_ENTRY_BYTES + 1);
        let copied = std::io::copy(&mut limited, &mut output)?;
        if copied > MAX_ARCHIVE_ENTRY_BYTES {
            return Err(ImportError::ForbiddenFormat(format!(
                "wpis ZIP przekracza limit rozmiaru: {name}"
            )));
        }
        output.flush()?;
    }
    Ok(())
}

fn infer_kind(files: &[(PathBuf, String)]) -> Option<ModelKind> {
    // Plik wag decyduje o rodzaju; `.json` jest tylko sidecarem konfiguracji,
    // a `.sha256` checksumem. JSON wygrywa dopiero, gdy nie ma innych wag.
    let mut extensions: Vec<String> = files.iter().map(|(_, name)| extension_of(name)).collect();
    extensions.sort();
    extensions.dedup();
    if extensions.iter().any(|ext| ext == "gguf") {
        return Some(ModelKind::LocalLlm);
    }
    if extensions.iter().any(|ext| ext == "onnx") {
        return Some(ModelKind::PiperTts);
    }
    if extensions.iter().any(|ext| ext == "bin") {
        return Some(ModelKind::WhisperAsr);
    }
    if extensions.iter().any(|ext| ext == "json") {
        return Some(ModelKind::Voice);
    }
    None
}

fn ensure_kind_files(kind: ModelKind, files: &[(PathBuf, String)]) -> Result<(), ImportError> {
    for (_, name) in files {
        let ext = extension_of(name);
        let allowed = match kind {
            ModelKind::PiperTts => matches!(ext.as_str(), "onnx" | "json" | "sha256"),
            ModelKind::WhisperAsr => matches!(ext.as_str(), "bin" | "json" | "sha256"),
            ModelKind::LocalLlm => matches!(ext.as_str(), "gguf" | "json" | "sha256"),
            ModelKind::Voice => matches!(ext.as_str(), "json" | "sha256"),
        };
        if !allowed {
            return Err(ImportError::ForbiddenFormat(format!(
                "plik '{name}' nie pasuje do rodzaju {}",
                kind.as_str()
            )));
        }
    }
    Ok(())
}

struct Layout {
    id: String,
    format: ModelFormat,
    weights_index: usize,
    weights_name: String,
    /// źródło → nazwa docelowa
    targets: Vec<(PathBuf, String)>,
    license_source: Option<PathBuf>,
}

fn resolve_layout(
    kind: ModelKind,
    source: &Path,
    files: &[(PathBuf, String)],
) -> Result<Layout, ImportError> {
    let find_by_ext = |ext: &str| -> Vec<usize> {
        files
            .iter()
            .enumerate()
            .filter(|(_, (_, name))| extension_of(name) == ext && !name.ends_with(".sha256"))
            .map(|(index, _)| index)
            .collect()
    };
    let sha_index = |name: &str| -> Option<usize> {
        files.iter().position(|(_, entry)| *entry == format!("{name}.sha256"))
    };

    match kind {
        ModelKind::PiperTts => {
            let weights = find_by_ext("onnx");
            if weights.len() != 1 {
                return Err(ImportError::InvalidModel(format!(
                    "oczekiwano dokładnie jednego pliku .onnx (jest {})",
                    weights.len()
                )));
            }
            let index = weights[0];
            let weights_name = files[index].1.clone();
            let stem = weights_name.trim_end_matches(".onnx").to_string();
            let config_name = format!("{stem}.onnx.json");
            let config_index = files
                .iter()
                .position(|(_, name)| *name == config_name)
                .ok_or_else(|| {
                    ImportError::InvalidModel(format!(
                        "brak pliku konfiguracji '{config_name}' — Piper bez niego jest niekompletny"
                    ))
                })?;
            let mut targets = vec![
                (files[index].0.clone(), weights_name.clone()),
                (files[config_index].0.clone(), config_name),
            ];
            if let Some(sha) = sha_index(&weights_name) {
                targets.push((files[sha].0.clone(), format!("{weights_name}.sha256")));
            }
            Ok(Layout {
                id: format!("piper/{stem}"),
                format: ModelFormat::Onnx,
                weights_index: index,
                weights_name,
                targets,
                license_source: Some(files[config_index].0.clone()),
            })
        }
        ModelKind::WhisperAsr => {
            let weights = find_by_ext("bin");
            if weights.len() != 1 {
                return Err(ImportError::InvalidModel(format!(
                    "oczekiwano dokładnie jednego pliku .bin (jest {})",
                    weights.len()
                )));
            }
            let index = weights[0];
            let weights_name = files[index].1.clone();
            let stem = if weights_name.eq_ignore_ascii_case("model.bin") {
                source
                    .file_stem()
                    .and_then(|s| s.to_str())
                    .map(|s| s.to_string())
                    .unwrap_or_else(|| "whisper-model".to_string())
            } else {
                weights_name.trim_end_matches(".bin").to_string()
            };
            validate_file_name(&stem)?;

            let config_index = files
                .iter()
                .position(|(_, name)| *name == format!("{stem}.json"))
                .or_else(|| files.iter().position(|(_, name)| *name == "config.json"));
            let mut targets = vec![(files[index].0.clone(), format!("{stem}.bin"))];
            if let Some(config) = config_index {
                targets.push((files[config].0.clone(), format!("{stem}.json")));
            }
            if let Some(sha) = sha_index(&weights_name) {
                targets.push((files[sha].0.clone(), format!("{stem}.bin.sha256")));
            }
            Ok(Layout {
                id: format!("whisper/{stem}"),
                format: ModelFormat::Binary,
                weights_index: index,
                weights_name,
                license_source: config_index.map(|i| files[i].0.clone()),
                targets,
            })
        }
        ModelKind::LocalLlm => {
            let weights = find_by_ext("gguf");
            if weights.len() != 1 {
                return Err(ImportError::InvalidModel(format!(
                    "oczekiwano dokładnie jednego pliku .gguf (jest {})",
                    weights.len()
                )));
            }
            let index = weights[0];
            let weights_name = files[index].1.clone();
            let stem = weights_name.trim_end_matches(".gguf").to_string();
            let sidecar_name = format!("{stem}.json");
            let sidecar = files.iter().position(|(_, name)| *name == sidecar_name);
            let mut targets = vec![(files[index].0.clone(), weights_name.clone())];
            if let Some(sidecar) = sidecar {
                targets.push((files[sidecar].0.clone(), sidecar_name));
            }
            if let Some(sha) = sha_index(&weights_name) {
                targets.push((files[sha].0.clone(), format!("{weights_name}.sha256")));
            }
            Ok(Layout {
                id: format!("llm/{stem}"),
                format: ModelFormat::Gguf,
                weights_index: index,
                weights_name,
                license_source: sidecar.map(|i| files[i].0.clone()),
                targets,
            })
        }
        ModelKind::Voice => {
            let json = find_by_ext("json");
            if json.len() != 1 {
                return Err(ImportError::InvalidModel(format!(
                    "oczekiwano dokładnie jednego pliku .json profilu głosu (jest {})",
                    json.len()
                )));
            }
            let index = json[0];
            let name = files[index].1.clone();
            let stem = name.trim_end_matches(".json").to_string();
            Ok(Layout {
                id: format!("voices/{stem}"),
                format: ModelFormat::Json,
                weights_index: index,
                weights_name: name.clone(),
                targets: vec![(files[index].0.clone(), name)],
                license_source: Some(files[index].0.clone()),
            })
        }
    }
}

fn validate_weight_header(kind: ModelKind, path: &Path) -> Result<(), ImportError> {
    let mut head = [0u8; 8];
    let mut file = File::open(path)?;
    let read = file.read(&mut head)?;
    let start = &head[..read];

    if start.starts_with(b"#!") || start.starts_with(b"\x7fELF") || start.starts_with(b"MZ") {
        return Err(ImportError::ForbiddenFormat(
            "plik wygląda na skrypt lub program wykonywalny".to_string(),
        ));
    }

    match kind {
        ModelKind::LocalLlm if !start.starts_with(b"GGUF") => {
            return Err(ImportError::ForbiddenFormat(
                "plik .gguf nie ma nagłówka GGUF".to_string(),
            ));
        }
        ModelKind::Voice => {
            let text = fs::read_to_string(path)?;
            serde_json::from_str::<serde_json::Value>(&text).map_err(|_| {
                ImportError::ForbiddenFormat("profil głosu nie jest poprawnym JSON-em".to_string())
            })?;
        }
        _ => {}
    }
    Ok(())
}

fn verify_source_checksum(
    files: &[(PathBuf, String)],
    weights_name: &str,
) -> Result<(), ImportError> {
    let sidecar_name = format!("{weights_name}.sha256");
    let Some((sidecar, _)) = files.iter().find(|(_, name)| *name == sidecar_name) else {
        return Ok(());
    };
    let expected = parse_checksum(&fs::read_to_string(sidecar)?).ok_or_else(|| {
        ImportError::ForbiddenFormat(format!("nieprawidłowy plik checksum: {sidecar_name}"))
    })?;
    let weights = files
        .iter()
        .find(|(_, name)| name == weights_name)
        .map(|(path, _)| path)
        .ok_or_else(|| ImportError::InvalidModel("brak pliku wag".to_string()))?;
    let actual = sha256_file(weights)?;
    if !expected.eq_ignore_ascii_case(&actual) {
        return Err(ImportError::ChecksumMismatch { expected, actual });
    }
    Ok(())
}

fn extract_license(json: &str) -> Option<String> {
    let value: serde_json::Value = serde_json::from_str(json).ok()?;
    let node = value.get("license")?;
    let text = match node {
        serde_json::Value::String(text) => text.trim().to_string(),
        serde_json::Value::Object(map) => map
            .get("spdx")
            .or_else(|| map.get("name"))
            .or_else(|| map.get("id"))
            .and_then(|v| v.as_str())
            .unwrap_or_default()
            .trim()
            .to_string(),
        _ => String::new(),
    };
    (!text.is_empty()).then_some(text)
}

// ============================================================================
// Pomocnicze: staging, checksum, czas
// ============================================================================

fn write_intent(staging: &Path, intent: StagingIntent) -> Result<(), ImportError> {
    fs::create_dir_all(staging)?;
    let json = serde_json::to_vec_pretty(&intent)
        .map_err(|err| ImportError::Io(err.to_string()))?;
    fs::write(staging.join(STAGING_INTENT_FILE), json)?;
    Ok(())
}

/// Przenosi pliki ze staging do katalogu rodzaju, odmawiając nadpisania.
fn commit_staging(
    staging: &Path,
    catalog: &ModelCatalog,
    kind: ModelKind,
    names: &[String],
) -> Result<(), ImportError> {
    let kind_dir = catalog.kind_dir(kind);
    fs::create_dir_all(&kind_dir)?;
    for name in names {
        if kind_dir.join(name).exists() {
            return Err(ImportError::AlreadyExists(name.clone()));
        }
    }
    for name in names {
        fs::rename(staging.join("out").join(name), kind_dir.join(name))?;
    }
    Ok(())
}

fn write_checksum_sidecar(weights: &Path, sha256: &str) -> Result<(), ImportError> {
    let mut sidecar = weights.to_path_buf().into_os_string();
    sidecar.push(".sha256");
    let name = weights
        .file_name()
        .and_then(|s| s.to_str())
        .unwrap_or("model");
    fs::write(PathBuf::from(sidecar), format!("{sha256}  {name}\n"))?;
    Ok(())
}

fn cleanup_dir(path: &Path) {
    let _ = fs::remove_dir_all(path);
}

fn cleanup_targets(catalog: &ModelCatalog, kind: ModelKind, names: &[String]) {
    let dir = catalog.kind_dir(kind);
    for name in names {
        let _ = fs::remove_file(dir.join(name));
        let _ = fs::remove_file(dir.join(format!("{name}.sha256")));
    }
}

fn parse_checksum(text: &str) -> Option<String> {
    let token = text.split_whitespace().next()?;
    is_sha256_hex(token).then(|| token.to_lowercase())
}

fn normalize_sha256(value: &str) -> Result<String, ImportError> {
    let trimmed = value.trim();
    if !is_sha256_hex(trimmed) {
        return Err(ImportError::ChecksumMismatch {
            expected: format!("nieprawidłowy SHA-256 '{trimmed}'"),
            actual: String::new(),
        });
    }
    Ok(trimmed.to_lowercase())
}

fn is_sha256_hex(value: &str) -> bool {
    value.len() == 64 && value.chars().all(|c| c.is_ascii_hexdigit())
}

fn sha256_file(path: &Path) -> Result<String, ImportError> {
    let mut hasher = Sha256::new();
    let mut file = File::open(path)?;
    let mut buffer = vec![0u8; 64 * 1024];
    loop {
        let read = file.read(&mut buffer)?;
        if read == 0 {
            break;
        }
        hasher.update(&buffer[..read]);
    }
    Ok(hex::encode(hasher.finalize()))
}

fn slug(value: &str) -> String {
    value
        .chars()
        .map(|c| if c.is_ascii_alphanumeric() { c } else { '-' })
        .collect::<String>()
        .trim_matches('-')
        .to_string()
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
    use crate::model_sources::{builtin_presets, list_sources};
    use std::sync::mpsc;
    use std::time::Duration;

    struct TempRoot {
        dir: PathBuf,
    }

    impl TempRoot {
        fn new(name: &str) -> Self {
            let dir = std::env::temp_dir().join(format!(
                "audioticon-m18-{name}-{}",
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

    fn write_piper_source(root: &TempRoot, stem: &str, bytes: usize, license: Option<&str>) -> PathBuf {
        let dir = root.path().join(format!("src-{stem}"));
        fs::create_dir_all(&dir).unwrap();
        fs::write(dir.join(format!("{stem}.onnx")), vec![0x08u8; bytes]).unwrap();
        let license = license
            .map(|value| format!(r#","license":"{value}""#))
            .unwrap_or_default();
        fs::write(
            dir.join(format!("{stem}.onnx.json")),
            format!(r#"{{"language":{{"code":"pl_PL"}}{license}}}"#),
        )
        .unwrap();
        dir
    }

    #[derive(Default)]
    struct MemoryTransport {
        calls: Mutex<Vec<String>>,
        bytes: Mutex<Vec<(String, Vec<u8>)>>,
        block: Mutex<Option<Arc<AtomicBool>>>,
    }

    impl MemoryTransport {
        fn serving(pairs: &[(&str, &[u8])]) -> Self {
            Self {
                calls: Mutex::new(Vec::new()),
                bytes: Mutex::new(
                    pairs
                        .iter()
                        .map(|(url, data)| (url.to_string(), data.to_vec()))
                        .collect(),
                ),
                block: Mutex::new(None),
            }
        }

        fn blocking() -> Self {
            Self {
                block: Mutex::new(Some(Arc::new(AtomicBool::new(true)))),
                ..Self::default()
            }
        }

        fn calls(&self) -> Vec<String> {
            self.calls.lock().unwrap().clone()
        }
    }

    impl RemoteTransport for MemoryTransport {
        fn fetch(
            &self,
            url: &str,
            sink: &mut dyn FnMut(&[u8]) -> Result<(), TransportError>,
        ) -> Result<(), TransportError> {
            self.calls.lock().unwrap().push(url.to_string());
            if let Some(flag) = self.block.lock().unwrap().as_ref() {
                while flag.load(Ordering::SeqCst) {
                    // Anulowanie objawia się przez sink (token importu).
                    sink(b"x").map_err(|_| TransportError::Cancelled)?;
                    std::thread::sleep(Duration::from_millis(2));
                }
                return Ok(());
            }
            let data = self
                .bytes
                .lock()
                .unwrap()
                .iter()
                .find(|(key, _)| key == url)
                .map(|(_, data)| data.clone())
                .ok_or_else(|| TransportError::HttpStatus(404))?;
            sink(&data)
        }
    }

    fn piper_urls(stem: &str) -> (String, String) {
        let preset = builtin_presets()
            .into_iter()
            .find(|p| p.id == format!("piper/{stem}"))
            .unwrap();
        let base = preset.weights_file().unwrap().url.clone();
        (base.clone(), format!("{base}.json"))
    }

    #[test]
    fn presets_are_listed_without_touching_disk_or_network() {
        let root = TempRoot::new("presets");
        let catalog = root.catalog();
        let sources = list_sources();
        assert_eq!(sources.len(), 4);

        let presets = builtin_presets();
        assert!(presets.len() >= 8);
        let darkman = presets
            .iter()
            .find(|p| p.id == "piper/pl_PL-darkman-medium")
            .unwrap();
        assert_eq!(darkman.kind, ModelKind::PiperTts);
        assert_eq!(darkman.format, ModelFormat::Onnx);
        assert!(!darkman.author.is_empty());
        assert!(!darkman.license.is_empty());
        assert!(darkman.size_bytes > 0);
        assert_eq!(darkman.origin, "Portable Local Studio");
        assert!(!darkman.files.is_empty());

        // Listowanie presetów nie tworzy katalogów ani nie pobiera niczego.
        assert!(!catalog.root().exists());
        assert!(catalog.scan().is_empty());
        let importer = ModelImporter::new(catalog);
        assert!(!importer.has_network());
    }

    #[test]
    fn local_import_installs_a_valid_piper_model_and_persists() {
        let root = TempRoot::new("local-piper");
        let catalog = root.catalog();
        let importer = ModelImporter::new(catalog.clone());
        let source = write_piper_source(&root, "pl_PL-darkman-medium", 4096, Some("CC0-1.0"));

        let model = importer
            .import_local(&source, Some(ModelKind::PiperTts), false)
            .expect("import powinien się udać");
        assert_eq!(model.id, "piper/pl_PL-darkman-medium");
        assert_eq!(model.status, ModelAvailability::Available);
        assert_eq!(model.license.as_deref(), Some("CC0-1.0"));
        assert_eq!(model.size_bytes, 4096);
        assert!(model.path.is_file());

        // Nowa instancja katalogu (restart aplikacji) widzi model.
        let reopened = ModelCatalog::new(catalog.root().to_path_buf());
        assert_eq!(reopened.scan().len(), 1);
        assert!(reopened.inspect("piper/pl_PL-darkman-medium").is_some());
        assert!(!catalog.staging_root().exists() || fs::read_dir(catalog.staging_root()).unwrap().next().is_none());
    }

    #[test]
    fn local_import_rejects_bad_format_and_scripts() {
        let root = TempRoot::new("local-bad");
        let importer = ModelImporter::new(root.catalog());

        let text = root.path().join("not-a-model.txt");
        fs::write(&text, b"hello").unwrap();
        assert!(matches!(
            importer.import_local(&text, None, false),
            Err(ImportError::ForbiddenFormat(_))
        ));

        // Katalog z modelem i skryptem: skrypt nie może wejść do katalogu.
        let source = write_piper_source(&root, "pl_PL-gosia-medium", 4096, Some("CC0-1.0"));
        fs::write(source.join("install.sh"), b"#!/bin/sh\necho pwned").unwrap();
        assert!(matches!(
            importer.import_local(&source, None, false),
            Err(ImportError::ForbiddenFormat(_))
        ));
        assert!(root.catalog().scan().is_empty());
    }

    #[test]
    fn local_import_rejects_checksum_mismatch() {
        let root = TempRoot::new("local-sha");
        let importer = ModelImporter::new(root.catalog());
        let source = write_piper_source(&root, "pl_PL-darkman-medium", 4096, Some("CC0-1.0"));
        let expected = super::sha256_file(&source.join("pl_PL-darkman-medium.onnx")).unwrap();
        let wrong = "0".repeat(64);
        assert_ne!(expected, wrong);
        fs::write(
            source.join("pl_PL-darkman-medium.onnx.sha256"),
            format!("{wrong}  pl_PL-darkman-medium.onnx\n"),
        )
        .unwrap();

        assert!(matches!(
            importer.import_local(&source, None, false),
            Err(ImportError::ChecksumMismatch { .. })
        ));
        assert!(root.catalog().scan().is_empty());
        assert!(!root.catalog().kind_dir(ModelKind::PiperTts).join("pl_PL-darkman-medium.onnx").exists());
    }

    #[test]
    fn local_import_rejects_path_traversal_inside_zip() {
        use zip::write::SimpleFileOptions;

        let root = TempRoot::new("local-zip");
        let importer = ModelImporter::new(root.catalog());
        let archive_path = root.path().join("model.zip");

        let mut buffer = std::io::Cursor::new(Vec::new());
        {
            let mut zip = zip::ZipWriter::new(&mut buffer);
            zip.start_file("../escape.gguf", SimpleFileOptions::default())
                .unwrap();
            zip.write_all(b"GGUF").unwrap();
            zip.finish().unwrap();
        }
        fs::write(&archive_path, buffer.into_inner()).unwrap();

        assert!(matches!(
            importer.import_local(&archive_path, Some(ModelKind::LocalLlm), false),
            Err(ImportError::UnsafePath(_))
        ));
        assert!(!root.path().join("escape.gguf").exists());
        assert!(!root.path().join("audioticon/escape.gguf").exists());
        assert!(root.catalog().scan().is_empty());

        // ZIP z zagnieżdżoną ścieżką też jest odrzucany (nie zgadujemy).
        let mut buffer = std::io::Cursor::new(Vec::new());
        {
            let mut zip = zip::ZipWriter::new(&mut buffer);
            zip.start_file("nested/model.gguf", SimpleFileOptions::default())
                .unwrap();
            zip.write_all(b"GGUFxxxx").unwrap();
            zip.finish().unwrap();
        }
        fs::write(&archive_path, buffer.into_inner()).unwrap();
        assert!(matches!(
            importer.import_local(&archive_path, Some(ModelKind::LocalLlm), false),
            Err(ImportError::UnsafePath(_))
        ));
    }

    #[test]
    fn remote_download_requires_explicit_consent_and_never_calls_transport() {
        let root = TempRoot::new("consent");
        let transport = Arc::new(MemoryTransport::serving(&[]));
        let importer = ModelImporter::with_transport(root.catalog(), transport.clone());

        let request = RemoteDownloadRequest::new("preset:piper/pl_PL-darkman-medium");
        assert!(matches!(importer.download(&request), Err(ImportError::ConsentRequired)));
        assert!(transport.calls().is_empty(), "brak zgody = zero transferu");
        assert!(root.catalog().scan().is_empty());
    }

    #[test]
    fn remote_download_without_transport_reports_missing_network() {
        let root = TempRoot::new("no-network");
        let catalog = root.catalog();
        let importer = ModelImporter::new(catalog.clone());
        let request = RemoteDownloadRequest::new("preset:piper/pl_PL-gosia-medium").with_consent();

        let error = importer.download(&request).unwrap_err();
        assert!(matches!(error, ImportError::NetworkUnavailable(_)));
        assert!(error.to_string().contains("sieci"));
        assert!(catalog.scan().is_empty());
        assert!(!catalog.staging_root().exists());
    }

    #[test]
    fn remote_download_installs_preset_and_records_checksum() {
        let root = TempRoot::new("download-ok");
        let catalog = root.catalog();
        let (weights_url, config_url) = piper_urls("pl_PL-darkman-medium");
        let weights = vec![0x08u8; 4096];
        let config = br#"{"language":{"code":"pl_PL"},"license":"CC0-1.0"}"#;
        let transport = Arc::new(MemoryTransport::serving(&[
            (weights_url.as_str(), weights.as_slice()),
            (config_url.as_str(), config.as_slice()),
        ]));
        let importer = ModelImporter::with_transport(catalog.clone(), transport.clone());

        let outcome = importer
            .download(&RemoteDownloadRequest::new("preset:piper/pl_PL-darkman-medium").with_consent())
            .expect("pobranie z transportem testowym");
        assert_eq!(outcome.status, ImportStatus::Completed);
        let model = outcome.model.expect("model po pobraniu");
        assert_eq!(model.id, "piper/pl_PL-darkman-medium");
        assert!(model.is_available());
        assert!(model.checksum_sha256.is_some(), "checksum zapisany po pobraniu");
        assert_eq!(model.license.as_deref(), Some("CC0-1.0"));
        assert_eq!(transport.calls().len(), 2);

        let verified = importer.verify("piper/pl_PL-darkman-medium").unwrap();
        assert!(verified.checked);
        assert_eq!(verified.matches, Some(true));
        assert!(!catalog.staging_root().exists()
            || fs::read_dir(catalog.staging_root()).unwrap().next().is_none());
    }

    #[test]
    fn remote_download_checksum_mismatch_leaves_nothing_behind() {
        let root = TempRoot::new("download-sha");
        let catalog = root.catalog();
        let (weights_url, config_url) = piper_urls("pl_PL-gosia-medium");
        let transport = Arc::new(MemoryTransport::serving(&[
            (weights_url.as_str(), vec![0x08u8; 4096].as_slice()),
            (config_url.as_str(), br#"{"license":"CC0-1.0"}"#.as_slice()),
        ]));
        let importer = ModelImporter::with_transport(catalog.clone(), transport);

        let request = RemoteDownloadRequest::new("preset:piper/pl_PL-gosia-medium")
            .with_consent()
            .with_expected_sha256("0".repeat(64));
        assert!(matches!(
            importer.download(&request),
            Err(ImportError::ChecksumMismatch { .. })
        ));
        assert!(catalog.scan().is_empty());
        assert!(!catalog.kind_dir(ModelKind::PiperTts).join("pl_PL-gosia-medium.onnx").exists());
        assert!(!catalog.staging_root().exists()
            || fs::read_dir(catalog.staging_root()).unwrap().next().is_none());
    }

    #[test]
    fn cancelled_download_removes_staging_and_keeps_catalog_empty() {
        let root = TempRoot::new("cancel");
        let catalog = root.catalog();
        let transport = Arc::new(MemoryTransport::blocking());
        let importer = Arc::new(ModelImporter::with_transport(catalog.clone(), transport));

        let (tx, rx) = mpsc::channel();
        let worker = {
            let importer = Arc::clone(&importer);
            std::thread::spawn(move || {
                let request =
                    RemoteDownloadRequest::new("preset:piper/pl_PL-gosia-medium").with_consent();
                let _ = tx.send(());
                importer.download(&request)
            })
        };

        // Czekamy, aż import się zarejestruje (staging powstanie).
        let mut import_id = 0;
        for _ in 0..200 {
            import_id = importer.active_import_id();
            if import_id > 0 {
                break;
            }
            std::thread::sleep(Duration::from_millis(5));
        }
        rx.recv_timeout(Duration::from_secs(2)).ok();
        assert!(import_id > 0, "import powinien być zarejestrowany");
        assert!(importer.cancel(import_id));
        assert!(!importer.cancel(import_id + 999), "nieznany import nie da się anulować");

        let outcome = worker.join().expect("wątek importu").expect("anulowanie to nie błąd");
        assert_eq!(outcome.status, ImportStatus::Cancelled);
        assert!(outcome.model.is_none());
        assert!(catalog.scan().is_empty());
        assert!(!catalog.kind_dir(ModelKind::PiperTts).join("pl_PL-gosia-medium.onnx").exists());
        assert!(!catalog.staging_root().exists()
            || fs::read_dir(catalog.staging_root()).unwrap().next().is_none());
    }

    #[test]
    fn uncurated_remote_sources_need_checksum_and_size() {
        let root = TempRoot::new("uncurated");
        let transport = Arc::new(MemoryTransport::serving(&[]));
        let importer = ModelImporter::with_transport(root.catalog(), transport.clone());

        let request = RemoteDownloadRequest::new("hf:Qwen/Qwen2.5-3B-Instruct-GGUF@main#qwen2.5-3b-instruct-q4_k_m.gguf")
            .with_consent();
        assert!(matches!(
            importer.download(&request),
            Err(ImportError::UnverifiedSource(_))
        ));
        assert!(transport.calls().is_empty());

        for bad in [
            "gh:owner/repo@tag#../../evil.gguf",
            "hf:owner/repo@main#/etc/passwd",
            "hf:not-a-repo#x.gguf",
            "https://evil.example.com/model.gguf",
            "https://huggingface.co.evil.com/model.gguf",
        ] {
            assert!(
                describe_remote_reference(bad).is_err(),
                "odwołanie '{bad}' musi być odrzucone"
            );
        }
    }

    #[test]
    fn unknown_license_requires_explicit_acceptance() {
        let root = TempRoot::new("license");
        let importer = ModelImporter::new(root.catalog());
        let dir = root.path().join("gguf-src");
        fs::create_dir_all(&dir).unwrap();
        let mut bytes = b"GGUF".to_vec();
        bytes.extend(vec![0u8; 2048]);
        fs::write(dir.join("local-model.gguf"), &bytes).unwrap();

        assert!(matches!(
            importer.import_local(&dir.join("local-model.gguf"), Some(ModelKind::LocalLlm), false),
            Err(ImportError::LicenseUnknown)
        ));
        let model = importer
            .import_local(&dir.join("local-model.gguf"), Some(ModelKind::LocalLlm), true)
            .expect("jawna akceptacja nieznanej licencji pozwala importować");
        assert_eq!(model.id, "llm/local-model");
        assert_eq!(model.format, ModelFormat::Gguf);
    }

    #[test]
    fn incomplete_staging_is_visible_and_not_selectable() {
        let root = TempRoot::new("incomplete");
        let catalog = root.catalog();
        let staging = catalog.staging_root().join("000001-llm-test");
        fs::create_dir_all(&staging).unwrap();
        fs::write(staging.join("model.gguf.part"), vec![0u8; 100]).unwrap();
        write_intent(
            &staging,
            StagingIntent {
                import_id: 1,
                id: "llm/test".to_string(),
                kind: "local_llm".to_string(),
                format: "gguf".to_string(),
                files: vec!["model.gguf".to_string()],
                source: "preset:llm/test".to_string(),
                started_at_utc: "0-UTC".to_string(),
            },
        )
        .unwrap();

        let models = catalog.scan();
        assert_eq!(models.len(), 1);
        assert_eq!(models[0].status, ModelAvailability::Incomplete);
        assert_eq!(models[0].id, "llm/test");
        assert!(!models[0].is_available());
        let status = catalog.status();
        assert_eq!(status.total, 1);
        assert_eq!(status.available, 0);
        assert_eq!(status.incomplete, 1);
        assert!(status.message().contains("nieukończon"));
        assert!(catalog.select("llm/test").is_err());
    }

    #[test]
    fn verify_and_remove_leave_the_catalog_consistent() {
        let root = TempRoot::new("verify-remove");
        let catalog = root.catalog();
        let importer = ModelImporter::new(catalog.clone());
        let source = write_piper_source(&root, "pl_PL-darkman-medium", 4096, Some("CC0-1.0"));
        importer
            .import_local(&source, None, false)
            .expect("import");
        catalog.select("piper/pl_PL-darkman-medium").unwrap();
        assert!(catalog.selected().is_some());

        let verified = importer.verify("piper/pl_PL-darkman-medium").unwrap();
        assert_eq!(verified.status, ModelAvailability::Available);
        assert!(!verified.checked, "lokalny import bez sidecara: nie ma z czym porównać");

        let removed = importer.remove("piper/pl_PL-darkman-medium").unwrap();
        assert!(removed.selection_cleared);
        assert!(!removed.removed_files.is_empty());
        assert!(catalog.scan().is_empty());
        assert!(catalog.selected().is_none(), "wybór usuniętego modelu jest czyszczony");
        assert!(matches!(
            importer.remove("piper/pl_PL-darkman-medium"),
            Err(ImportError::NotFound(_))
        ));

        // Próby wyjścia z katalogu są odrzucane.
        for bad in ["piper/../llm/x", "../piper/x", "piper/x/y", "/etc/passwd"] {
            assert!(
                matches!(importer.remove(bad), Err(ImportError::UnsafePath(_)) | Err(ImportError::NotFound(_))),
                "id '{bad}' musi być odrzucone"
            );
        }
    }

    #[test]
    fn import_module_has_no_implicit_network_client() {
        let source = include_str!("model_import.rs");
        let production = source.split("#[cfg(test)]").next().unwrap_or(source);
        for forbidden in ["reqwest", "ureq", "TcpStream", "Command::new", "std::net", "curl"] {
            assert!(
                !production.contains(forbidden),
                "moduł importu nie może zawierać '{forbidden}'"
            );
        }
    }
}
