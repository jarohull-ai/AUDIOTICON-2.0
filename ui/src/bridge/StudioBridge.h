#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>

#include <atomic>
#include <memory>
#include <thread>

#include "AudioticonCore.h"
#include "AudioPlayer.h"

// Qt façade over the Rust render job runtime and audio playback.
//
// The GUI thread never waits for Rust or Audio I/O: startRender() only registers
// a job in the Rust registry and returns, while AudioPlayer handles non-blocking
// stream decoding and playback via QAudioSink.
class StudioBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString taskStatus READ taskStatus NOTIFY taskStatusChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY taskStatusChanged)
    Q_PROPERTY(double taskPercent READ taskPercent NOTIFY taskStatusChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString outputPath READ outputPath NOTIFY taskStatusChanged)
    Q_PROPERTY(quint64 jobId READ jobId NOTIFY taskStatusChanged)
    Q_PROPERTY(QVariantList segments READ segments NOTIFY segmentsChanged)
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged)

    // Odtwarzacz audio (M8)
    Q_PROPERTY(AudioPlayer* player READ player CONSTANT)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    Q_PROPERTY(bool isPaused READ isPaused NOTIFY isPausedChanged)
    Q_PROPERTY(qint64 playbackPosition READ playbackPosition NOTIFY playbackPositionChanged)
    Q_PROPERTY(qint64 playbackDuration READ playbackDuration NOTIFY playbackDurationChanged)
    Q_PROPERTY(QString playbackError READ playbackError NOTIFY playbackErrorChanged)

    // M17: lokalny katalog modeli (offline, bez pobierania)
    Q_PROPERTY(QString activeModelId READ activeModelId NOTIFY modelSelectionChanged)
    Q_PROPERTY(QString modelError READ modelError NOTIFY modelSelectionChanged)

    // M19: stan importu / stagingu / weryfikacji w UI Model Hub
    Q_PROPERTY(bool importBusy READ importBusy NOTIFY importStateChanged)
    Q_PROPERTY(QString importStage READ importStage NOTIFY importStateChanged)
    Q_PROPERTY(QString importMessage READ importMessage NOTIFY importStateChanged)
    Q_PROPERTY(QString importError READ importError NOTIFY importStateChanged)
    Q_PROPERTY(quint64 currentImportId READ currentImportId NOTIFY importStateChanged)

