use std::collections::HashMap;
use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::{Arc, Mutex, MutexGuard, OnceLock};
use std::time::Duration;

// Bring the synthesizer trait into scope so `PiperBridge::synthesize` resolves.
use audioticon_core::SpeechSynthesizer as _;

#[cxx::bridge(namespace = "audioticon::bridge")]
pub mod ffi {
    #[derive(Clone, Debug, PartialEq)]
    struct DirectiveItemDto {
        token: String,
        kind: String,
        pause_ms: u32,
        speed: f64,
        dsp: String,
        label: String,
    }

    #[derive(Clone, Debug, PartialEq)]
    struct VoiceSegmentDto {
        index: u32,
        text: String,
        speed: f64,
        dsp: String,
        pause_ms: u32,
        leading_pause_ms: u32,
        cache_hash: String,
    }

    #[derive(Clone, Debug, PartialEq)]
    struct ChapterDto {
        segment_index: u32,
        title: String,
    }

    #[derive(Clone, Debug, PartialEq)]
    struct DirectedTextResultDto {
        segments: Vec<VoiceSegmentDto>,
        chapters: Vec<ChapterDto>,
        directives: Vec<DirectiveItemDto>,
        leading_pause_ms: u32,
        plain_text: String,
    }

    #[derive(Clone, Debug, PartialEq)]
    struct AudioBufferDto {
        sample_rate: u32,
        channels: u16,
        bits_per_sample: u16,
        duration_ms: u64,
        samples: Vec<i16>,
    }

    #[derive(Copy, Clone, PartialEq, Eq, Debug)]
    enum TaskStage {
        Idle,
        Queued,
        Normalizing,
        Synthesizing,
        Mastering,
        Finished,
        Failed,
        Cancelled,
    }

    #[derive(Clone, Debug, PartialEq)]
    struct RenderStatusDto {
        job_id: u64,
        stage: TaskStage,
        current_segment: u32,
        total_segments: u32,
        cache_hits: u32,
        progress_percent: f32,
        status_message: String,
        error_message: String,
    }

    /// One locally installed model, as listed by the M17 catalog.
    ///
    /// `status` is one of `available`, `missing_metadata`, `corrupt`; `detail`
    /// carries the human-readable reason (empty when the model is available).
    #[derive(Clone, Debug, PartialEq)]
    struct ModelInfoDto {
        id: String,
        kind: String,
        name: String,
        path: String,
        format: String,
        language: String,
        size_bytes: u64,
        license: String,
        status: String,
        checksum_sha256: String,
        detail: String,
    }

    /// Snapshot of the whole local catalog plus the persisted selection.
    #[derive(Clone, Debug, PartialEq)]
    struct ModelStatusDto {
        catalog_root: String,
        root_exists: bool,
        total: u32,
        available: u32,
        unavailable: u32,
        /// Models whose import/download is still in progress (or was interrupted).
        incomplete: u32,
        /// Persisted selection, if any (may point at a model that vanished).
        active_id: String,
        active_kind: String,
        active_path: String,
        /// `available`, `missing_metadata`, `corrupt` or `unavailable`.
        active_status: String,
        /// Piper model actually wired into rendering, if any.
        piper_model_path: String,
        message: String,
    }

    /// Jeden plik presetu lub kandydata zdalnego.
    #[derive(Clone, Debug, PartialEq)]
    struct PresetFileDto {
        role: String,
        file_name: String,
        url: String,
        sha256: String,
        size_bytes: u64,
    }

    /// Jeden z czterech sposobów dodawania modeli.
    #[derive(Clone, Debug, PartialEq)]
    struct ModelSourceDto {
        kind: String,
        name: String,
        detail: String,
        network: bool,
        requires_explicit_consent: bool,
    }

    /// Preset kurowany (Modele znane z Portable Local Studio), bez pobierania.
    #[derive(Clone, Debug, PartialEq)]
    struct CuratedPresetDto {
        id: String,
        kind: String,
        name: String,
        author: String,
        license: String,
        language: String,
        format: String,
        size_bytes: u64,
        source: String,
        source_reference: String,
        origin: String,
        description: String,
        files: Vec<PresetFileDto>,
        /// Czy model jest już w katalogu.
        installed: bool,
        /// Status katalogu albo `not_installed`.
        installed_status: String,
    }

    /// To, co UI pokazuje **przed** pobraniem: źródło, autor, licencja,
    /// rozmiar, format i ewentualne braki metadanych.
    #[derive(Clone, Debug, PartialEq)]
    struct RemoteCandidateDto {
        id: String,
        kind: String,
        name: String,
        author: String,
        license: String,
        language: String,
        format: String,
        size_bytes: u64,
        source: String,
        source_reference: String,
        origin_url: String,
        curated: bool,
        downloadable: bool,
        blockers: Vec<String>,
        files: Vec<PresetFileDto>,
    }

    /// Wynik importu lub pobrania.
    #[derive(Clone, Debug, PartialEq)]
    struct ImportResultDto {
        import_id: u64,
        /// `completed`, `cancelled` albo `failed`.
        status: String,
        model_id: String,
        model_path: String,
        model_status: String,
        message: String,
        source: String,
    }

    /// Wynik jawnej weryfikacji checksumu.
    #[derive(Clone, Debug, PartialEq)]
    struct VerifyResultDto {
        id: String,
        kind: String,
        status: String,
        checked: bool,
        expected_sha256: String,
        actual_sha256: String,
        /// Ma sens tylko gdy `checked`.
        matches: bool,
        size_bytes: u64,
        message: String,
    }

    /// Wynik usunięcia modelu.
    #[derive(Clone, Debug, PartialEq)]
    struct RemoveResultDto {
        id: String,
        kind: String,
        removed_files: u32,
        selection_cleared: bool,
    }

    /// Snapshot of one render job, pulled by the Qt side with `job_state`.
    ///
    /// A plain value type on purpose: the GUI thread only ever copies a
    /// snapshot out of the registry, it never waits for the worker.
    #[derive(Clone, Debug, PartialEq)]
    struct JobStateDto {
        job_id: u64,
        stage: TaskStage,
        progress_percent: f32,
        current_segment: u32,
        total_segments: u32,
        status_message: String,
        error_message: String,
        output_path: String,
    }

    unsafe extern "C++" {
        include!("BridgeObserver.h");
        type BridgeObserver;

        fn on_progress(self: &BridgeObserver, status: RenderStatusDto);
    }

    extern "Rust" {
        type BridgeEngine;

        // M4 link proof. The value is NOT a literal in this crate: it is
        // re-exported by audioticon-core, so a successful call means the real
        // core staticlib was linked into the Qt binary and executed.
        // 44100 Hz == audioticon_core::DEFAULT_SAMPLE_RATE.
        fn core_version() -> u32;

        fn create_engine() -> Result<Box<BridgeEngine>>;

        fn parse_directed_text(
            engine: &BridgeEngine,
            raw_text: &str,
            polish: bool,
        ) -> Result<DirectedTextResultDto>;

        fn create_test_tone_pcm(
            engine: &BridgeEngine,
            frequency_hz: f32,
            duration_ms: u32,
        ) -> Result<AudioBufferDto>;

        fn get_mock_render_status(
            engine: &BridgeEngine,
            job_id: u64,
            current_step: u32,
            total_steps: u32,
        ) -> RenderStatusDto;

        fn notify_progress(
            engine: &BridgeEngine,
            status: RenderStatusDto,
            observer: &BridgeObserver,
        );

        // ---- M5: asynchronous job runtime ----------------------------------
        // start_job() only registers the job and returns its id. The work runs
        // on the crate's tokio runtime, so the caller (the Qt GUI thread) never
        // blocks and never needs a block_on().
        fn start_job(engine: &BridgeEngine, text: &str, output_dir: &str) -> Result<u64>;

        fn job_state(engine: &BridgeEngine, job_id: u64) -> JobStateDto;

        fn cancel_job(engine: &BridgeEngine, job_id: u64) -> bool;

        fn active_jobs(engine: &BridgeEngine) -> u32;

        // ---- M17: one local model catalog contract -------------------------
        // Offline by construction: these calls only read the XDG model tree and
        // the persisted selection file. Nothing here downloads anything.
        fn list_models(engine: &BridgeEngine) -> Vec<ModelInfoDto>;

        fn inspect_model(engine: &BridgeEngine, id: &str) -> Result<ModelInfoDto>;

        fn select_model(engine: &BridgeEngine, id: &str) -> Result<ModelInfoDto>;

        fn model_status(engine: &BridgeEngine) -> ModelStatusDto;

        fn clear_model_selection(engine: &BridgeEngine) -> Result<()>;

        // ---- M18: dodawanie modeli (presety / lokalny / zdalny) -------------
        // Pobranie jest jedyną funkcją, która może otworzyć sieć — i tylko z
        // jawną zgodą oraz wstrzykniętym transportem.
        fn list_sources(engine: &BridgeEngine) -> Vec<ModelSourceDto>;

        fn list_curated_presets(engine: &BridgeEngine) -> Vec<CuratedPresetDto>;

        fn inspect_remote_candidate(
            engine: &BridgeEngine,
            reference: &str,
        ) -> Result<RemoteCandidateDto>;

        fn import_local(
            engine: &BridgeEngine,
            path: &str,
            kind: &str,
            accept_unknown_license: bool,
        ) -> Result<ImportResultDto>;

        fn download_remote(
            engine: &BridgeEngine,
            reference: &str,
            expected_sha256: &str,
            expected_size: u64,
            consent: bool,
        ) -> Result<ImportResultDto>;

        fn cancel_import(engine: &BridgeEngine, import_id: u64) -> bool;

        fn active_import_id(engine: &BridgeEngine) -> u64;

        fn verify_model(engine: &BridgeEngine, id: &str) -> Result<VerifyResultDto>;

        fn remove_model(engine: &BridgeEngine, id: &str) -> Result<RemoveResultDto>;
    }
}

pub use ffi::*;
use thiserror::Error;

#[derive(Error, Debug)]
pub enum BridgeError {
    #[error("Bridge initialization failed: {0}")]
    InitFailed(String),
    #[error("Parsing error: {0}")]
    ParseError(String),
    #[error("Audio generation error: {0}")]
    AudioError(String),
    #[error("Nieprawidłowe żądanie: {0}")]
    InvalidRequest(String),
    #[error("Model niedostępny: {0}")]
    ModelUnavailable(String),
    #[error("Import modelu nie powiódł się: {0}")]
    ModelImport(String),
}

// ============================================================================
// M5: render job runtime
// ============================================================================

/// Gap between two progress steps of the placeholder render.
const JOB_STEP_DELAY: Duration = Duration::from_millis(80);
/// Length of the placeholder audio every job produces in M5.
const MOCK_RENDER_MS: u32 = 200;

