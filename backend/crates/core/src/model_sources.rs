//! M18: źródła modeli — presety kurowane, import lokalny i zdalny.
//!
//! Ten moduł jest **opisem**, nie transportem: nie otwiera żadnego połączenia.
//! Zawiera:
//!
//! * katalog presetów znanych z Portable Local Studio (Piper PL/EN, whisper.cpp,
//!   lokalny LLM) wraz z autorem, licencją, formatem, rozmiarem i plikami,
//! * rozpoznawanie odwołań do źródeł zdalnych (`preset:<id>`, `hf:...`, `gh:...`),
//! * listę dozwolonych hostów pobierania.
//!
//! Pobieranie żyje w `model_import.rs` i wymaga jawnej zgody oraz włączonego
//! transportu. Dzięki temu „pokaż katalog” nigdy nie dotyka sieci.

use crate::model_catalog::{ModelFormat, ModelKind};

const MIB: u64 = 1024 * 1024;

/// Rodzaj źródła modelu, widoczny w UI jako osobna ścieżka dodawania.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ModelSourceKind {
    /// Presety wbudowane w aplikację (bez pobierania w tle).
    Curated,
    /// Plik lub katalog wskazany przez użytkownika.
    Local,
    /// Assety GitHub Releases, pobierane tylko po jawnym żądaniu.
    GitHubRelease,
    /// Repozytorium/plik Hugging Face, pobierane tylko po jawnym żądaniu.
    HuggingFace,
}

impl ModelSourceKind {
    pub const ALL: [ModelSourceKind; 4] = [
        ModelSourceKind::Curated,
        ModelSourceKind::Local,
        ModelSourceKind::GitHubRelease,
        ModelSourceKind::HuggingFace,
    ];

    pub fn as_str(self) -> &'static str {
        match self {
            ModelSourceKind::Curated => "curated",
            ModelSourceKind::Local => "local",
            ModelSourceKind::GitHubRelease => "github_release",
            ModelSourceKind::HuggingFace => "hugging_face",
        }
    }

    pub fn parse(value: &str) -> Option<Self> {
        match value.trim().to_lowercase().as_str() {
            "curated" | "preset" | "presets" => Some(ModelSourceKind::Curated),
            "local" | "file" | "directory" => Some(ModelSourceKind::Local),
            "github_release" | "github" | "gh" => Some(ModelSourceKind::GitHubRelease),
            "hugging_face" | "huggingface" | "hf" => Some(ModelSourceKind::HuggingFace),
            _ => None,
        }
    }
}

/// Opis jednego sposobu dodawania modeli.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ModelSourceInfo {
    pub kind: ModelSourceKind,
    pub name: String,
    pub detail: String,
    /// Czy ta ścieżka może otworzyć połączenie sieciowe.
    pub network: bool,
    /// Czy wymaga jawnego potwierdzenia użytkownika przed pobraniem.
    pub requires_explicit_consent: bool,
}

/// Cztery źródła dodawania modeli, w kolejności prezentacji w UI.
pub fn list_sources() -> Vec<ModelSourceInfo> {
    vec![
        ModelSourceInfo {
            kind: ModelSourceKind::Curated,
            name: "Polecane presety".to_string(),
            detail:
                "Modele znane z Portable Local Studio (Piper PL/EN, whisper.cpp, lokalny LLM). \
                 Lista jest offline; instalacja wymaga jawnego żądania i sieci."
                    .to_string(),
            network: false,
            requires_explicit_consent: true,
        },
        ModelSourceInfo {
            kind: ModelSourceKind::Local,
            name: "Import lokalny".to_string(),
            detail:
                "Plik lub katalog z modelem (Piper, Whisper, LLM, voices). Walidacja formatu, \
                 metadanych, licencji i checksum; pliki z modelu nigdy nie są uruchamiane."
                    .to_string(),
            network: false,
            requires_explicit_consent: false,
        },
        ModelSourceInfo {
            kind: ModelSourceKind::GitHubRelease,
            name: "GitHub Releases".to_string(),
            detail:
                "Konkretny asset wydania, tylko po kliknięciu użytkownika. Przed pobraniem \
                 pokazywane są autor, licencja, rozmiar i format; SHA-256 jest wymagany."
                    .to_string(),
            network: true,
            requires_explicit_consent: true,
        },
        ModelSourceInfo {
            kind: ModelSourceKind::HuggingFace,
            name: "Hugging Face".to_string(),
            detail:
                "Repozytorium lub pojedynczy plik (repo/file), tylko po jawnym żądaniu. Bez \
                 auto-downloadu i bez pobierania w tle."
                    .to_string(),
            network: true,
            requires_explicit_consent: true,
        },
    ]
}