public:
    explicit StudioBridge(QObject *parent = nullptr);
    ~StudioBridge() override;

    QString taskStatus() const { return m_taskStatus; }
    QString statusMessage() const { return m_statusMessage; }
    double taskPercent() const { return m_taskPercent; }
    QString errorMessage() const { return m_errorMessage; }
    QString outputPath() const { return m_outputPath; }
    quint64 jobId() const { return m_jobId; }
    QVariantList segments() const { return m_segments; }
    bool isBusy() const { return m_isBusy; }

    AudioPlayer *player() const { return m_player; }
    bool isPlaying() const;
    bool isPaused() const;
    qint64 playbackPosition() const;
    qint64 playbackDuration() const;
    QString playbackError() const;

    // Expose'owane do QML
    Q_INVOKABLE QVariantMap parseDirectedText(const QString &rawText);
    Q_INVOKABLE quint64 startRender(const QString &rawText, const QString &outputDir = QString());
    Q_INVOKABLE void cancelRender();
    Q_INVOKABLE void resetStatus();

    // Akcje odtwarzacza (M8)
    Q_INVOKABLE bool playFile(const QString &filePath = QString());
    Q_INVOKABLE bool playOutput();
    Q_INVOKABLE void pausePlayback();
    Q_INVOKABLE void resumePlayback();
    Q_INVOKABLE void stopPlayback();
    Q_INVOKABLE void togglePlayback();
    Q_INVOKABLE void seekPlayback(qint64 positionMs);
    Q_INVOKABLE void playSegment(int index);

    // Katalog modeli (M17): list/inspect/select/status/clear na jednym kontrakcie.
    Q_INVOKABLE QVariantList models() const;
    Q_INVOKABLE QVariantMap modelStatus() const;
    Q_INVOKABLE QVariantMap inspectModel(const QString &id) const;
    Q_INVOKABLE bool selectModel(const QString &id);
    Q_INVOKABLE bool clearModelSelection();

    // Dodawanie modeli (M18): źródła, presety, import lokalny, pobranie na żądanie.
    Q_INVOKABLE QVariantList modelSources() const;
    Q_INVOKABLE QVariantList curatedPresets() const;
    Q_INVOKABLE QVariantMap inspectRemoteCandidate(const QString &reference) const;
    Q_INVOKABLE QVariantMap importLocalModel(const QString &path,
                                             const QString &kind = QString(),
                                             bool acceptUnknownLicense = false);
    Q_INVOKABLE QVariantMap downloadRemoteModel(const QString &reference,
                                                const QString &expectedSha256 = QString(),
                                                quint64 expectedSize = 0, bool consent = false);
    Q_INVOKABLE bool cancelImport(quint64 importId);
    Q_INVOKABLE bool verifyModel(const QString &id);
    Q_INVOKABLE bool removeModel(const QString &id);

    // M19: pomocnicze metody UI Model Hub (bez zmiany FFI/backendu)
    Q_INVOKABLE QVariantMap verifyModelDetailed(const QString &id);
    Q_INVOKABLE quint64 activeImportId() const;
    Q_INVOKABLE bool cancelActiveImport();
    Q_INVOKABLE void startImportLocal(const QString &path,
                                      const QString &kind = QString(),
                                      bool acceptUnknownLicense = false);
    Q_INVOKABLE void startDownloadRemote(const QString &reference,
                                         const QString &expectedSha256 = QString(),
                                         quint64 expectedSize = 0,
                                         bool consent = false);
    Q_INVOKABLE void resetImportState();
    Q_INVOKABLE QString urlToLocalPath(const QUrl &url) const;

    QString activeModelId() const;
    QString modelError() const { return m_modelError; }
    bool importBusy() const { return m_importBusy; }
    QString importStage() const { return m_importStage; }
    QString importMessage() const { return m_importMessage; }
    QString importError() const { return m_importError; }
    quint64 currentImportId() const { return m_currentImportId; }

    // Entry points for a worker-side callback (BridgeObserver, M3 seam). The M5
    // flow does not use them: it polls the Rust registry instead.
    void updateProgressFromWorker(int step, int total, double percent, const QString &status);
    void setErrorFromWorker(const QString &error);

signals:
    void taskStatusChanged();
    void errorMessageChanged();
    void segmentsChanged();
    void isBusyChanged();
    void progressUpdated(int step, int total, double percent, const QString &status);
    void renderFinished(bool ok);

    // Sygnały odtwarzacza (M8)
    void isPlayingChanged();
    void isPausedChanged();
    void playbackPositionChanged(qint64 positionMs);
    void playbackDurationChanged(qint64 durationMs);
    void playbackErrorChanged();
    void playbackFinished();

    // Wybór modelu zmienił się (albo próba wyboru zwróciła błąd).
    void modelSelectionChanged();

    // Zakończony import/pobranie: `completed`, `cancelled` albo `failed`.
    void modelImportFinished(const QString &status);

    // M19: zmiana stanu importu/stagingu/weryfikacji
    void importStateChanged();

private slots:
    void pollJob();

private:
    void joinImportThread();

    QString m_taskStatus = QStringLiteral("Idle");
    QString m_statusMessage;
    double m_taskPercent = 0.0;
    QString m_errorMessage;
    QString m_outputPath;
    QString m_plainText;
    QString m_modelError;
    QVariantList m_segments;
    bool m_isBusy = false;
    quint64 m_jobId = 0;

    bool m_importBusy = false;
    QString m_importStage;
    QString m_importMessage;
    QString m_importError;
    quint64 m_currentImportId = 0;
    std::atomic<bool> m_importCancelRequested{false};
    std::thread m_importThread;

    AudioPlayer *m_player = nullptr;
    bool m_autoPlayOnFinish = false;

    std::unique_ptr<audioticon::ui::CoreSession> m_session;
    QTimer m_pollTimer;
};