/// The one tokio runtime of the process, created on first use.
///
/// It is deliberately never dropped: dropping a `Runtime` shuts its workers down
/// and blocks whichever thread happens to release the last engine — possibly the
/// Qt GUI thread. Jobs are started with `spawn`, never `block_on`.
fn job_runtime() -> &'static tokio::runtime::Runtime {
    static RUNTIME: OnceLock<tokio::runtime::Runtime> = OnceLock::new();
    RUNTIME.get_or_init(|| {
        tokio::runtime::Builder::new_multi_thread()
            .worker_threads(1)
            .thread_name("audioticon-jobs")
            .enable_all()
            .build()
            .expect("nie można utworzyć runtime tokio dla zadań renderowania")
    })
}

/// Number of progress steps the placeholder render reports.
///
/// Scaled by the text so the progress bar follows the work, but clamped so a
/// one-word job is still observable and a whole book does not spend a minute
/// sleeping.
fn planned_steps(text: &str) -> u32 {
    (text.split_whitespace().count() / 8).clamp(3, 6) as u32
}

/// Where jobs write their output when the caller does not choose a directory.
fn default_output_dir() -> PathBuf {
    std::env::temp_dir().join("audioticon-jobs")
}

/// Resolves a local Piper ONNX model, if any is available.
///
/// Order of precedence: an explicit env var, then a few well-known locations
/// used by the developer's machines, then the core `models_dir()`. Returns
/// `None` (rather than an error) when nothing is found — the job runtime then
/// keeps the M5 placeholder so the GUI and the non-model tests keep working.
/// Resolves a local Piper ONNX model exactly the way the M17 catalog does.
///
/// Order of precedence:
/// 1. `AUDIOTICON_PIPER_MODEL` — an explicit developer override (user catalog only).
/// 2. The persisted selection, if it points at an available Piper model.
/// 3. The first available model in `<root>/piper/`.
/// 4. Legacy flat locations next to `models/` — only for the user's real data
///    dir, so a test catalog can never pick up somebody else's model.
///
/// Returns `None` (rather than an error) when nothing is found; the job runtime
/// then reports "Brak modelu głosu" instead of pretending to render (B13).
fn resolve_piper_model_in(catalog: &audioticon_core::ModelCatalog) -> Option<PathBuf> {
    // The env override belongs to the user-facing engine. An explicit catalog
    // root (tests, portable bundle) must describe its models alone, otherwise a
    // developer's global model would leak into an isolated catalog.
    if catalog.uses_legacy_fallback() {
        if let Ok(p) = std::env::var("AUDIOTICON_PIPER_MODEL") {
            let path = PathBuf::from(p.trim());
            if path.is_file() {
                return Some(path);
            }
        }
    }

    if let Some(selection) = catalog.active() {
        if selection.kind == audioticon_core::ModelKind::PiperTts && selection.path.is_file() {
            return Some(selection.path);
        }
    }

    if let Some(model) = catalog
        .list(Some(audioticon_core::ModelKind::PiperTts))
        .into_iter()
        .find(|m| m.is_available())
    {
        return Some(model.path);
    }

    if !catalog.uses_legacy_fallback() {
        return None;
    }

    let models_dir = audioticon_core::models::models_dir();
    let names = [
        "pl_PL-bass-high.onnx",
        "pl_PL-gosia-medium.onnx",
        "pl_PL-darkman-medium.onnx",
    ];
    for n in names {
        let p = models_dir.join(n);
        if p.is_file() {
            return Some(p);
        }
    }

    None
}

fn model_info_dto(model: &audioticon_core::CatalogModel) -> ModelInfoDto {
    ModelInfoDto {
        id: model.id.clone(),
        kind: model.kind.as_str().to_string(),
        name: model.name.clone(),
        path: model.path.display().to_string(),
        format: model.format.as_str().to_string(),
        language: model.language.clone().unwrap_or_default(),
        size_bytes: model.size_bytes,
        license: model.license.clone().unwrap_or_default(),
        status: model.status.as_str().to_string(),
        checksum_sha256: model.checksum_sha256.clone().unwrap_or_default(),
        detail: model.detail.clone(),
    }
}

/// One entry of the job registry.
#[derive(Debug)]
struct JobRecord {
    stage: TaskStage,
    progress_percent: f32,
    current_segment: u32,
    total_segments: u32,
    status_message: String,
    error_message: String,
    output_path: String,
    cancel: Arc<AtomicBool>,
}

impl JobRecord {
    fn new(total_segments: u32) -> Self {
        Self {
            stage: TaskStage::Queued,
            progress_percent: 0.0,
            current_segment: 0,
            total_segments,
            status_message: format!("W kolejce (0/{} segmentów)", total_segments),
            error_message: String::new(),
            output_path: String::new(),
            cancel: Arc::new(AtomicBool::new(false)),
        }
    }

    fn is_terminal(&self) -> bool {
        matches!(
            self.stage,
            TaskStage::Finished | TaskStage::Failed | TaskStage::Cancelled
        )
    }

    fn snapshot(&self, job_id: u64) -> JobStateDto {
        JobStateDto {
            job_id,
            stage: self.stage,
            progress_percent: self.progress_percent,
            current_segment: self.current_segment,
            total_segments: self.total_segments,
            status_message: self.status_message.clone(),
            error_message: self.error_message.clone(),
            output_path: self.output_path.clone(),
        }
    }
}

/// Job registry shared between the FFI side and the runtime tasks.
///
/// A `HashMap` behind a `Mutex` is enough: the GUI thread reads single
/// snapshots, so there is no contention worth optimising for.
#[derive(Default)]
struct JobRegistry {
    next_id: AtomicU64,
    jobs: Mutex<HashMap<u64, JobRecord>>,
}

impl JobRegistry {
    fn lock(&self) -> MutexGuard<'_, HashMap<u64, JobRecord>> {
        // A panicking job must not poison the registry for every later job.
        self.jobs
            .lock()
            .unwrap_or_else(|poisoned| poisoned.into_inner())
    }

    fn register(&self, total_segments: u32) -> (u64, Arc<AtomicBool>) {
        let job_id = self.next_id.fetch_add(1, Ordering::SeqCst) + 1;
        let record = JobRecord::new(total_segments);
        let cancel = Arc::clone(&record.cancel);
        self.lock().insert(job_id, record);
        (job_id, cancel)
    }

    fn snapshot(&self, job_id: u64) -> JobStateDto {
        match self.lock().get(&job_id) {
            Some(record) => record.snapshot(job_id),
            // Unknown ids are not an error: the UI may poll one tick after a
            // reset. Idle says "nothing here", which is exactly true.
            None => JobStateDto {
                job_id,
                stage: TaskStage::Idle,
                progress_percent: 0.0,
                current_segment: 0,
                total_segments: 0,
                status_message: String::new(),
                error_message: String::new(),
                output_path: String::new(),
            },
        }
    }

    /// Updates a running job. Terminal jobs are frozen, so a late progress step
    /// from the worker can never resurrect a cancelled job.
    fn update_running(&self, job_id: u64, update: impl FnOnce(&mut JobRecord)) {
        let mut jobs = self.lock();
        if let Some(record) = jobs.get_mut(&job_id) {
            if record.is_terminal() {
                return;
            }
            update(record);
        }
    }

    fn finish(
        &self,
        job_id: u64,
        stage: TaskStage,
        progress: f32,
        message: String,
        error: String,
        output: String,
    ) {
        let mut jobs = self.lock();
        if let Some(record) = jobs.get_mut(&job_id) {
            if record.is_terminal() {
                return;
            }
            record.stage = stage;
            record.progress_percent = progress;
            record.status_message = message;
            record.error_message = error;
            record.output_path = output;
        }
    }

    /// Marks the job cancelled right now; the worker only notices the flag on
    /// its next step and simply stops.
    fn cancel(&self, job_id: u64) -> bool {
        let mut jobs = self.lock();
        let Some(record) = jobs.get_mut(&job_id) else {
            return false;
        };
        if record.is_terminal() {
            return false;
        }
        record.cancel.store(true, Ordering::SeqCst);
        record.stage = TaskStage::Cancelled;
        record.status_message = "Anulowano przez użytkownika".to_string();
        true
    }

    fn active(&self) -> u32 {
        self.lock().values().filter(|r| !r.is_terminal()).count() as u32
    }
}

/// Drives one placeholder render on the shared runtime.
///
/// M5 has no model and no Piper: every job walks a few progress steps and then
/// writes a 200 ms room-tone WAV through the real core (`create_silence_wav`),
/// which is enough to exercise the whole start -> progress -> finish contract.
fn spawn_mock_render(
    registry: Arc<JobRegistry>,
    job_id: u64,
    _text: String,
    output_dir: PathBuf,
    cancel: Arc<AtomicBool>,
) {
    job_runtime().spawn(async move {
        registry.update_running(job_id, |record| {
            record.stage = TaskStage::Synthesizing;
            record.status_message = format!("Synteza 0/{}", record.total_segments);
        });

        let total = registry.snapshot(job_id).total_segments.max(1);
        for step in 1..=total {
            tokio::time::sleep(JOB_STEP_DELAY).await;
            if cancel.load(Ordering::SeqCst) {
                return;
            }
            registry.update_running(job_id, |record| {
                record.current_segment = step;
                // The last 10% belong to writing the placeholder WAV.
                record.progress_percent = (step as f32 / total as f32) * 90.0;
                record.status_message = format!("Synteza segmentu {}/{}", step, total);
            });
        }

        if cancel.load(Ordering::SeqCst) {
            return;
        }

        let output = output_dir.join(format!("job_{job_id}_silence_{MOCK_RENDER_MS}ms.wav"));
        let rendered = std::fs::create_dir_all(&output_dir)
            .map_err(|err| format!("Nie można utworzyć katalogu {}: {err}", output_dir.display()))
            .and_then(|()| {
                audioticon_core::create_silence_wav(
                    &output,
                    MOCK_RENDER_MS,
                    audioticon_core::DEFAULT_SAMPLE_RATE,
                )
                .map_err(|err| format!("Nie można zapisać pliku {}: {err:#}", output.display()))
            });

        let progress = registry.snapshot(job_id).progress_percent;
        match rendered {
            Ok(()) => registry.finish(
                job_id,
                TaskStage::Finished,
                100.0,
                format!("Gotowe: {}", output.display()),
                String::new(),
                output.display().to_string(),
            ),
            Err(error) => registry.finish(
                job_id,
                TaskStage::Failed,
                progress,
                "Renderowanie nie powiodło się".to_string(),
                error,
                String::new(),
            ),
        }
    });
}

/// Drives fallback reporting when no voice model is available (B13).
///
/// Clearly transitions the job to TaskStage::Failed with "Brak modelu głosu",
/// rather than creating a fake 200 ms silence WAV and falsely claiming "Finished".
fn spawn_fallback_no_model(
    registry: Arc<JobRegistry>,
    job_id: u64,
    cancel: Arc<AtomicBool>,
) {
    job_runtime().spawn(async move {
        tokio::time::sleep(JOB_STEP_DELAY).await;
        if cancel.load(Ordering::SeqCst) {
            return;
        }
        registry.finish(
            job_id,
            TaskStage::Failed,
            0.0,
            "Brak modelu głosu".to_string(),
            "Brak modelu głosu (ustaw AUDIOTICON_PIPER_MODEL)".to_string(),
            String::new(),
        );
    });
}