/// Rola pliku w paczce modelu.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PresetFileRole {
    Weights,
    Config,
    Checksum,
}

impl PresetFileRole {
    pub fn as_str(self) -> &'static str {
        match self {
            PresetFileRole::Weights => "weights",
            PresetFileRole::Config => "config",
            PresetFileRole::Checksum => "checksum",
        }
    }
}

/// Jeden plik presetu: co, skąd i pod jaką nazwą ma wylądować.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PresetFile {
    pub role: PresetFileRole,
    /// Nazwa docelowa w katalogu rodzaju (bez ścieżek).
    pub file_name: String,
    pub url: String,
    /// Oczekiwany SHA-256, gdy producent go publikuje. Puste = nieznany.
    pub sha256: Option<String>,
    /// Deklarowany rozmiar (0 = nieznany).
    pub size_bytes: u64,
}

/// Preset kurowany: model, który znamy z Portable Local Studio.
#[derive(Debug, Clone, PartialEq)]
pub struct CuratedPreset {
    /// Stabilne id katalogu, np. `piper/pl_PL-darkman-medium`.
    pub id: String,
    pub kind: ModelKind,
    pub name: String,
    pub author: String,
    pub license: String,
    pub language: Option<String>,
    pub format: ModelFormat,
    /// Rozmiar całej paczki (przybliżony, informacyjny).
    pub size_bytes: u64,
    pub source: ModelSourceKind,
    /// `owner/repo@revision` — do pokazania przed pobraniem.
    pub source_reference: String,
    /// Skąd preset pochodzi (Portable Local Studio / dokumentacja).
    pub origin: String,
    pub description: String,
    pub files: Vec<PresetFile>,
}

impl CuratedPreset {
    pub fn weights_file(&self) -> Option<&PresetFile> {
        self.files
            .iter()
            .find(|f| f.role == PresetFileRole::Weights)
    }

    pub fn file_names(&self) -> Vec<String> {
        self.files.iter().map(|f| f.file_name.clone()).collect()
    }

    /// Pobranie presetu zawsze wymaga jawnej zgody — nie ma trybu cichego.
    pub fn requires_explicit_consent(&self) -> bool {
        true
    }
}

fn piper_preset(stem: &str, name: &str, license: &str, description: &str) -> CuratedPreset {
    let lower = stem.to_lowercase();
    let parts: Vec<&str> = lower.split('-').collect();
    let (locale, voice, quality) = (
        parts.first().copied().unwrap_or("pl_pl"),
        parts.get(1).copied().unwrap_or("voice"),
        parts.get(2).copied().unwrap_or("medium"),
    );
    let lang = locale.split('_').next().unwrap_or("pl");
    let path = format!("{lang}/{locale}/{voice}/{quality}/{stem}");
    let base = format!("https://huggingface.co/rhasspy/piper-voices/resolve/v1.0.0/{path}");
    CuratedPreset {
        id: format!("piper/{stem}"),
        kind: ModelKind::PiperTts,
        name: name.to_string(),
        author: "rhasspy / Piper voices".to_string(),
        license: license.to_string(),
        language: Some(locale.to_string()),
        format: ModelFormat::Onnx,
        size_bytes: 63 * MIB,
        source: ModelSourceKind::HuggingFace,
        source_reference: "rhasspy/piper-voices@v1.0.0".to_string(),
        origin: "Portable Local Studio".to_string(),
        description: description.to_string(),
        files: vec![
            PresetFile {
                role: PresetFileRole::Weights,
                file_name: format!("{stem}.onnx"),
                url: format!("{base}.onnx"),
                sha256: None,
                size_bytes: 63 * MIB,
            },
            PresetFile {
                role: PresetFileRole::Config,
                file_name: format!("{stem}.onnx.json"),
                url: format!("{base}.onnx.json"),
                sha256: None,
                size_bytes: 8 * 1024,
            },
        ],
    }
}

