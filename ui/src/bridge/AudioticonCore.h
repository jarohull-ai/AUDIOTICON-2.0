#pragma once

#include <QString>
#include <QVector>

#include <memory>
#include <optional>

// The only place in the Qt tree that knows a Rust library exists.
//
// AudioticonCore.cpp includes the cxx-generated header and calls into
// `audioticon::bridge`. Everything else in the UI talks to this header, so the
// cxx types stay out of QML, out of other headers, and out of the test harness.
namespace audioticon::ui {

/// Sample rate reported by the linked audioticon-core crate, in Hz.
///
/// M4 link proof: returns 44100 only if the Rust staticlib is really linked and
/// its code really runs. The value comes from `audioticon_core::DEFAULT_SAMPLE_RATE`.
int coreSampleRate();

/// Mirror of the Rust `TaskStage` enum (backend/crates/bridge/src/lib.rs).
///
/// Kept as a separate C++ enum so no cxx header is ever pulled into a Qt header.
enum class JobStage {
    Idle,
    Queued,
    Normalizing,
    Synthesizing,
    Mastering,
    Finished,
    Failed,
    Cancelled,
};

/// Stable name of a stage, matching the Rust variant ("Queued", "Finished", ...).
QString jobStageName(JobStage stage);

/// Value snapshot of one Rust render job, read with `CoreSession::jobState()`.
struct JobSnapshot {
    quint64 jobId = 0;
    JobStage stage = JobStage::Idle;
    double percent = 0.0;
    int currentSegment = 0;
    int totalSegments = 0;
    QString statusMessage;
    QString errorMessage;
    QString outputPath;
};

/// One locally installed model, mirrored from the Rust catalog (M17).
///
/// `status` is the Rust availability verdict: `available`, `missing_metadata`
/// or `corrupt`. Only `available` models can become the active selection.
struct ModelInfo {
    QString id;
    QString kind;
    QString name;
    QString path;
    QString format;
    QString language;
    qint64 sizeBytes = 0;
    QString license;
    QString status;
    QString checksumSha256;
    QString detail;

    bool isAvailable() const { return status == QLatin1String("available"); }
};

/// Snapshot of the local model catalog plus the persisted selection.
struct ModelCatalogStatus {
    QString catalogRoot;
    bool rootExists = false;
    int total = 0;
    int available = 0;
    int unavailable = 0;
    QString activeId;
    QString activeKind;
    QString activePath;
    QString activeStatus;
    QString piperModelPath;
    QString message;
};

/// Owning handle to the Rust engine: the job registry and the tokio runtime
/// behind it live on the Rust side.
///
/// None of these calls block on rendering. `startJob()` registers work and
/// returns an id; `jobState()` copies a snapshot out of the registry.
class CoreSession {
public:
    CoreSession();
    ~CoreSession();
    CoreSession(const CoreSession &) = delete;
    CoreSession &operator=(const CoreSession &) = delete;

    /// Registers a render job. Returns its id, or 0 on failure with `errorOut`
    /// filled in. An empty `outputDir` lets the Rust side choose its default.
    quint64 startJob(const QString &text, const QString &outputDir = QString(),
                     QString *errorOut = nullptr);

    /// Current state of a job. An unknown id reads as `JobStage::Idle`.
    JobSnapshot jobState(quint64 jobId) const;

    /// Requests cancellation; false if the job is unknown or already finished.
    bool cancelJob(quint64 jobId);

    // ---- M17: local model catalog -----------------------------------------
    // Offline: these only read the XDG model tree and the selection file.

    /// Every model on disk, with its availability status.
    QVector<ModelInfo> listModels() const;

    /// One model by id; empty when the catalog has no such model.
    std::optional<ModelInfo> inspectModel(const QString &id) const;

    /// Persists the selection. Returns empty and fills `errorOut` when the model
    /// is unknown or not available.
    std::optional<ModelInfo> selectModel(const QString &id, QString *errorOut = nullptr);

    /// Catalog root, counters, persisted selection and the wired Piper path.
    ModelCatalogStatus modelStatus() const;

    /// Removes the persisted selection; false with `errorOut` on I/O failure.
    bool clearModelSelection(QString *errorOut = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace audioticon::ui