/// Splits the input text into renderable speech segments (sentences).
fn split_chapter_segments(text: &str) -> Vec<String> {
    let mut out = Vec::new();
    let mut current = String::new();
    for ch in text.chars() {
        current.push(ch);
        if matches!(ch, '.' | '!' | '?' | '\n') {
            let trimmed = current.trim();
            if !trimmed.is_empty() {
                out.push(trimmed.to_string());
            }
            current.clear();
        }
    }
    let trimmed = current.trim();
    if !trimmed.is_empty() {
        out.push(trimmed.to_string());
    }
    if out.is_empty() && !text.trim().is_empty() {
        out.push(text.trim().to_string());
    }
    out
}

/// Drives a full chapter render on the shared runtime.
///
/// Splits the text into sentences, synthesizes (or reuses cached WAVs for)
/// each segment, then stitches them into a single chapter WAV. Progress and
/// cancellation cover the whole job.
fn spawn_chapter_render(
    registry: Arc<JobRegistry>,
    job_id: u64,
    text: String,
    output_dir: PathBuf,
    cancel: Arc<AtomicBool>,
    piper: std::sync::Arc<audioticon_core::PiperBridge>,
) {
    job_runtime().spawn(async move {
        let segments_text = split_chapter_segments(&text);
        let total = segments_text.len().max(1) as u32;

        registry.update_running(job_id, |record| {
            record.total_segments = total;
            record.stage = TaskStage::Synthesizing;
            record.progress_percent = 0.0;
            record.status_message = format!("Rozdział: 0/{} segmentów", total);
        });

        if cancel.load(Ordering::SeqCst) {
            return;
        }

        if let Err(err) = std::fs::create_dir_all(&output_dir) {
            registry.finish(
                job_id,
                TaskStage::Failed,
                0.0,
                "Renderowanie nie powiodło się".to_string(),
                format!("Nie można utworzyć katalogu {}: {err}", output_dir.display()),
                String::new(),
            );
            return;
        }

        let mut segment_paths: Vec<PathBuf> = Vec::with_capacity(segments_text.len());
        let mut cache_hits: u32 = 0;

        for (i, seg_text) in segments_text.iter().enumerate() {
            if cancel.load(Ordering::SeqCst) {
                return;
            }

            registry.update_running(job_id, |record| {
                record.current_segment = (i as u32) + 1;
                record.progress_percent = (i as f32 / total as f32) * 90.0;
                record.status_message = format!("Synteza segmentu {}/{}", i + 1, total);
            });

            let segment = audioticon_core::types::TextSegment {
                text: seg_text.clone(),
                rate_mod: -6,
                pitch_mod: -4,
                volume_mod: 0,
                filter: None,
                actor_tags: Vec::new(),
                voice: None,
                pan: 0.0,
            };

            let hash = audioticon_core::cache::compute_segment_hash(&segment, "default");
            let seg_output = output_dir.join(format!("piper_{hash}.wav"));

            if seg_output.is_file() {
                cache_hits += 1;
                registry.update_running(job_id, |record| {
                    record.status_message = format!("Cache hit: {}", seg_output.display());
                });
            } else {
                let result = piper.synthesize(&segment, &seg_output).await;
                if let Err(err) = result {
                    if cancel.load(Ordering::SeqCst) {
                        let _ = std::fs::remove_file(&seg_output);
                        return;
                    }
                    registry.finish(
                        job_id,
                        TaskStage::Failed,
                        (i as f32 / total as f32) * 90.0,
                        "Renderowanie nie powiodło się".to_string(),
                        format!("{err:#}"),
                        String::new(),
                    );
                    return;
                }
            }

            if cancel.load(Ordering::SeqCst) {
                return;
            }
            segment_paths.push(seg_output);
        }

        if cancel.load(Ordering::SeqCst) {
            return;
        }

        registry.update_running(job_id, |record| {
            record.stage = TaskStage::Mastering;
            record.progress_percent = 95.0;
            record.status_message = "Łączenie segmentów rozdziału...".to_string();
        });

        let chapter_output = output_dir.join(format!("chapter_{}.wav", job_id));
        let stitch = audioticon_core::stitch_wav_files(
            &segment_paths,
            &chapter_output,
            audioticon_core::DEFAULT_SAMPLE_RATE,
        );

        match stitch {
            Ok(()) => registry.finish(
                job_id,
                TaskStage::Finished,
                100.0,
                format!(
                    "Gotowe: {} (Cache hits: {}/{})",
                    chapter_output.display(),
                    cache_hits,
                    total
                ),
                String::new(),
                chapter_output.display().to_string(),
            ),
            Err(err) => registry.finish(
                job_id,
                TaskStage::Failed,
                95.0,
                "Renderowanie nie powiodło się".to_string(),
                format!("{err:#}"),
                String::new(),
            ),
        }
    });
}

pub struct BridgeEngine {
    director: audioticon_core::parser::EmojiDirector,
    /// Optional real Piper engine. `None` when no model was found.
    ///
    /// Behind a mutex because `select_model()` swaps the active voice while the
    /// Qt side still holds `&BridgeEngine` (cxx only hands out shared refs).
    piper: Mutex<Option<Arc<audioticon_core::PiperBridge>>>,
    /// M17: the one local model catalog used by every model-related call.
    catalog: audioticon_core::ModelCatalog,
    /// M18: import lokalny, pobrania na żądanie, weryfikacja i usuwanie.
    importer: audioticon_core::ModelImporter,
    /// Shared with the runtime tasks, so a job outlives the Qt-side engine.
    jobs: Arc<JobRegistry>,
    mock_enabled: bool,
}

impl Default for BridgeEngine {
    fn default() -> Self {
        Self::new()
    }
}

impl BridgeEngine {
    pub fn new() -> Self {
        Self::new_with_mock(false)
    }

    pub fn new_mock() -> Self {
        Self::new_with_mock(true)
    }

    pub fn new_with_mock(allow_mock: bool) -> Self {
        Self::from_catalog(
            audioticon_core::ModelCatalog::for_current_user(),
            allow_mock,
            None,
        )
    }

    /// Builds an engine over an explicit catalog root (tests, portable bundle).
    ///
    /// The legacy flat-model fallback stays off: an explicit root means an
    /// explicit model tree, nothing else on the machine.
    pub fn new_with_catalog(root: PathBuf, allow_mock: bool) -> Self {
        Self::from_catalog(audioticon_core::ModelCatalog::new(root), allow_mock, None)
    }

    /// Builds an engine with an explicit transport for downloads (tests, and a
    /// future HTTPS client). `None` keeps the engine fully offline.
    pub fn new_with_catalog_and_transport(
        root: PathBuf,
        allow_mock: bool,
        transport: Arc<dyn audioticon_core::RemoteTransport>,
    ) -> Self {
        Self::from_catalog(
            audioticon_core::ModelCatalog::new(root),
            allow_mock,
            Some(transport),
        )
    }

    fn from_catalog(
        catalog: audioticon_core::ModelCatalog,
        allow_mock: bool,
        transport: Option<Arc<dyn audioticon_core::RemoteTransport>>,
    ) -> Self {
        let piper = resolve_piper_model_in(&catalog)
            .map(|m| Arc::new(audioticon_core::PiperBridge::new(m)));
        let importer = match transport {
            Some(transport) => audioticon_core::ModelImporter::with_transport(
                catalog.clone(),
                transport,
            ),
            None => audioticon_core::ModelImporter::new(catalog.clone()),
        };
        let mock_enabled = allow_mock
            || std::env::var("AUDIOTICON_MOCK_RENDER")
                .map(|v| v == "1" || v.eq_ignore_ascii_case("true"))
                .unwrap_or(false);
        Self {
            director: audioticon_core::parser::EmojiDirector::default(),
            piper: Mutex::new(piper),
            catalog,
            importer,
            jobs: Arc::new(JobRegistry::default()),
            mock_enabled,
        }
    }