fn whisper_preset(stem: &str, name: &str, size_bytes: u64) -> CuratedPreset {
    CuratedPreset {
        id: format!("whisper/{stem}"),
        kind: ModelKind::WhisperAsr,
        name: name.to_string(),
        author: "OpenAI (wagi) / ggml-org (format ggml)".to_string(),
        license: "MIT".to_string(),
        language: None,
        format: ModelFormat::Binary,
        size_bytes,
        source: ModelSourceKind::HuggingFace,
        source_reference: "ggerganov/whisper.cpp@main".to_string(),
        origin: "Portable Local Studio (backend whisper.cpp)".to_string(),
        description: "Model whisper.cpp w formacie ggml; metadane są w środku pliku."
            .to_string(),
        files: vec![PresetFile {
            role: PresetFileRole::Weights,
            file_name: format!("{stem}.bin"),
            url: format!("https://huggingface.co/ggerganov/whisper.cpp/resolve/main/{stem}.bin"),
            sha256: None,
            size_bytes,
        }],
    }
}

/// Presety znane z Portable Local Studio (`setup-piper.sh`, `setup-whisper.sh`,
/// `setup-llama.sh`) plus modele z listy kurowanej w `models.rs`.
///
/// Rozmiary są deklarowane zgrubnie (informacyjne). Nie wymyślamy sum SHA-256:
/// gdy producent ich nie publikuje, liczymy własną sumę po pobraniu i zapisujemy
/// ją jako sidecar, żeby późniejsza weryfikacja miała punkt odniesienia.
pub fn builtin_presets() -> Vec<CuratedPreset> {
    vec![
        piper_preset(
            "pl_PL-gosia-medium",
            "Gosia (polski głos żeński)",
            "CC0-1.0",
            "Podstawowy polski głos żeński używany przez Portable Local Studio.",
        ),
        piper_preset(
            "pl_PL-darkman-medium",
            "Darkman (polski głos męski)",
            "CC0-1.0",
            "Podstawowy polski głos męski używany przez Portable Local Studio.",
        ),
        piper_preset(
            "pl_PL-mc_speech-medium",
            "MC Speech (polski lektor)",
            "CC0-1.0",
            "Dodatkowy polski głos z listy kurowanej.",
        ),
        piper_preset(
            "en_US-lessac-medium",
            "Lessac (English US, męski)",
            "Blizzard non-commercial",
            "Angielski głos z listy kurowanej; licencja ogranicza użycie komercyjne.",
        ),
        piper_preset(
            "en_US-amy-medium",
            "Amy (English US, żeński)",
            "CC0-1.0",
            "Angielski głos z listy kurowanej.",
        ),
        whisper_preset("ggml-base", "Whisper base (ggml)", 148 * MIB),
        whisper_preset("ggml-small", "Whisper small (ggml)", 488 * MIB),
        CuratedPreset {
            id: "llm/qwen2.5-3b-instruct-q4".to_string(),
            kind: ModelKind::LocalLlm,
            name: "Qwen2.5 3B Instruct (Q4_K_M)".to_string(),
            author: "Alibaba Qwen".to_string(),
            license: "Apache-2.0".to_string(),
            language: Some("pl,en".to_string()),
            format: ModelFormat::Gguf,
            size_bytes: 1926 * MIB,
            source: ModelSourceKind::HuggingFace,
            source_reference: "Qwen/Qwen2.5-3B-Instruct-GGUF@main".to_string(),
            origin: "Portable Local Studio (backend llama.cpp)".to_string(),
            description: "Lokalny model instrukcyjny do pracy offline; wymaga llama.cpp."
                .to_string(),
            files: vec![PresetFile {
                role: PresetFileRole::Weights,
                file_name: "qwen2.5-3b-instruct-q4.gguf".to_string(),
                url: "https://huggingface.co/Qwen/Qwen2.5-3B-Instruct-GGUF/resolve/main/qwen2.5-3b-instruct-q4_k_m.gguf".to_string(),
                sha256: None,
                size_bytes: 1926 * MIB,
            }],
        },
    ]
}

/// Preset po id, akceptuje też prefiks `preset:` / `curated:`.
pub fn find_preset(reference: &str) -> Option<CuratedPreset> {
    let cleaned = reference
        .trim()
        .strip_prefix("preset:")
        .or_else(|| reference.trim().strip_prefix("curated:"))
        .unwrap_or(reference.trim());
    builtin_presets().into_iter().find(|p| p.id == cleaned)
}

/// Kandydat do pobrania — to, co UI pokazuje **przed** jakimkolwiek transferem.
#[derive(Debug, Clone, PartialEq)]
pub struct RemoteCandidate {
    pub id: String,
    pub kind: ModelKind,
    pub name: String,
    pub author: String,
    pub license: String,
    pub language: Option<String>,
    pub format: ModelFormat,
    /// Deklarowany rozmiar; 0 = nieznany.
    pub size_bytes: u64,
    pub source: ModelSourceKind,
    pub source_reference: String,
    /// Adres repozytorium/wydania (nie pliku) — do pokazania użytkownikowi.
    pub origin_url: String,
    pub curated: bool,
    /// Czy komplet metadanych pozwala na pobranie bez dodatkowych deklaracji.
    pub downloadable: bool,
    /// Czego brakuje, żeby pobranie było bezpieczne.
    pub blockers: Vec<String>,
    pub files: Vec<PresetFile>,
}

impl RemoteCandidate {
    pub fn weights_file(&self) -> Option<&PresetFile> {
        self.files
            .iter()
            .find(|f| f.role == PresetFileRole::Weights)
    }

    pub fn file_names(&self) -> Vec<String> {
        self.files.iter().map(|f| f.file_name.clone()).collect()
    }
}

fn candidate_from_preset(preset: CuratedPreset) -> RemoteCandidate {
    RemoteCandidate {
        origin_url: format!(
            "https://huggingface.co/{}",
            preset.source_reference.replace('@', "/tree/")
        ),
        id: preset.id,
        kind: preset.kind,
        name: preset.name,
        author: preset.author,
        license: preset.license,
        language: preset.language,
        format: preset.format,
        size_bytes: preset.size_bytes,
        source: preset.source,
        source_reference: preset.source_reference,
        curated: true,
        downloadable: true,
        blockers: Vec::new(),
        files: preset.files,
    }
}

fn invalid_hosts(url: &str) -> bool {
    url.chars()
        .any(|c| c.is_whitespace() || c.is_control() || c == '\\')
        || url.contains("@")
        || url.matches("..").count() > 0
}

/// Dozwolone hosty pobierania. Nic innego nie przejdzie walidacji.
pub fn is_allowed_download_url(url: &str) -> bool {
    const HOSTS: [&str; 6] = [
        "huggingface.co",
        "cdn-lfs.huggingface.co",
        "cdn-lfs-us-1.huggingface.co",
        "github.com",
        "objects.githubusercontent.com",
        "release-assets.githubusercontent.com",
    ];
    if invalid_hosts(url) {
        return false;
    }
    let Some(rest) = url.strip_prefix("https://") else {
        return false;
    };
    let authority = rest.split('/').next().unwrap_or("");
    if authority.is_empty() || authority.contains(':') {
        return false;
    }
    HOSTS.iter().any(|h| authority.eq_ignore_ascii_case(h))
}

fn is_safe_segment(value: &str) -> bool {
    !value.is_empty()
        && value != ".."
        && value != "."
        && !value.starts_with('/')
        && !value.contains('\\')
        && !value.contains('/')
        && !value.chars().any(|c| c.is_control())
        && value
            .chars()
            .all(|c| c.is_ascii_alphanumeric() || matches!(c, '.' | '_' | '-'))
}

fn is_safe_repo(value: &str) -> bool {
    let mut parts = value.split('/');
    match (parts.next(), parts.next(), parts.next()) {
        (Some(owner), Some(name), None) => is_safe_segment(owner) && is_safe_segment(name),
        _ => false,
    }
}

fn is_safe_rel_path(value: &str) -> bool {
    !value.is_empty() && value.split('/').all(is_safe_segment)
}

fn infer_kind_and_format(path: &str) -> Option<(ModelKind, ModelFormat)> {
    let ext = path.rsplit_once('.').map(|(_, ext)| ext.to_lowercase())?;
    match ext.as_str() {
        "gguf" => Some((ModelKind::LocalLlm, ModelFormat::Gguf)),
        "onnx" => Some((ModelKind::PiperTts, ModelFormat::Onnx)),
        "bin" => Some((ModelKind::WhisperAsr, ModelFormat::Binary)),
        "json" => Some((ModelKind::Voice, ModelFormat::Json)),
        _ => None,
    }
}