    fn piper_guard(&self) -> MutexGuard<'_, Option<Arc<audioticon_core::PiperBridge>>> {
        self.piper
            .lock()
            .unwrap_or_else(|poisoned| poisoned.into_inner())
    }

    /// The local catalog this engine reads and writes.
    pub fn catalog(&self) -> &audioticon_core::ModelCatalog {
        &self.catalog
    }

    /// Exposed for tests and diagnostics: whether a real Piper model is wired in.
    pub fn has_piper_model(&self) -> bool {
        self.piper_guard().is_some()
    }

    pub fn piper_model_path(&self) -> Option<std::path::PathBuf> {
        self.piper_guard()
            .as_ref()
            .map(|p| p.default_model_path.clone())
    }

    pub fn parse_directed_text(
        &self,
        raw_text: &str,
        polish: bool,
    ) -> Result<DirectedTextResultDto, BridgeError> {
        if raw_text.trim().is_empty() {
            return Ok(DirectedTextResultDto {
                segments: Vec::new(),
                chapters: Vec::new(),
                directives: Vec::new(),
                leading_pause_ms: 0,
                plain_text: String::new(),
            });
        }

        let script = self.director.parse(raw_text);
        let mut segments = Vec::new();
        let mut chapters = Vec::new();
        let mut directives = Vec::new();
        let mut plain_parts = Vec::new();
        let mut leading_pause_ms = 0u32;
        let mut pending_pause = 0u32;
        let mut is_first_speech = true;

        for item in &script.items {
            match item {
                audioticon_core::types::DirectedItem::Speech(seg) => {
                    let text = if polish {
                        audioticon_core::verbalizer::normalize_polish_text(&seg.text)
                    } else {
                        seg.text.clone()
                    };
                    plain_parts.push(text.clone());

                    let current_idx = segments.len() as u32;
                    let speed = 1.0 + (seg.rate_mod as f64 / 100.0);
                    let dsp = seg.filter.clone().unwrap_or_default();
                    let hash = audioticon_core::cache::compute_segment_hash(seg, "default");

                    let seg_leading = if is_first_speech {
                        leading_pause_ms
                    } else {
                        pending_pause
                    };
                    pending_pause = 0;
                    is_first_speech = false;

                    segments.push(VoiceSegmentDto {
                        index: current_idx,
                        text,
                        speed,
                        dsp,
                        pause_ms: 0,
                        leading_pause_ms: seg_leading,
                        cache_hash: hash,
                    });
                }
                audioticon_core::types::DirectedItem::Silence(ms) => {
                    directives.push(DirectiveItemDto {
                        token: "🛑".to_string(),
                        kind: "pause".to_string(),
                        pause_ms: *ms,
                        speed: 1.0,
                        dsp: String::new(),
                        label: format!("Pauza {} ms", ms),
                    });
                    if is_first_speech {
                        leading_pause_ms += *ms;
                    } else if let Some(last) = segments.last_mut() {
                        last.pause_ms += *ms;
                    } else {
                        pending_pause += *ms;
                    }
                }
                audioticon_core::types::DirectedItem::Breath(ms) => {
                    directives.push(DirectiveItemDto {
                        token: "😮‍💨".to_string(),
                        kind: "breath".to_string(),
                        pause_ms: *ms,
                        speed: 1.0,
                        dsp: String::new(),
                        label: format!("Oddech {} ms", ms),
                    });
                    if is_first_speech {
                        leading_pause_ms += *ms;
                    } else if let Some(last) = segments.last_mut() {
                        last.pause_ms += *ms;
                    } else {
                        pending_pause += *ms;
                    }
                }
                audioticon_core::types::DirectedItem::Chapter(title) => {
                    let seg_idx = segments.len() as u32;
                    chapters.push(ChapterDto {
                        segment_index: seg_idx,
                        title: title.clone(),
                    });
                    directives.push(DirectiveItemDto {
                        token: "📖".to_string(),
                        kind: "chapter".to_string(),
                        pause_ms: 0,
                        speed: 1.0,
                        dsp: String::new(),
                        label: format!("Rozdział: {}", title),
                    });
                }
                audioticon_core::types::DirectedItem::SoundEffect(sfx) => {
                    directives.push(DirectiveItemDto {
                        token: "🔊".to_string(),
                        kind: "sfx".to_string(),
                        pause_ms: 0,
                        speed: 1.0,
                        dsp: sfx.clone(),
                        label: format!("Efekt: {}", sfx),
                    });
                }
                audioticon_core::types::DirectedItem::Ambience(amb) => {
                    directives.push(DirectiveItemDto {
                        token: "🌲".to_string(),
                        kind: "ambience".to_string(),
                        pause_ms: 0,
                        speed: 1.0,
                        dsp: amb.clone().unwrap_or_default(),
                        label: "Tło otoczenia".to_string(),
                    });
                }
            }
        }

        Ok(DirectedTextResultDto {
            segments,
            chapters,
            directives,
            leading_pause_ms,
            plain_text: plain_parts.join(" "),
        })
    }

    pub fn create_test_tone_pcm(
        &self,
        frequency_hz: f32,
        duration_ms: u32,
    ) -> Result<AudioBufferDto, BridgeError> {
        if frequency_hz <= 0.0 {
            return Err(BridgeError::AudioError(
                "Częstotliwość musi być większa od zera".to_string(),
            ));
        }

        let sample_rate = 22050u32;
        let channels = 1u16;
        let bits_per_sample = 16u16;
        let num_samples = ((sample_rate as f64 * duration_ms as f64) / 1000.0) as usize;
        let mut samples = Vec::with_capacity(num_samples);

        for i in 0..num_samples {
            let t = i as f32 / sample_rate as f32;
            let val = (2.0 * std::f32::consts::PI * frequency_hz * t).sin();
            let sample_i16 = (val * 32767.0).clamp(-32768.0, 32767.0) as i16;
            samples.push(sample_i16);
        }

        Ok(AudioBufferDto {
            sample_rate,
            channels,
            bits_per_sample,
            duration_ms: duration_ms as u64,
            samples,
        })
    }

    pub fn get_mock_render_status(
        &self,
        job_id: u64,
        current_step: u32,
        total_steps: u32,
    ) -> RenderStatusDto {
        let safe_total = total_steps.max(1);
        let progress_percent = ((current_step as f32 / safe_total as f32) * 100.0).clamp(0.0, 100.0);
        let stage = if current_step >= safe_total {
            TaskStage::Finished
        } else if current_step == 0 {
            TaskStage::Queued
        } else {
            TaskStage::Synthesizing
        };

        RenderStatusDto {
            job_id,
            stage,
            current_segment: current_step,
            total_segments: safe_total,
            cache_hits: current_step / 2,
            progress_percent,
            status_message: format!("Segment {}/{} ({}%)", current_step, safe_total, progress_percent as u32),
            error_message: String::new(),
        }
    }
}

impl BridgeEngine {
    /// Registers a render and returns its id. Returns immediately: the work is
    /// on the tokio runtime, never on the caller's thread.
    pub fn start_job(&self, text: &str, output_dir: &str) -> Result<u64, BridgeError> {
        if text.trim().is_empty() {
            return Err(BridgeError::InvalidRequest(
                "Tekst do syntezy jest pusty".to_string(),
            ));
        }

        let output_dir = if output_dir.trim().is_empty() {
            default_output_dir()
        } else {
            PathBuf::from(output_dir)
        };

        if self.mock_enabled {
            // Explicit mock mode for tests exercising the progress pipeline
            let (job_id, cancel) = self.jobs.register(planned_steps(text));
            spawn_mock_render(
                Arc::clone(&self.jobs),
                job_id,
                text.to_string(),
                output_dir,
                cancel,
            );
            Ok(job_id)
        } else if let Some(piper) = self.piper_guard().clone() {
            // Real Piper path: full chapter render (many segments + cache + stitch).
            let segments = split_chapter_segments(text);
            let total = segments.len().max(1) as u32;
            let (job_id, cancel) = self.jobs.register(total);
            spawn_chapter_render(
                Arc::clone(&self.jobs),
                job_id,
                text.to_string(),
                output_dir,
                cancel,
                piper,
            );
            Ok(job_id)
        } else {
            // Fallback without model: clearly report "brak modelu głosu", NOT a fake "Finished" (B13)
            let (job_id, cancel) = self.jobs.register(1);
            spawn_fallback_no_model(
                Arc::clone(&self.jobs),
                job_id,
                cancel,
            );
            Ok(job_id)
        }
    }

    pub fn job_state(&self, job_id: u64) -> JobStateDto {
        self.jobs.snapshot(job_id)
    }

    pub fn cancel_job(&self, job_id: u64) -> bool {
        self.jobs.cancel(job_id)
    }

    pub fn active_jobs(&self) -> u32 {
        self.jobs.active()
    }
}

// ============================================================================
// FFI Implementations
// ============================================================================

/// Sample rate of the linked audioticon-core, in Hz.
///
/// M4: the single function that proves `Rust -> cxx::bridge -> staticlib ->
/// qmake -> Qt binary` works end to end. It reads a constant out of the real
/// core crate instead of hardcoding 44100 here, so an accidental stub cannot
/// produce a false green.
pub fn core_version() -> u32 {
    audioticon_core::DEFAULT_SAMPLE_RATE
}

/// Standard system transport using `curl` streaming stdout into the sink.
/// Enables on-demand downloads requested explicitly by user in Model Hub.
#[derive(Debug, Default)]
pub struct SystemCurlTransport;

impl audioticon_core::RemoteTransport for SystemCurlTransport {
    fn fetch(
        &self,
        url: &str,
        sink: &mut dyn FnMut(&[u8]) -> Result<(), audioticon_core::TransportError>,
    ) -> Result<(), audioticon_core::TransportError> {
        use std::io::Read;
        let mut child = std::process::Command::new("curl")
            .arg("-sSL")
            .arg("--fail")
            .arg("--connect-timeout")
            .arg("15")
            .arg(url)
            .stdout(std::process::Stdio::piped())
            .stderr(std::process::Stdio::piped())
            .spawn()
            .map_err(|e| audioticon_core::TransportError::Network(format!("Nie można uruchomić curl: {e}")))?;

        let mut stdout = child.stdout.take().ok_or_else(|| {
            audioticon_core::TransportError::Network("Brak wyjścia stdout procesu curl".to_string())
        })?;

        let mut buffer = [0u8; 64 * 1024];
        loop {
            match stdout.read(&mut buffer) {
                Ok(0) => break,
                Ok(n) => {
                    sink(&buffer[..n])?;
                }
                Err(e) if e.kind() == std::io::ErrorKind::Interrupted => continue,
                Err(e) => return Err(audioticon_core::TransportError::Io(e.to_string())),
            }
        }

        let status = child
            .wait()
            .map_err(|e| audioticon_core::TransportError::Network(format!("Błąd oczekiwania na curl: {e}")))?;

        if !status.success() {
            let code = status.code().unwrap_or(-1);
            return Err(audioticon_core::TransportError::Network(format!(
                "curl zakończył się błędem (kod {code})"
            )));
        }

        Ok(())
    }
}

pub fn create_engine() -> Result<Box<BridgeEngine>, BridgeError> {
    let offline = std::env::var("AUDIOTICON_OFFLINE_TEST")
        .map(|v| v == "1" || v.eq_ignore_ascii_case("true"))
        .unwrap_or(false);
    if offline {
        Ok(Box::new(BridgeEngine::new()))
    } else {
        let transport = Arc::new(SystemCurlTransport);
        Ok(Box::new(BridgeEngine::from_catalog(
            audioticon_core::ModelCatalog::for_current_user(),
            false,
            Some(transport),
        )))
    }
}

pub fn parse_directed_text(
    engine: &BridgeEngine,
    raw_text: &str,
    polish: bool,
) -> Result<DirectedTextResultDto, BridgeError> {
    engine.parse_directed_text(raw_text, polish)
}

pub fn create_test_tone_pcm(
    engine: &BridgeEngine,
    frequency_hz: f32,
    duration_ms: u32,
) -> Result<AudioBufferDto, BridgeError> {
    engine.create_test_tone_pcm(frequency_hz, duration_ms)
}

pub fn get_mock_render_status(
    engine: &BridgeEngine,
    job_id: u64,
    current_step: u32,
    total_steps: u32,
) -> RenderStatusDto {
    engine.get_mock_render_status(job_id, current_step, total_steps)
}

pub fn notify_progress(
    _engine: &BridgeEngine,
    status: RenderStatusDto,
    observer: &ffi::BridgeObserver,
) {
    observer.on_progress(status);
}

pub fn start_job(engine: &BridgeEngine, text: &str, output_dir: &str) -> Result<u64, BridgeError> {
    engine.start_job(text, output_dir)
}

pub fn job_state(engine: &BridgeEngine, job_id: u64) -> JobStateDto {
    engine.job_state(job_id)
}

pub fn cancel_job(engine: &BridgeEngine, job_id: u64) -> bool {
    engine.cancel_job(job_id)
}

pub fn active_jobs(engine: &BridgeEngine) -> u32 {
    engine.active_jobs()
}

// ---- M17: local model catalog --------------------------------------------

impl BridgeEngine {
    /// Every model found on disk, with its availability status.
    pub fn list_models(&self) -> Vec<ModelInfoDto> {
        self.catalog.scan().iter().map(model_info_dto).collect()
    }

    /// One model by its stable id, or an error naming the id.
    pub fn inspect_model(&self, id: &str) -> Result<ModelInfoDto, BridgeError> {
        self.catalog
            .inspect(id)
            .map(|m| model_info_dto(&m))
            .ok_or_else(|| BridgeError::ModelUnavailable(format!("nie ma modelu '{id}' w katalogu")))
    }

    /// Persists the selection, and for a Piper model swaps the render engine.
    ///
    /// Refuses anything that is not `available`: a corrupt or incomplete model
    /// can never become the active one.
    pub fn select_model(&self, id: &str) -> Result<ModelInfoDto, BridgeError> {
        let selection = self
            .catalog
            .select(id)
            .map_err(|err| BridgeError::ModelUnavailable(err.to_string()))?;
        let model = self
            .catalog
            .inspect(&selection.id)
            .ok_or_else(|| BridgeError::ModelUnavailable(format!("nie ma modelu '{id}'")))?;

        if model.kind == audioticon_core::ModelKind::PiperTts {
            *self.piper_guard() = Some(Arc::new(audioticon_core::PiperBridge::new(
                model.path.clone(),
            )));
        }
        Ok(model_info_dto(&model))
    }