fn stem_of(path: &str) -> String {
    let file = path.rsplit('/').next().unwrap_or(path);
    match file.rsplit_once('.') {
        Some((stem, _)) => stem.to_string(),
        None => file.to_string(),
    }
}

fn unverified_candidate(
    kind: ModelKind,
    format: ModelFormat,
    name: String,
    author: String,
    source: ModelSourceKind,
    source_reference: String,
    origin_url: String,
    file_name: String,
    url: String,
) -> RemoteCandidate {
    RemoteCandidate {
        id: format!("{}/{}", kind.dir_name(), stem_of(&file_name)),
        kind,
        name,
        author,
        license: String::new(),
        language: None,
        format,
        size_bytes: 0,
        source,
        source_reference,
        origin_url,
        curated: false,
        downloadable: false,
        blockers: vec![
            "brak zadeklarowanego SHA-256 (pobranie wymaga sumy kontrolnej)".to_string(),
            "brak zadeklarowanego rozmiaru pliku".to_string(),
            "brak zadeklarowanej licencji".to_string(),
        ],
        files: vec![PresetFile {
            role: PresetFileRole::Weights,
            file_name,
            url,
            sha256: None,
            size_bytes: 0,
        }],
    }
}

/// `hf:owner/repo@rev#path/to/file.gguf`
fn parse_huggingface(rest: &str) -> Result<RemoteCandidate, SourceError> {
    let (repo_part, path) = rest
        .split_once('#')
        .ok_or_else(|| SourceError::UnsafeReference("HF wymaga formatu owner/repo@rev#plik".into()))?;
    let (repo, revision) = match repo_part.split_once('@') {
        Some((repo, rev)) => (repo, rev),
        None => (repo_part, "main"),
    };
    if !is_safe_repo(repo) {
        return Err(SourceError::UnsafeReference(format!(
            "nieprawidłowe repozytorium HF: {repo}"
        )));
    }
    if !is_safe_segment(revision) || !is_safe_rel_path(path) {
        return Err(SourceError::UnsafeReference(
            "nieprawidłowa rewizja lub ścieżka pliku HF".to_string(),
        ));
    }
    let (kind, format) = infer_kind_and_format(path).ok_or_else(|| {
        SourceError::UnsupportedReference(format!("nieobsługiwane rozszerzenie pliku HF: {path}"))
    })?;
    let file_name = path.rsplit('/').next().unwrap_or(path).to_string();
    let url = format!("https://huggingface.co/{repo}/resolve/{revision}/{path}");
    Ok(unverified_candidate(
        kind,
        format,
        stem_of(path),
        repo.split('/').next().unwrap_or(repo).to_string(),
        ModelSourceKind::HuggingFace,
        format!("{repo}@{revision}"),
        format!("https://huggingface.co/{repo}"),
        file_name,
        url,
    ))
}

/// `gh:owner/repo@tag#asset-name.gguf`
fn parse_github(rest: &str) -> Result<RemoteCandidate, SourceError> {
    let (repo_part, asset) = rest
        .split_once('#')
        .ok_or_else(|| SourceError::UnsafeReference("GH wymaga formatu owner/repo@tag#asset".into()))?;
    let (repo, tag) = repo_part
        .split_once('@')
        .ok_or_else(|| SourceError::UnsafeReference("GH wymaga tagu wydania (@tag)".into()))?;
    if !is_safe_repo(repo) || !is_safe_segment(tag) || !is_safe_segment(asset) {
        return Err(SourceError::UnsafeReference(
            "nieprawidłowe repozytorium, tag lub nazwa assetu GitHub".to_string(),
        ));
    }
    let (kind, format) = infer_kind_and_format(asset).ok_or_else(|| {
        SourceError::UnsupportedReference(format!(
            "nieobsługiwane rozszerzenie assetu GitHub: {asset}"
        ))
    })?;
    let url = format!("https://github.com/{repo}/releases/download/{tag}/{asset}");
    Ok(unverified_candidate(
        kind,
        format,
        stem_of(asset),
        repo.split('/').next().unwrap_or(repo).to_string(),
        ModelSourceKind::GitHubRelease,
        format!("{repo}@{tag}"),
        format!("https://github.com/{repo}/releases/tag/{tag}"),
        asset.to_string(),
        url,
    ))
}