    /// Removes the persisted selection and re-resolves the Piper engine from
    /// what the catalog still offers (never from a stale path).
    pub fn clear_model_selection(&self) -> Result<(), BridgeError> {
        self.catalog
            .clear_selection()
            .map_err(|err| BridgeError::ModelUnavailable(err.to_string()))?;
        *self.piper_guard() = resolve_piper_model_in(&self.catalog)
            .map(|m| Arc::new(audioticon_core::PiperBridge::new(m)));
        Ok(())
    }

    /// Catalog root, counters and the persisted selection in one snapshot.
    pub fn model_status(&self) -> ModelStatusDto {
        let status = self.catalog.status();
        let selected = status.selected.clone();
        let inspected = selected.as_ref().and_then(|s| self.catalog.inspect(&s.id));
        let active_status = match (&selected, &inspected) {
            (Some(_), Some(model)) => model.status.as_str().to_string(),
            (Some(_), None) => "unavailable".to_string(),
            (None, _) => {
                if self.has_piper_model() {
                    "available".to_string()
                } else {
                    "unavailable".to_string()
                }
            }
        };

        ModelStatusDto {
            catalog_root: status.root.display().to_string(),
            root_exists: status.root_exists,
            total: status.total,
            available: status.available,
            unavailable: status.unavailable,
            incomplete: status.incomplete,
            active_id: selected.as_ref().map(|s| s.id.clone()).unwrap_or_default(),
            active_kind: selected
                .as_ref()
                .map(|s| s.kind.as_str().to_string())
                .unwrap_or_default(),
            active_path: selected
                .as_ref()
                .map(|s| s.path.display().to_string())
                .unwrap_or_default(),
            active_status,
            piper_model_path: self
                .piper_model_path()
                .map(|p| p.display().to_string())
                .unwrap_or_default(),
            message: status.message(),
        }
    }
}

pub fn list_models(engine: &BridgeEngine) -> Vec<ModelInfoDto> {
    engine.list_models()
}

pub fn inspect_model(engine: &BridgeEngine, id: &str) -> Result<ModelInfoDto, BridgeError> {
    engine.inspect_model(id)
}

pub fn select_model(engine: &BridgeEngine, id: &str) -> Result<ModelInfoDto, BridgeError> {
    engine.select_model(id)
}

pub fn model_status(engine: &BridgeEngine) -> ModelStatusDto {
    engine.model_status()
}

pub fn clear_model_selection(engine: &BridgeEngine) -> Result<(), BridgeError> {
    engine.clear_model_selection()
}

// ---- M18: sources, import, download, verify, remove -----------------------

fn preset_file_dto(file: &audioticon_core::PresetFile) -> PresetFileDto {
    PresetFileDto {
        role: file.role.as_str().to_string(),
        file_name: file.file_name.clone(),
        url: file.url.clone(),
        sha256: file.sha256.clone().unwrap_or_default(),
        size_bytes: file.size_bytes,
    }
}

fn remote_candidate_dto(candidate: audioticon_core::RemoteCandidate) -> RemoteCandidateDto {
    RemoteCandidateDto {
        id: candidate.id,
        kind: candidate.kind.as_str().to_string(),
        name: candidate.name,
        author: candidate.author,
        license: candidate.license,
        language: candidate.language.unwrap_or_default(),
        format: candidate.format.as_str().to_string(),
        size_bytes: candidate.size_bytes,
        source: candidate.source.as_str().to_string(),
        source_reference: candidate.source_reference,
        origin_url: candidate.origin_url,
        curated: candidate.curated,
        downloadable: candidate.downloadable,
        blockers: candidate.blockers,
        files: candidate.files.iter().map(preset_file_dto).collect(),
    }
}

fn import_result_dto(
    import_id: u64,
    status: &str,
    model: Option<&audioticon_core::CatalogModel>,
    message: String,
    source: String,
) -> ImportResultDto {
    ImportResultDto {
        import_id,
        status: status.to_string(),
        model_id: model.map(|m| m.id.clone()).unwrap_or_default(),
        model_path: model
            .map(|m| m.path.display().to_string())
            .unwrap_or_default(),
        model_status: model
            .map(|m| m.status.as_str().to_string())
            .unwrap_or_else(|| "unavailable".to_string()),
        message,
        source,
    }
}

impl BridgeEngine {
    /// The four ways of adding a model (curated / local / GitHub / Hugging Face).
    pub fn list_sources(&self) -> Vec<ModelSourceDto> {
        audioticon_core::list_sources()
            .into_iter()
            .map(|source| ModelSourceDto {
                kind: source.kind.as_str().to_string(),
                name: source.name,
                detail: source.detail,
                network: source.network,
                requires_explicit_consent: source.requires_explicit_consent,
            })
            .collect()
    }

    /// Curated presets with their on-disk state. Pure listing: no network, no
    /// downloads, no filesystem writes.
    pub fn list_curated_presets(&self) -> Vec<CuratedPresetDto> {
        audioticon_core::builtin_presets()
            .into_iter()
            .map(|preset| {
                let installed = self.catalog.inspect(&preset.id);
                CuratedPresetDto {
                    id: preset.id,
                    kind: preset.kind.as_str().to_string(),
                    name: preset.name,
                    author: preset.author,
                    license: preset.license,
                    language: preset.language.unwrap_or_default(),
                    format: preset.format.as_str().to_string(),
                    size_bytes: preset.size_bytes,
                    source: preset.source.as_str().to_string(),
                    source_reference: preset.source_reference,
                    origin: preset.origin,
                    description: preset.description,
                    files: preset.files.iter().map(preset_file_dto).collect(),
                    installed: installed.is_some(),
                    installed_status: installed
                        .map(|m| m.status.as_str().to_string())
                        .unwrap_or_else(|| "not_installed".to_string()),
                }
            })
            .collect()
    }

    /// Everything the UI must show before a download. No transfer happens here.
    pub fn inspect_remote_candidate(
        &self,
        reference: &str,
    ) -> Result<RemoteCandidateDto, BridgeError> {
        audioticon_core::describe_remote_reference(reference)
            .map(remote_candidate_dto)
            .map_err(|err| BridgeError::ModelImport(err.to_string()))
    }

    /// Imports a local file/directory/ZIP. Unknown or unsafe formats are
    /// rejected before anything lands in the catalog.
    pub fn import_local_model(
        &self,
        path: &str,
        kind: &str,
        accept_unknown_license: bool,
    ) -> Result<ImportResultDto, BridgeError> {
        let parsed_kind = if kind.trim().is_empty() {
            None
        } else {
            Some(audioticon_core::ModelKind::parse(kind).ok_or_else(|| {
                BridgeError::InvalidRequest(format!("nieznany rodzaj modelu: '{kind}'"))
            })?)
        };
        let model = self
            .importer
            .import_local(std::path::Path::new(path), parsed_kind, accept_unknown_license)
            .map_err(|err| BridgeError::ModelImport(err.to_string()))?;
        if model.kind == audioticon_core::ModelKind::PiperTts {
            self.refresh_piper_engine();
        }
        Ok(import_result_dto(
            0,
            "completed",
            Some(&model),
            format!("zaimportowano {} ({})", model.id, model.path.display()),
            format!("local:{path}"),
        ))
    }

    /// Downloads only on an explicit request: `consent` must be true and the
    /// engine must have a transport. No transport = plain error, no transfer.
    pub fn download_remote_model(
        &self,
        reference: &str,
        expected_sha256: &str,
        expected_size: u64,
        consent: bool,
    ) -> Result<ImportResultDto, BridgeError> {
        let mut request = audioticon_core::RemoteDownloadRequest::new(reference);
        request.consent = if consent {
            audioticon_core::ImportConsent::Explicit
        } else {
            audioticon_core::ImportConsent::NotGiven
        };
        request.expected_size = expected_size;
        let sha = expected_sha256.trim();
        request.expected_sha256 = (!sha.is_empty()).then(|| sha.to_string());

        let outcome = self
            .importer
            .download(&request)
            .map_err(|err| BridgeError::ModelImport(err.to_string()))?;
        if let Some(model) = &outcome.model {
            if model.kind == audioticon_core::ModelKind::PiperTts {
                self.refresh_piper_engine();
            }
        }
        Ok(import_result_dto(
            outcome.import_id,
            outcome.status.as_str(),
            outcome.model.as_ref(),
            outcome.message,
            outcome.source,
        ))
    }

    /// Cancels a running import. `false` when the id already finished.
    pub fn cancel_import(&self, import_id: u64) -> bool {
        self.importer.cancel(import_id)
    }

    /// Handle for `cancel_import` from another thread (0 = nothing running).
    pub fn active_import_id(&self) -> u64 {
        self.importer.active_import_id()
    }

    /// Explicit checksum verification of an installed model.
    pub fn verify_model(&self, id: &str) -> Result<VerifyResultDto, BridgeError> {
        let result = self
            .importer
            .verify(id)
            .map_err(|err| BridgeError::ModelImport(err.to_string()))?;
        Ok(VerifyResultDto {
            id: result.id,
            kind: result.kind.as_str().to_string(),
            status: result.status.as_str().to_string(),
            checked: result.checked,
            expected_sha256: result.expected_sha256.unwrap_or_default(),
            actual_sha256: result.actual_sha256.unwrap_or_default(),
            matches: result.matches.unwrap_or(true),
            size_bytes: result.size_bytes,
            message: result.message,
        })
    }

    /// Removes a model (weights, config, checksum) and clears its selection.
    pub fn remove_model(&self, id: &str) -> Result<RemoveResultDto, BridgeError> {
        let result = self
            .importer
            .remove(id)
            .map_err(|err| BridgeError::ModelImport(err.to_string()))?;
        self.refresh_piper_engine();
        Ok(RemoveResultDto {
            id: result.id,
            kind: result.kind.as_str().to_string(),
            removed_files: result.removed_files.len() as u32,
            selection_cleared: result.selection_cleared,
        })
    }

    fn refresh_piper_engine(&self) {
        *self.piper_guard() = resolve_piper_model_in(&self.catalog)
            .map(|path| Arc::new(audioticon_core::PiperBridge::new(path)));
    }
}

pub fn list_sources(engine: &BridgeEngine) -> Vec<ModelSourceDto> {
    engine.list_sources()
}

pub fn list_curated_presets(engine: &BridgeEngine) -> Vec<CuratedPresetDto> {
    engine.list_curated_presets()
}

pub fn inspect_remote_candidate(
    engine: &BridgeEngine,
    reference: &str,
) -> Result<RemoteCandidateDto, BridgeError> {
    engine.inspect_remote_candidate(reference)
}

pub fn import_local(
    engine: &BridgeEngine,
    path: &str,
    kind: &str,
    accept_unknown_license: bool,
) -> Result<ImportResultDto, BridgeError> {
    engine.import_local_model(path, kind, accept_unknown_license)
}