/// Rozpoznaje odwołanie do kandydata zdalnego **bez sieci**.
///
/// Obsługiwane formy:
/// * `preset:<id>` lub samo `<id>` presetu — pełne metadane, gotowe do pobrania,
/// * `hf:owner/repo@rev#plik` — metadane niepełne (brak sumy/rozmiaru/licencji),
/// * `gh:owner/repo@tag#asset` — jak wyżej,
/// * pełny `https://` na dozwolonym hoście — jak wyżej.
pub fn describe_remote_reference(reference: &str) -> Result<RemoteCandidate, SourceError> {
    let trimmed = reference.trim();
    if trimmed.is_empty() {
        return Err(SourceError::UnknownReference("puste odwołanie".to_string()));
    }
    if let Some(preset) = find_preset(trimmed) {
        return Ok(candidate_from_preset(preset));
    }
    if let Some(rest) = trimmed.strip_prefix("hf:") {
        return parse_huggingface(rest);
    }
    if let Some(rest) = trimmed
        .strip_prefix("gh:")
        .or_else(|| trimmed.strip_prefix("github:"))
    {
        return parse_github(rest);
    }
    if trimmed.starts_with("https://") {
        if !is_allowed_download_url(trimmed) {
            return Err(SourceError::UnsafeReference(format!(
                "host nie jest na liście dozwolonych: {trimmed}"
            )));
        }
        let (kind, format) = infer_kind_and_format(trimmed)
            .ok_or_else(|| SourceError::UnsupportedReference(trimmed.to_string()))?;
        let file_name = trimmed
            .split('?')
            .next()
            .unwrap_or(trimmed)
            .rsplit('/')
            .next()
            .unwrap_or("model")
            .to_string();
        return Ok(unverified_candidate(
            kind,
            format,
            stem_of(&file_name),
            String::new(),
            ModelSourceKind::HuggingFace,
            String::new(),
            trimmed.to_string(),
            file_name,
            trimmed.to_string(),
        ));
    }
    Err(SourceError::UnknownReference(trimmed.to_string()))
}

#[derive(Debug, Clone, PartialEq, Eq, thiserror::Error)]
pub enum SourceError {
    #[error("nieznane odwołanie do źródła: {0}")]
    UnknownReference(String),
    #[error("niebezpieczne odwołanie do źródła: {0}")]
    UnsafeReference(String),
    #[error("nieobsługiwane źródło: {0}")]
    UnsupportedReference(String),
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sources_describe_four_ways_without_network_for_curated_and_local() {
        let sources = list_sources();
        assert_eq!(sources.len(), 4);
        assert_eq!(
            sources.iter().map(|s| s.kind).collect::<Vec<_>>(),
            ModelSourceKind::ALL.to_vec()
        );
        let curated = sources
            .iter()
            .find(|s| s.kind == ModelSourceKind::Curated)
            .unwrap();
        assert!(!curated.network);
        assert!(curated.requires_explicit_consent);
        let local = sources
            .iter()
            .find(|s| s.kind == ModelSourceKind::Local)
            .unwrap();
        assert!(!local.network);
        assert!(!local.requires_explicit_consent);
        for kind in [
            ModelSourceKind::GitHubRelease,
            ModelSourceKind::HuggingFace,
        ] {
            let source = sources.iter().find(|s| s.kind == kind).unwrap();
            assert!(source.network);
            assert!(source.requires_explicit_consent);
        }
    }

    #[test]
    fn presets_keep_the_models_known_from_portable_local_studio() {
        let presets = builtin_presets();
        let ids: Vec<&str> = presets.iter().map(|p| p.id.as_str()).collect();
        for expected in [
            "piper/pl_PL-gosia-medium",
            "piper/pl_PL-darkman-medium",
            "piper/pl_PL-mc_speech-medium",
            "piper/en_US-lessac-medium",
            "piper/en_US-amy-medium",
            "whisper/ggml-base",
            "whisper/ggml-small",
            "llm/qwen2.5-3b-instruct-q4",
        ] {
            assert!(ids.contains(&expected), "brak presetu {expected}");
        }
        assert!(presets.iter().all(|p| p.requires_explicit_consent()));
        assert!(presets.iter().all(|p| !p.author.is_empty()));
        assert!(presets.iter().all(|p| p.size_bytes > 0));
        assert!(presets.iter().all(|p| !p.files.is_empty()));
        assert!(presets
            .iter()
            .all(|p| p.files.iter().all(|f| is_allowed_download_url(&f.url))));
        // Id presetu musi być stabilnym id katalogu (rodzaj/nazwa pliku wag).
        for preset in &presets {
            let weights = preset.weights_file().expect("preset bez wag");
            let stem = stem_of(&weights.file_name);
            assert_eq!(preset.id, format!("{}/{stem}", preset.kind.dir_name()));
        }
    }

    #[test]
    fn preset_preview_has_everything_needed_before_a_download() {
        let candidate = describe_remote_reference("preset:piper/pl_PL-darkman-medium").unwrap();
        assert!(candidate.curated);
        assert!(candidate.downloadable);
        assert!(candidate.blockers.is_empty());
        assert_eq!(candidate.source, ModelSourceKind::HuggingFace);
        assert_eq!(candidate.author, "rhasspy / Piper voices");
        assert_eq!(candidate.license, "CC0-1.0");
        assert_eq!(candidate.format, ModelFormat::Onnx);
        assert_eq!(candidate.kind, ModelKind::PiperTts);
        assert!(candidate.size_bytes > 0);
        assert_eq!(candidate.files.len(), 2);
        assert_eq!(candidate.file_names(), vec![
            "pl_PL-darkman-medium.onnx",
            "pl_PL-darkman-medium.onnx.json"
        ]);
        // Samo id też działa (UI może trzymać id bez prefiksu).
        assert_eq!(describe_remote_reference("piper/pl_PL-darkman-medium").unwrap(), candidate);
    }

    #[test]
    fn uncurated_references_are_previewed_but_not_downloadable_without_checksum() {
        let hf = describe_remote_reference("hf:Qwen/Qwen2.5-3B-Instruct-GGUF@main#x/y.gguf")
            .unwrap();
        assert!(!hf.curated);
        assert!(!hf.downloadable);
        assert_eq!(hf.kind, ModelKind::LocalLlm);
        assert_eq!(hf.format, ModelFormat::Gguf);
        assert!(hf.blockers.iter().any(|b| b.contains("SHA-256")));
        assert!(is_allowed_download_url(&hf.files[0].url));

        let gh = describe_remote_reference("gh:some-org/models@v1#voice.bin").unwrap();
        assert_eq!(gh.source, ModelSourceKind::GitHubRelease);
        assert_eq!(gh.kind, ModelKind::WhisperAsr);
        assert!(gh.files[0]
            .url
            .starts_with("https://github.com/some-org/models/releases/download/v1/"));
        assert!(!gh.downloadable);
    }

    #[test]
    fn unsafe_or_unknown_references_are_rejected() {
        for bad in [
            "hf:../etc@main#passwd.onnx",
            "hf:owner/repo@main#../../evil.gguf",
            "hf:owner#file.gguf",
            "gh:owner/repo#asset.bin",
            "gh:owner/repo@../x#asset.bin",
            "http://huggingface.co/x.gguf",
            "https://evil.example.com/x.gguf",
            "https://huggingface.co/x.exe",
            "ftp://huggingface.co/x.gguf",
            "",
        ] {
            assert!(
                describe_remote_reference(bad).is_err(),
                "odwołanie '{bad}' musi zostać odrzucone"
            );
        }
    }

    #[test]
    fn download_hosts_are_allowlisted_and_https_only() {
        assert!(is_allowed_download_url(
            "https://huggingface.co/rhasspy/piper-voices/resolve/v1.0.0/x.onnx"
        ));
        assert!(is_allowed_download_url(
            "https://github.com/ggml-org/whisper.cpp/releases/download/v1.9.1/x.tar.gz"
        ));
        for bad in [
            "http://huggingface.co/x",
            "https://huggingface.co.evil.com/x",
            "https://user@huggingface.co/x",
            "https://huggingface.co:8443/x",
            "https://cdn.example.com/x",
            "file:///etc/passwd",
        ] {
            assert!(!is_allowed_download_url(bad), "host '{bad}' musi być odrzucony");
        }
    }

    #[test]
    fn module_has_no_network_client() {
        let source = include_str!("model_sources.rs");
        let production = source.split("#[cfg(test)]").next().unwrap_or(source);
        for forbidden in ["reqwest", "ureq", "TcpStream", "Command::new", "std::net"] {
            assert!(
                !production.contains(forbidden),
                "moduł źródeł nie może zawierać '{forbidden}'"
            );
        }
    }
}