pub fn download_remote(
    engine: &BridgeEngine,
    reference: &str,
    expected_sha256: &str,
    expected_size: u64,
    consent: bool,
) -> Result<ImportResultDto, BridgeError> {
    engine.download_remote_model(reference, expected_sha256, expected_size, consent)
}

pub fn cancel_import(engine: &BridgeEngine, import_id: u64) -> bool {
    engine.cancel_import(import_id)
}

pub fn active_import_id(engine: &BridgeEngine) -> u64 {
    engine.active_import_id()
}

pub fn verify_model(engine: &BridgeEngine, id: &str) -> Result<VerifyResultDto, BridgeError> {
    engine.verify_model(id)
}

pub fn remove_model(engine: &BridgeEngine, id: &str) -> Result<RemoveResultDto, BridgeError> {
    engine.remove_model(id)
}

// ============================================================================
// Unit Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_core_version_reports_core_sample_rate() {
        // Must match audioticon_core::DEFAULT_SAMPLE_RATE (44.1 kHz), and must be
        // read from the core crate rather than duplicated in the bridge.
        assert_eq!(core_version(), 44100);
        assert_eq!(core_version(), audioticon_core::DEFAULT_SAMPLE_RATE);
    }

    #[test]
    fn test_create_engine() {
        let engine = create_engine().expect("Nie udało się utworzyć BridgeEngine");
        let status = engine.get_mock_render_status(1, 0, 10);
        assert_eq!(status.stage, TaskStage::Queued);
        assert_eq!(status.current_segment, 0);
        assert_eq!(status.total_segments, 10);
    }

    #[test]
    fn test_parse_directed_text_basic() {
        let engine = BridgeEngine::new();
        let input = "Rozdział 1: Początek. 🛑 To jest test 15:45.";
        let res = engine.parse_directed_text(input, true).expect("Błąd parsowania");

        assert!(!res.segments.is_empty(), "Brak wygenerowanych segmentów");
        assert!(
            res.plain_text.contains("piętnasta czterdzieści pięć"),
            "Verbalizer polski nie przekonwertował godziny: {}",
            res.plain_text
        );
        assert!(res.directives.iter().any(|d| d.kind == "pause"));
    }

    #[test]
    fn test_audio_buffer_pcm_generation() {
        let engine = BridgeEngine::new();
        let tone = engine.create_test_tone_pcm(440.0, 500).expect("Błąd generowania tonu");

        assert_eq!(tone.sample_rate, 22050);
        assert_eq!(tone.channels, 1);
        assert_eq!(tone.bits_per_sample, 16);
        assert_eq!(tone.duration_ms, 500);
        assert_eq!(tone.samples.len(), 11025);
    }

    #[test]
    fn test_render_status_progression() {
        let engine = BridgeEngine::new();
        let s0 = engine.get_mock_render_status(42, 0, 4);
        assert_eq!(s0.stage, TaskStage::Queued);
        assert_eq!(s0.progress_percent, 0.0);

        let s2 = engine.get_mock_render_status(42, 2, 4);
        assert_eq!(s2.stage, TaskStage::Synthesizing);
        assert_eq!(s2.progress_percent, 50.0);

        let s4 = engine.get_mock_render_status(42, 4, 4);
        assert_eq!(s4.stage, TaskStage::Finished);
        assert_eq!(s4.progress_percent, 100.0);
    }

    #[test]
    fn test_empty_input_handling() {
        let engine = BridgeEngine::new();
        let res = engine.parse_directed_text("", true).unwrap();
        assert!(res.segments.is_empty());
        assert_eq!(res.plain_text, "");
    }

    // ---- M5: job runtime ---------------------------------------------------

    fn test_dir(name: &str) -> PathBuf {
        let dir = std::env::temp_dir().join(format!(
            "audioticon-m5-{name}-{}",
            std::process::id()
        ));
        let _ = std::fs::remove_dir_all(&dir);
        dir
    }

    /// Polls the registry the way the Qt side does, but from a plain thread.
    fn wait_for_terminal(engine: &BridgeEngine, job_id: u64, timeout: Duration) -> JobStateDto {
        let deadline = std::time::Instant::now() + timeout;
        loop {
            let state = engine.job_state(job_id);
            let terminal = matches!(
                state.stage,
                TaskStage::Finished | TaskStage::Failed | TaskStage::Cancelled
            );
            if terminal {
                return state;
            }
            assert!(
                std::time::Instant::now() < deadline,
                "job {job_id} nie zakończył się w {timeout:?} (stan: {state:?})"
            );
            std::thread::sleep(Duration::from_millis(10));
        }
    }

    #[test]
    fn test_job_flow_queued_progress_finished() {
        let engine = BridgeEngine::new_mock();
        let dir = test_dir("flow");
        let job_id = engine
            .start_job("To jest test przepływu zadania.", dir.to_str().unwrap())
            .expect("start_job");
        assert!(job_id > 0);

        // The registration is synchronous, the first stage is already Queued.
        let first = engine.job_state(job_id);
        assert!(
            matches!(first.stage, TaskStage::Queued | TaskStage::Synthesizing),
            "nieoczekiwany stan początkowy: {:?}",
            first.stage
        );

        let mut saw_synthesizing = false;
        let mut max_percent = 0.0f32;
        let deadline = std::time::Instant::now() + Duration::from_secs(10);
        let final_state = loop {
            let state = engine.job_state(job_id);
            saw_synthesizing |= state.stage == TaskStage::Synthesizing;
            max_percent = max_percent.max(state.progress_percent);
            if state.stage == TaskStage::Finished {
                break state;
            }
            assert!(
                state.stage != TaskStage::Failed,
                "job niepowiódł się: {}",
                state.error_message
            );
            assert!(
                std::time::Instant::now() < deadline,
                "job nie zakończył się w czasie (stan: {state:?})"
            );
            std::thread::sleep(Duration::from_millis(10));
        };

        assert!(saw_synthesizing, "nie zaobserwowano stanu Synthesizing");
        assert!(max_percent > 0.0, "postęp nigdy nie wzrósł ponad 0");
        assert_eq!(final_state.progress_percent, 100.0);
        assert_eq!(final_state.current_segment, final_state.total_segments);
        assert!(final_state.error_message.is_empty());
        assert!(!final_state.output_path.is_empty());

        let output = PathBuf::from(&final_state.output_path);
        assert!(output.is_file(), "brak pliku wynikowego {}", output.display());
        // The placeholder is a real WAV, not a renamed blob: 44-byte header plus
        // 200 ms of 16-bit mono at the core sample rate.
        let bytes = std::fs::read(&output).expect("odczyt pliku wynikowego");
        assert_eq!(&bytes[0..4], b"RIFF");
        assert_eq!(&bytes[8..12], b"WAVE");
        let channels = u16::from_le_bytes([bytes[22], bytes[23]]);
        let sample_rate = u32::from_le_bytes([bytes[24], bytes[25], bytes[26], bytes[27]]);
        assert_eq!(channels, 1, "kanały: {channels}");
        assert_eq!(sample_rate, audioticon_core::DEFAULT_SAMPLE_RATE);
        // M7: the output may be the M5 200 ms placeholder OR a real Piper render,
        // so only the container/shape invariants are asserted here.
        assert!(bytes.len() > 44, "plik WAV jest pusty lub uszkodzony");

        assert_eq!(engine.active_jobs(), 0, "rejestr nie został opróżniony");
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn test_job_cancel_stops_the_worker() {
        let engine = BridgeEngine::new_mock();
        let dir = test_dir("cancel");
        let job_id = engine
            .start_job("Anulujemy to zadanie w trakcie syntezy.", dir.to_str().unwrap())
            .expect("start_job");

        let deadline = std::time::Instant::now() + Duration::from_secs(10);
        loop {
            if engine.job_state(job_id).progress_percent > 0.0 {
                break;
            }
            assert!(std::time::Instant::now() < deadline, "brak postępu do anulowania");
            std::thread::sleep(Duration::from_millis(5));
        }

        assert!(engine.cancel_job(job_id), "cancel powinien się udać");
        let cancelled = engine.job_state(job_id);
        assert_eq!(cancelled.stage, TaskStage::Cancelled);
        assert!(cancelled.progress_percent < 100.0);
        assert!(cancelled.output_path.is_empty());
        assert_eq!(engine.active_jobs(), 0);

        // The worker must not overwrite the terminal state afterwards.
        std::thread::sleep(Duration::from_millis(400));
        let later = engine.job_state(job_id);
        assert_eq!(later.stage, TaskStage::Cancelled);
        assert_eq!(later.progress_percent, cancelled.progress_percent);
        assert!(!engine.cancel_job(job_id), "drugi cancel nic nie zmienia");
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn test_job_failure_reports_error_message() {
        let engine = BridgeEngine::new();
        // /dev/null is a regular file, so creating a directory inside it fails
        // deterministically - a real I/O failure, not a stubbed one.
        let job_id = engine
            .start_job("Zadanie bez miejsca na plik.", "/dev/null/audioticon")
            .expect("start_job");
        let state = wait_for_terminal(&engine, job_id, Duration::from_secs(10));
        assert_eq!(state.stage, TaskStage::Failed);
        assert!(!state.error_message.is_empty(), "brak komunikatu błędu");
        assert!(state.output_path.is_empty());
        assert_eq!(engine.active_jobs(), 0);
    }

    #[test]
    fn test_start_job_rejects_empty_text() {
        let engine = BridgeEngine::new();
        assert!(engine.start_job("   ", "").is_err());
        assert_eq!(engine.active_jobs(), 0);
    }

    #[test]
    fn test_job_state_of_unknown_id_is_idle() {
        let engine = BridgeEngine::new();
        let state = engine.job_state(424242);
        assert_eq!(state.stage, TaskStage::Idle);
        assert_eq!(state.progress_percent, 0.0);
        assert!(state.status_message.is_empty());
        assert!(!engine.cancel_job(424242));
    }

    #[test]
    fn test_real_piper_synthesis_one_segment() {
        // Conditional: requires a local model + a python with piper. Skip gracefully
        // when either is missing so the suite stays green in minimal environments.
        let model_path = match std::env::var("AUDIOTICON_PIPER_MODEL") {
            Ok(v) if !v.trim().is_empty() && std::path::Path::new(v.trim()).is_file() => {
                v.trim().to_string()
            }
            _ => {
                // Fall back to the known local pl_bass location if present.
                let candidate = "/home/jaro/Portable-Local-Studio/app/tts-models/piper/pl_PL-bass-high.onnx";
                if std::path::Path::new(candidate).is_file() {
                    candidate.to_string()
                } else {
                    println!("SKIP: brak modelu Piper (AUDIOTICON_PIPER_MODEL)");
                    return;
                }
            }
        };

        // Ensure python with piper is usable.
        let venv = "/home/jaro/Portable-Local-Studio/app/tts-runtime/piper-venv";
        if !std::path::Path::new(venv).is_dir() {
            println!("SKIP: brak venv piper");
            return;
        }
        std::env::set_var("VIRTUAL_ENV", venv);
        std::env::set_var("AUDIOTICON_PIPER_MODEL", &model_path);

        let engine = BridgeEngine::new();
        assert!(engine.has_piper_model(), "model nie został wykryty mimo ścieżki");

        let dir = test_dir("piper-real");
        let job_id = engine
            .start_job("To jest test prawdziwej syntezy Piper.", dir.to_str().unwrap())
            .expect("start_job");
        assert!(job_id > 0);

        let state = wait_for_terminal(&engine, job_id, std::time::Duration::from_secs(60));
        assert_eq!(state.stage, TaskStage::Finished, "błąd: {}", state.error_message);
        assert!(!state.output_path.is_empty());
        assert_eq!(state.progress_percent, 100.0);

        let output = std::path::PathBuf::from(&state.output_path);
        assert!(output.is_file(), "brak pliku wynikowego");
        let bytes = std::fs::read(&output).expect("odczyt pliku wynikowego");
        assert!(&bytes[0..4] == b"RIFF");
        assert!(&bytes[8..12] == b"WAVE");
        let channels = u16::from_le_bytes([bytes[22], bytes[23]]);
        let sample_rate = u32::from_le_bytes([bytes[24], bytes[25], bytes[26], bytes[27]]);
        assert_eq!(channels, 1);
        assert_eq!(sample_rate, audioticon_core::DEFAULT_SAMPLE_RATE);
        assert!(bytes.len() > 1000, "plik zbyt mały: {} B", bytes.len());

        // Cache hit: second identical request reuses the file.
        let job_id2 = engine
            .start_job("To jest test prawdziwej syntezy Piper.", dir.to_str().unwrap())
            .expect("start_job");
        let state2 = wait_for_terminal(&engine, job_id2, std::time::Duration::from_secs(60));
        assert_eq!(state2.stage, TaskStage::Finished);
        assert!(state2.status_message.contains("Cache hit"), "brak cache hit: {}", state2.status_message);

        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn test_real_piper_chapter_pipeline_cache() {
        // Conditional: requires a local model + a python with piper.
        let model_path = match std::env::var("AUDIOTICON_PIPER_MODEL") {
            Ok(v) if !v.trim().is_empty() && std::path::Path::new(v.trim()).is_file() => {
                v.trim().to_string()
            }
            _ => {
                let candidate = "/home/jaro/Portable-Local-Studio/app/tts-models/piper/pl_PL-bass-high.onnx";
                if std::path::Path::new(candidate).is_file() {
                    candidate.to_string()
                } else {
                    println!("SKIP: brak modelu Piper (AUDIOTICON_PIPER_MODEL)");
                    return;
                }
            }
        };

        let venv = "/home/jaro/Portable-Local-Studio/app/tts-runtime/piper-venv";
        if !std::path::Path::new(venv).is_dir() {
            println!("SKIP: brak venv piper");
            return;
        }
        std::env::set_var("VIRTUAL_ENV", venv);
        std::env::set_var("AUDIOTICON_PIPER_MODEL", &model_path);

        let engine = BridgeEngine::new();
        assert!(engine.has_piper_model());

        let dir = test_dir("piper-chapter");

        let text1 = "Zdanie pierwsze testu. Zdanie drugie testu. Zdanie trzecie testu.";
        let job1 = engine.start_job(text1, dir.to_str().unwrap()).expect("start_job 1");
        let state1 = wait_for_terminal(&engine, job1, std::time::Duration::from_secs(120));
        assert_eq!(state1.stage, TaskStage::Finished, "render 1: {}", state1.error_message);
        assert!(!state1.output_path.is_empty());
        let out1 = std::path::PathBuf::from(&state1.output_path);
        assert!(out1.is_file());
        let bytes1 = std::fs::read(&out1).expect("odczyt rozdziału 1");
        assert_eq!(&bytes1[0..4], b"RIFF");
        assert_eq!(&bytes1[8..12], b"WAVE");
        assert!(bytes1.len() > 1000);

        // Zmiana jednego zdania → drugi render z cache hitami dla pozostałych.
        let text2 = "Zdanie pierwsze testu. ZMIENIONE zdanie drugie testu. Zdanie trzecie testu.";
        let job2 = engine.start_job(text2, dir.to_str().unwrap()).expect("start_job 2");
        let state2 = wait_for_terminal(&engine, job2, std::time::Duration::from_secs(120));
        assert_eq!(state2.stage, TaskStage::Finished, "render 2: {}", state2.error_message);
        assert!(!state2.output_path.is_empty());
        assert!(state2.status_message.contains("Cache hit"), "brak cache hit: {}", state2.status_message);

        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn test_fallback_without_model_reports_missing_voice_model() {
        // Fallback bez modelu (B13) nie może dawać fałszywego "Finished",
        // lecz jasno raportować TaskStage::Failed z komunikatem "Brak modelu głosu".
        let engine = BridgeEngine::new_with_mock(false);
        if engine.has_piper_model() {
            // Jeśli środowisko ma globalnie podpięty model, pomijamy ten test
            return;
        }
        let dir = test_dir("fallback-no-model");
        let job_id = engine
            .start_job("To jest test fallbacku bez modelu.", dir.to_str().unwrap())
            .expect("start_job");
        assert!(job_id > 0);

        let state = wait_for_terminal(&engine, job_id, Duration::from_secs(10));
        assert_eq!(state.stage, TaskStage::Failed, "Fallback bez modelu nie może być Finished (B13)");
        assert_eq!(state.status_message, "Brak modelu głosu");
        assert!(state.error_message.contains("Brak modelu głosu"));
        assert!(state.output_path.is_empty(), "Fallback bez modelu nie może tworzyć fałszywego audio");

        let _ = std::fs::remove_dir_all(&dir);
    }

    // ---- M17: local model catalog contract ---------------------------------

    /// Isolated catalog root shaped like the real one:
    /// `<tmp>/audioticon/models`, so the persisted selection lands in
    /// `<tmp>/audioticon/model_selection.json` and never in /tmp itself.
    fn catalog_test_dir(name: &str) -> PathBuf {
        let base = std::env::temp_dir().join(format!(
            "audioticon-m17-bridge-{name}-{}",
            std::process::id()
        ));
        let _ = std::fs::remove_dir_all(&base);
        let root = base.join("audioticon").join("models");
        std::fs::create_dir_all(&root).unwrap();
        root
    }

    fn cleanup_catalog(root: &std::path::Path) {
        if let Some(app_dir) = root.parent().and_then(|p| p.parent()) {
            let _ = std::fs::remove_dir_all(app_dir);
        }
    }

    fn seed_piper_model(root: &std::path::Path, stem: &str) -> PathBuf {
        let dir = root.join("piper");
        std::fs::create_dir_all(&dir).unwrap();
        let onnx = dir.join(format!("{stem}.onnx"));
        std::fs::write(&onnx, vec![0x08u8; 4096]).unwrap();
        std::fs::write(
            dir.join(format!("{stem}.onnx.json")),
            r#"{"language":{"code":"pl_PL"}}"#,
        )
        .unwrap();
        onnx
    }

    #[test]
    fn test_catalog_lists_models_and_reports_unavailable_status() {
        let root = catalog_test_dir("empty");
        let engine = BridgeEngine::new_with_catalog(root.clone(), false);

        assert!(engine.list_models().is_empty());

        let status = engine.model_status();
        assert_eq!(status.total, 0);
        assert_eq!(status.available, 0);
        assert_eq!(status.unavailable, 0);
        assert_eq!(status.active_status, "unavailable");
        assert!(status.active_id.is_empty());
        assert!(status.piper_model_path.is_empty());
        assert!(status.message.contains("Brak modeli"), "{}", status.message);
        assert!(status.catalog_root.starts_with(root.to_str().unwrap()));

        assert!(engine.inspect_model("piper/nie-ma").is_err());
        assert!(engine.select_model("piper/nie-ma").is_err());

        cleanup_catalog(&root);
    }

    #[test]
    fn test_catalog_select_persists_and_activates_piper() {
        let root = catalog_test_dir("select");
        seed_piper_model(&root, "pl_PL-darkman-medium");

        let engine = BridgeEngine::new_with_catalog(root.clone(), false);
        let models = engine.list_models();
        assert_eq!(models.len(), 1);
        let info = &models[0];
        assert_eq!(info.id, "piper/pl_PL-darkman-medium");
        assert_eq!(info.kind, "piper_tts");
        assert_eq!(info.format, "onnx");
        assert_eq!(info.language, "pl_PL");
        assert_eq!(info.size_bytes, 4096);
        assert_eq!(info.status, "available");
        assert!(info.checksum_sha256.is_empty());

        // Sama obecność modelu nie znaczy, że jest aktywny.
        assert!(engine.model_status().active_id.is_empty());

        let selected = engine.select_model(&info.id).expect("select_model");
        assert_eq!(selected.id, info.id);
        assert!(engine.has_piper_model(), "wybór modelu Piper musi podpiąć silnik");
        assert_eq!(
            engine.piper_model_path().unwrap(),
            PathBuf::from(&info.path)
        );

        let status = engine.model_status();
        assert_eq!(status.active_id, info.id);
        assert_eq!(status.active_kind, "piper_tts");
        assert_eq!(status.active_status, "available");
        assert!(!status.piper_model_path.is_empty());

        // Persystencja: nowy silnik na tym samym katalogu odtwarza wybór.
        let reopened = BridgeEngine::new_with_catalog(root.clone(), false);
        assert!(reopened.has_piper_model());
        assert_eq!(reopened.model_status().active_id, info.id);

        // Zmiana wyboru na drugi model nadpisuje poprzedni.
        seed_piper_model(&root, "pl_PL-gosia-medium");
        reopened.select_model("piper/pl_PL-gosia-medium").unwrap();
        assert_eq!(reopened.model_status().active_id, "piper/pl_PL-gosia-medium");

        // Czyszczenie usuwa utrwalony wybór.
        reopened.clear_model_selection().unwrap();
        assert!(reopened.model_status().active_id.is_empty());

        cleanup_catalog(&root);
    }

    #[test]
    fn test_catalog_rejects_corrupt_and_incomplete_models() {
        let root = catalog_test_dir("broken");
        let piper = root.join("piper");
        std::fs::create_dir_all(&piper).unwrap();
        // Uszkodzony: pusty plik wag.
        std::fs::write(piper.join("pl_PL-empty-medium.onnx"), b"").unwrap();
        std::fs::write(piper.join("pl_PL-empty-medium.onnx.json"), "{}").unwrap();
        // Niekompletny: wagi bez konfiguracji.
        std::fs::write(piper.join("pl_PL-nometa-medium.onnx"), vec![1u8; 4096]).unwrap();

        let engine = BridgeEngine::new_with_catalog(root.clone(), false);
        let models = engine.list_models();
        assert_eq!(models.len(), 2);
        assert!(models.iter().any(|m| m.status == "corrupt"));
        assert!(models.iter().any(|m| m.status == "missing_metadata"));
        assert!(models.iter().all(|m| !m.detail.is_empty()));

        let status = engine.model_status();
        assert_eq!(status.available, 0);
        assert_eq!(status.unavailable, 2);
        assert!(status.message.contains("wszystkie niekompletne"));

        for model in &models {
            assert!(
                engine.select_model(&model.id).is_err(),
                "model {} nie może zostać wybrany",
                model.id
            );
        }
        assert!(!engine.has_piper_model(), "uszkodzony model nie może napędzać renderu");
        assert!(engine.model_status().active_id.is_empty());

        // B13: bez modelu zadanie kończy się jawnym błędem, nie "Finished".
        let out = catalog_test_dir("broken-out");
        let job_id = engine
            .start_job("Render bez modelu.", out.to_str().unwrap())
            .expect("start_job");
        let state = wait_for_terminal(&engine, job_id, Duration::from_secs(10));
        assert_eq!(state.stage, TaskStage::Failed);
        assert!(state.error_message.contains("Brak modelu głosu"));
        assert!(state.output_path.is_empty());

        cleanup_catalog(&root);
        cleanup_catalog(&out);
    }

    #[test]
    fn test_catalog_selected_model_that_disappears_is_reported_unavailable() {
        let root = catalog_test_dir("vanish");
        let onnx = seed_piper_model(&root, "pl_PL-darkman-medium");

        let engine = BridgeEngine::new_with_catalog(root.clone(), false);
        engine.select_model("piper/pl_PL-darkman-medium").unwrap();
        assert!(engine.has_piper_model());

        std::fs::remove_file(&onnx).unwrap();

        let status = engine.model_status();
        assert_eq!(status.active_id, "piper/pl_PL-darkman-medium");
        assert_eq!(status.active_status, "unavailable");
        assert_eq!(status.available, 0);
        assert!(status.message.contains("nie jest już dostępny"));

        // Nowy silnik nie podpina znikniętego modelu.
        let reopened = BridgeEngine::new_with_catalog(root.clone(), false);
        assert!(!reopened.has_piper_model());

        cleanup_catalog(&root);
    }

    // ---- M18: sources, local import, download, verify, remove -------------

    struct FakeTransport {
        data: Vec<(String, Vec<u8>)>,
    }

    impl audioticon_core::RemoteTransport for FakeTransport {
        fn fetch(
            &self,
            url: &str,
            sink: &mut dyn FnMut(&[u8]) -> Result<(), audioticon_core::TransportError>,
        ) -> Result<(), audioticon_core::TransportError> {
            match self.data.iter().find(|(key, _)| key == url) {
                Some((_, bytes)) => sink(bytes),
                None => Err(audioticon_core::TransportError::HttpStatus(404)),
            }
        }
    }

    fn piper_source_dir(root: &std::path::Path, stem: &str) -> PathBuf {
        let dir = root.join(format!("src-{stem}"));
        std::fs::create_dir_all(&dir).unwrap();
        std::fs::write(dir.join(format!("{stem}.onnx")), vec![0x08u8; 4096]).unwrap();
        std::fs::write(
            dir.join(format!("{stem}.onnx.json")),
            r#"{"language":{"code":"pl_PL"},"license":"CC0-1.0"}"#,
        )
        .unwrap();
        dir
    }

    #[test]
    fn test_m18_sources_and_presets_are_offline() {
        let root = catalog_test_dir("m18-sources");
        let engine = BridgeEngine::new_with_catalog(root.clone(), false);

        let sources = engine.list_sources();
        assert_eq!(sources.len(), 4);
        assert_eq!(sources[0].kind, "curated");
        assert!(sources[0].requires_explicit_consent);
        assert!(!sources[0].network);
        assert!(sources.iter().any(|s| s.kind == "local" && !s.network));
        assert!(sources
            .iter()
            .any(|s| s.kind == "github_release" && s.network));
        assert!(sources
            .iter()
            .any(|s| s.kind == "hugging_face" && s.network));

        let presets = engine.list_curated_presets();
        assert!(presets.len() >= 8);
        let darkman = presets
            .iter()
            .find(|p| p.id == "piper/pl_PL-darkman-medium")
            .expect("preset Portable Local Studio");
        assert_eq!(darkman.origin, "Portable Local Studio");
        assert_eq!(darkman.format, "onnx");
        assert!(!darkman.author.is_empty());
        assert!(!darkman.license.is_empty());
        assert!(darkman.size_bytes > 0);
        assert!(!darkman.installed);
        assert_eq!(darkman.installed_status, "not_installed");

        // Listowanie presetów niczego nie instaluje i nie tworzy stagingu.
        assert!(engine.list_models().is_empty());
        assert!(!root.join(".staging").exists());

        // Podgląd kandydata zdalnego pokazuje metadane przed pobraniem.
        let candidate = engine
            .inspect_remote_candidate("preset:piper/pl_PL-darkman-medium")
            .unwrap();
        assert!(candidate.curated);
        assert!(candidate.downloadable);
        assert!(!candidate.author.is_empty());
        assert!(!candidate.license.is_empty());
        assert!(candidate.size_bytes > 0);
        assert_eq!(candidate.files.len(), 2);

        cleanup_catalog(&root);
    }

    #[test]
    fn test_m18_local_import_is_validated_persisted_and_removable() {
        let root = catalog_test_dir("m18-local");
        let engine = BridgeEngine::new_with_catalog(root.clone(), false);
        let source = piper_source_dir(&root, "pl_PL-gosia-medium");

        let imported = engine
            .import_local_model(source.to_str().unwrap(), "", false)
            .expect("import lokalny");
        assert_eq!(imported.status, "completed");
        assert_eq!(imported.model_id, "piper/pl_PL-gosia-medium");
        assert_eq!(imported.model_status, "available");
        assert!(imported.source.starts_with("local:"));

        // Zły format: odrzucony, katalog bez zmian.
        let bad = root.join("bad.txt");
        std::fs::write(&bad, b"not a model").unwrap();
        let error = engine
            .import_local_model(bad.to_str().unwrap(), "", false)
            .unwrap_err();
        assert!(error.to_string().contains("format"), "{error}");
        assert_eq!(engine.list_models().len(), 1);

        // Nowy silnik (restart) widzi model.
        let reopened = BridgeEngine::new_with_catalog(root.clone(), false);
        assert_eq!(reopened.list_models().len(), 1);
        assert!(reopened.has_piper_model());

        let verified = reopened.verify_model("piper/pl_PL-gosia-medium").unwrap();
        assert_eq!(verified.status, "available");
        assert!(!verified.checked, "brak sidecara w imporcie lokalnym");
        assert!(!verified.actual_sha256.is_empty());

        reopened.select_model("piper/pl_PL-gosia-medium").unwrap();
        let removed = reopened.remove_model("piper/pl_PL-gosia-medium").unwrap();
        assert!(removed.selection_cleared);
        assert!(removed.removed_files >= 2);
        assert!(reopened.list_models().is_empty());
        assert!(reopened.model_status().active_id.is_empty());
        assert!(!reopened.has_piper_model());

        cleanup_catalog(&root);
    }

    #[test]
    fn test_m18_remote_download_needs_consent_and_network() {
        let root = catalog_test_dir("m18-consent");
        let engine = BridgeEngine::new_with_catalog(root.clone(), false);

        // Bez zgody: brak transferu i jawny błąd.
        let denied = engine
            .download_remote_model("preset:piper/pl_PL-darkman-medium", "", 0, false)
            .unwrap_err();
        assert!(denied.to_string().contains("zgody"), "{denied}");

        // Zgodna, ale bez transportu: brak sieci, zero plików.
        let offline = engine
            .download_remote_model("preset:piper/pl_PL-darkman-medium", "", 0, true)
            .unwrap_err();
        assert!(offline.to_string().contains("sieci"), "{offline}");
        assert!(engine.list_models().is_empty());
        assert!(!root.join(".staging").exists());

        // Źródło spoza presetów wymaga checksumu i rozmiaru.
        let unverified = engine
            .download_remote_model("hf:owner/repo@main#model.gguf", "", 0, true)
            .unwrap_err();
        assert!(unverified.to_string().contains("SHA-256"), "{unverified}");

        assert_eq!(engine.active_import_id(), 0);
        assert!(!engine.cancel_import(999));

        cleanup_catalog(&root);
    }

    #[test]
    fn test_m18_download_with_transport_installs_and_verifies() {
        let root = catalog_test_dir("m18-download");
        let preset = audioticon_core::builtin_presets()
            .into_iter()
            .find(|p| p.id == "piper/pl_PL-darkman-medium")
            .unwrap();
        let weights = preset.weights_file().unwrap().url.clone();
        let config = format!("{weights}.json");
        let transport = Arc::new(FakeTransport {
            data: vec![
                (weights.clone(), vec![0x08u8; 4096]),
                (
                    config.clone(),
                    br#"{"language":{"code":"pl_PL"},"license":"CC0-1.0"}"#.to_vec(),
                ),
            ],
        });
        let engine = BridgeEngine::new_with_catalog_and_transport(root.clone(), false, transport);

        let result = engine
            .download_remote_model("preset:piper/pl_PL-darkman-medium", "", 0, true)
            .expect("pobranie z transportem testowym");
        assert_eq!(result.status, "completed");
        assert_eq!(result.model_id, "piper/pl_PL-darkman-medium");
        assert_eq!(result.model_status, "available");

        let models = engine.list_models();
        assert_eq!(models.len(), 1);
        assert!(!models[0].checksum_sha256.is_empty());
        assert!(engine.has_piper_model());

        let verified = engine.verify_model("piper/pl_PL-darkman-medium").unwrap();
        assert!(verified.checked);
        assert!(verified.matches);

        // Checksum mismatch po pobraniu: nic nie zostaje w katalogu.
        let other = "piper/pl_PL-gosia-medium";
        let other_preset = audioticon_core::builtin_presets()
            .into_iter()
            .find(|p| p.id == other)
            .unwrap();
        let other_weights = other_preset.weights_file().unwrap().url.clone();
        let transport2 = Arc::new(FakeTransport {
            data: vec![
                (other_weights.clone(), vec![0x08u8; 4096]),
                (
                    format!("{other_weights}.json"),
                    br#"{"license":"CC0-1.0"}"#.to_vec(),
                ),
            ],
        });
        let engine2 = BridgeEngine::new_with_catalog_and_transport(root.clone(), false, transport2);
        let mismatch = engine2
            .download_remote_model(other, &"0".repeat(64), 0, true)
            .unwrap_err();
        assert!(mismatch.to_string().contains("checksum"), "{mismatch}");
        assert!(engine2.inspect_model(other).is_err());

        cleanup_catalog(&root);
    }

    #[test]
    fn test_preflight_cxx_header_validity() {
        // Preflight B2: weryfikacja czy nagłówek cxx.h istnieje, nie jest pusty i zawiera 'namespace rust'
        let manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
        let backend = manifest_dir.parent().unwrap().parent().unwrap();
        let cxx_h = backend.join("target").join("cxxbridge").join("rust").join("cxx.h");
        assert!(cxx_h.is_file(), "B2: brak nagłówka {:?}", cxx_h);
        let size = std::fs::metadata(&cxx_h).unwrap().len();
        assert!(size > 1000, "B2: cxx.h jest pusty lub uszkodzony ({} B)", size);
        let content = std::fs::read_to_string(&cxx_h).unwrap();
        assert!(content.contains("namespace rust"), "B2: cxx.h nie zawiera 'namespace rust'");
    }
}
