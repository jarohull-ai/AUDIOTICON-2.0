#include "StudioBridge.h"

#include "voice/DirectiveParser.h"
#include "voice/PolishVerbalizer.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>

namespace {
// Polling, not push: the Rust registry answers from memory, so a tick costs a
// mutex lock and a struct copy. 50 ms keeps the bar smooth and the GUI free.
constexpr int kPollIntervalMs = 50;

QVariantMap modelToVariant(const audioticon::ui::ModelInfo &model)
{
    return QVariantMap{
        {QStringLiteral("id"), model.id},
        {QStringLiteral("kind"), model.kind},
        {QStringLiteral("name"), model.name},
        {QStringLiteral("path"), model.path},
        {QStringLiteral("format"), model.format},
        {QStringLiteral("language"), model.language},
        {QStringLiteral("sizeBytes"), model.sizeBytes},
        {QStringLiteral("license"), model.license},
        {QStringLiteral("status"), model.status},
        {QStringLiteral("checksumSha256"), model.checksumSha256},
        {QStringLiteral("detail"), model.detail},
        {QStringLiteral("available"), model.isAvailable()},
    };
}
} // namespace

StudioBridge::StudioBridge(QObject *parent)
    : QObject(parent),
      m_player(new AudioPlayer(this)),
      m_session(std::make_unique<audioticon::ui::CoreSession>())
{
    m_pollTimer.setInterval(kPollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &StudioBridge::pollJob);

    // Forwarding sygnałów odtwarzacza AudioPlayer (M8)
    connect(m_player, &AudioPlayer::playingChanged, this, &StudioBridge::isPlayingChanged);
    connect(m_player, &AudioPlayer::pausedChanged, this, &StudioBridge::isPausedChanged);
    connect(m_player, &AudioPlayer::positionChanged, this, &StudioBridge::playbackPositionChanged);
    connect(m_player, &AudioPlayer::durationChanged, this, &StudioBridge::playbackDurationChanged);
    connect(m_player, &AudioPlayer::errorMessageChanged, this, &StudioBridge::playbackErrorChanged);
    connect(m_player, &AudioPlayer::playbackFinished, this, &StudioBridge::playbackFinished);
}

StudioBridge::~StudioBridge()
{
    m_pollTimer.stop();
    m_importCancelRequested.store(true);
    if (m_session) {
        const quint64 activeId = m_session->activeImportId();
        if (activeId > 0)
            m_session->cancelImport(activeId);
    }
    joinImportThread();
}

void StudioBridge::joinImportThread()
{
    if (m_importThread.joinable())
        m_importThread.join();
}

bool StudioBridge::isPlaying() const
{
    return m_player ? m_player->isPlaying() : false;
}

bool StudioBridge::isPaused() const
{
    return m_player ? m_player->isPaused() : false;
}

qint64 StudioBridge::playbackPosition() const
{
    return m_player ? m_player->position() : 0;
}

qint64 StudioBridge::playbackDuration() const
{
    return m_player ? m_player->duration() : 0;
}

QString StudioBridge::playbackError() const
{
    return m_player ? m_player->errorMessage() : QString();
}

bool StudioBridge::playFile(const QString &filePath)
{
    if (!m_player)
        return false;
    return m_player->play(filePath);
}

bool StudioBridge::playOutput()
{
    if (m_outputPath.isEmpty()) {
        setErrorFromWorker(QStringLiteral("Brak wyrenderowanego pliku audio do odtworzenia"));
        return false;
    }
    return playFile(m_outputPath);
}

void StudioBridge::pausePlayback()
{
    if (m_player)
        m_player->pause();
}

void StudioBridge::resumePlayback()
{
    if (m_player)
        m_player->resume();
}

void StudioBridge::stopPlayback()
{
    if (m_player)
        m_player->stop();
}

void StudioBridge::togglePlayback()
{
    if (!m_player)
        return;

    if (m_player->isPlaying()) {
        m_player->pause();
    } else if (m_player->isPaused()) {
        m_player->resume();
    } else if (!m_outputPath.isEmpty()) {
        playOutput();
    } else {
        if (!m_plainText.isEmpty()) {
            m_autoPlayOnFinish = true;
            startRender(m_plainText);
        }
    }
}

void StudioBridge::seekPlayback(qint64 positionMs)
{
    if (m_player)
        m_player->seek(positionMs);
}

void StudioBridge::playSegment(int index)
{
    if (index < 0 || index >= m_segments.size())
        return;

    const QVariantMap seg = m_segments.at(index).toMap();
    const QString segText = seg.value(QStringLiteral("text")).toString();
    if (segText.isEmpty())
        return;

    m_autoPlayOnFinish = true;
    startRender(segText);
}

namespace {

QVariantMap presetFileToVariant(const audioticon::ui::ModelPresetFile &file)
{
    return QVariantMap{
        {QStringLiteral("role"), file.role},
        {QStringLiteral("fileName"), file.fileName},
        {QStringLiteral("url"), file.url},
        {QStringLiteral("sha256"), file.sha256},
        {QStringLiteral("sizeBytes"), file.sizeBytes},
    };
}

QVariantList presetFilesToVariant(const QVector<audioticon::ui::ModelPresetFile> &files)
{
    QVariantList list;
    list.reserve(files.size());
    for (const auto &file : files)
        list.append(presetFileToVariant(file));
    return list;
}

} // namespace

QVariantList StudioBridge::modelSources() const
{
    QVariantList list;
    if (!m_session)
        return list;
    for (const auto &source : m_session->listSources()) {
        list.append(QVariantMap{
            {QStringLiteral("kind"), source.kind},
            {QStringLiteral("name"), source.name},
            {QStringLiteral("detail"), source.detail},
            {QStringLiteral("network"), source.network},
            {QStringLiteral("requiresExplicitConsent"), source.requiresExplicitConsent},
        });
    }
    return list;
}

QVariantList StudioBridge::curatedPresets() const
{
    QVariantList list;
    if (!m_session)
        return list;
    for (const auto &preset : m_session->listCuratedPresets()) {
        list.append(QVariantMap{
            {QStringLiteral("id"), preset.id},
            {QStringLiteral("kind"), preset.kind},
            {QStringLiteral("name"), preset.name},
            {QStringLiteral("author"), preset.author},
            {QStringLiteral("license"), preset.license},
            {QStringLiteral("language"), preset.language},
            {QStringLiteral("format"), preset.format},
            {QStringLiteral("sizeBytes"), preset.sizeBytes},
            {QStringLiteral("source"), preset.source},
            {QStringLiteral("sourceReference"), preset.sourceReference},
            {QStringLiteral("origin"), preset.origin},
            {QStringLiteral("description"), preset.description},
            {QStringLiteral("files"), presetFilesToVariant(preset.files)},
            {QStringLiteral("installed"), preset.installed},
            {QStringLiteral("installedStatus"), preset.installedStatus},
        });
    }
    return list;
}

QVariantMap StudioBridge::inspectRemoteCandidate(const QString &reference) const
{
    if (!m_session)
        return QVariantMap{};

    QString error;
    const std::optional<audioticon::ui::RemoteCandidate> candidate =
        m_session->inspectRemoteCandidate(reference, &error);
    if (!candidate)
        return QVariantMap{{QStringLiteral("error"), error.isEmpty()
                                                       ? QStringLiteral("Nie można odczytać źródła '%1'").arg(reference)
                                                       : error}};

    QVariantList blockers;
    for (const auto &blocker : candidate->blockers)
        blockers.append(blocker);

    return QVariantMap{
        {QStringLiteral("id"), candidate->id},
        {QStringLiteral("kind"), candidate->kind},
        {QStringLiteral("name"), candidate->name},
        {QStringLiteral("author"), candidate->author},
        {QStringLiteral("license"), candidate->license},
        {QStringLiteral("language"), candidate->language},
        {QStringLiteral("format"), candidate->format},
        {QStringLiteral("sizeBytes"), candidate->sizeBytes},
        {QStringLiteral("source"), candidate->source},
        {QStringLiteral("sourceReference"), candidate->sourceReference},
        {QStringLiteral("originUrl"), candidate->originUrl},
        {QStringLiteral("curated"), candidate->curated},
        {QStringLiteral("downloadable"), candidate->downloadable},
        {QStringLiteral("blockers"), blockers},
        {QStringLiteral("files"), presetFilesToVariant(candidate->files)},
    };
}

QVariantMap StudioBridge::importLocalModel(const QString &path, const QString &kind,
                                           bool acceptUnknownLicense)
{
    if (!m_session)
        return QVariantMap{{QStringLiteral("error"), QStringLiteral("Silnik Rust nie został zainicjalizowany")}};

    QString error;
    const std::optional<audioticon::ui::ModelImportResult> result =
        m_session->importLocal(path, kind, acceptUnknownLicense, &error);
    if (!result) {
        m_modelError = error.isEmpty()
                           ? QStringLiteral("Nie można zaimportować modelu '%1'").arg(path)
                           : error;
        emit modelSelectionChanged();
        emit modelImportFinished(QStringLiteral("failed"));
        return QVariantMap{{QStringLiteral("status"), QStringLiteral("failed")},
                           {QStringLiteral("error"), m_modelError}};
    }

    m_modelError.clear();
    emit modelSelectionChanged();
    emit modelImportFinished(result->status);
    return QVariantMap{
        {QStringLiteral("status"), result->status},
        {QStringLiteral("modelId"), result->modelId},
        {QStringLiteral("modelPath"), result->modelPath},
        {QStringLiteral("modelStatus"), result->modelStatus},
        {QStringLiteral("message"), result->message},
        {QStringLiteral("source"), result->source},
    };
}

QVariantMap StudioBridge::downloadRemoteModel(const QString &reference,
                                              const QString &expectedSha256,
                                              quint64 expectedSize, bool consent)
{
    if (!m_session)
        return QVariantMap{{QStringLiteral("error"), QStringLiteral("Silnik Rust nie został zainicjalizowany")}};

    QString error;
    const std::optional<audioticon::ui::ModelImportResult> result =
        m_session->downloadRemote(reference, expectedSha256, expectedSize, consent, &error);
    if (!result) {
        m_modelError = error.isEmpty()
                           ? QStringLiteral("Nie można pobrać '%1'").arg(reference)
                           : error;
        emit modelSelectionChanged();
        emit modelImportFinished(QStringLiteral("failed"));
        return QVariantMap{{QStringLiteral("status"), QStringLiteral("failed")},
                           {QStringLiteral("error"), m_modelError}};
    }

    m_modelError.clear();
    emit modelSelectionChanged();
    emit modelImportFinished(result->status);
    return QVariantMap{
        {QStringLiteral("status"), result->status},
        {QStringLiteral("modelId"), result->modelId},
        {QStringLiteral("modelPath"), result->modelPath},
        {QStringLiteral("modelStatus"), result->modelStatus},
        {QStringLiteral("message"), result->message},
        {QStringLiteral("source"), result->source},
    };
}

bool StudioBridge::cancelImport(quint64 importId)
{
    return m_session && m_session->cancelImport(importId);
}

bool StudioBridge::verifyModel(const QString &id)
{
    if (!m_session)
        return false;
    QString error;
    const auto result = m_session->verifyModel(id, &error);
    if (!result) {
        m_modelError = error;
        emit modelSelectionChanged();
        return false;
    }
    m_modelError = result->checked && !result->matches ? result->message : QString();
    emit modelSelectionChanged();
    return result->checked ? result->matches : true;
}

bool StudioBridge::removeModel(const QString &id)
{
    if (!m_session)
        return false;
    QString error;
    const auto result = m_session->removeModel(id, &error);
    if (!result) {
        m_modelError = error.isEmpty() ? QStringLiteral("Nie można usunąć modelu '%1'").arg(id)
                                       : error;
        emit modelSelectionChanged();
        return false;
    }
    m_modelError.clear();
    emit modelSelectionChanged();
    return true;
}

QVariantMap StudioBridge::verifyModelDetailed(const QString &id)
{
    if (!m_session) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QCoreApplication::translate("ModelHub", "Rust engine is not initialized")},
        };
    }
    QString error;
    const auto result = m_session->verifyModel(id, &error);
    if (!result) {
        m_modelError = error.isEmpty()
                           ? QCoreApplication::translate("ModelHub", "Failed to verify model '%1'").arg(id)
                           : error;
        emit modelSelectionChanged();
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("id"), id},
            {QStringLiteral("error"), m_modelError},
        };
    }
    const bool ok = !result->checked || result->matches;
    m_modelError = ok ? QString() : result->message;
    emit modelSelectionChanged();
    return QVariantMap{
        {QStringLiteral("ok"), ok},
        {QStringLiteral("id"), result->id},
        {QStringLiteral("kind"), result->kind},
        {QStringLiteral("status"), result->status},
        {QStringLiteral("checked"), result->checked},
        {QStringLiteral("expectedSha256"), result->expectedSha256},
        {QStringLiteral("actualSha256"), result->actualSha256},
        {QStringLiteral("matches"), result->matches},
        {QStringLiteral("sizeBytes"), result->sizeBytes},
        {QStringLiteral("message"), result->message},
        {QStringLiteral("error"), ok ? QString() : result->message},
    };
}

quint64 StudioBridge::activeImportId() const
{
    return m_session ? m_session->activeImportId() : 0;
}

bool StudioBridge::cancelActiveImport()
{
    m_importCancelRequested.store(true);
    bool cancelledInCore = false;
    if (m_session) {
        const quint64 activeId = m_session->activeImportId();
        if (activeId > 0)
            cancelledInCore = m_session->cancelImport(activeId);
        else if (m_currentImportId > 0)
            cancelledInCore = m_session->cancelImport(m_currentImportId);
    }
    if (m_importBusy || cancelledInCore) {
        m_importStage = QStringLiteral("cancelled");
        m_importMessage = QCoreApplication::translate("ModelHub", "Import cancelled — staging cleaned up");
        m_importError.clear();
        emit importStateChanged();
        return true;
    }
    return false;
}

void StudioBridge::startImportLocal(const QString &path, const QString &kind,
                                    bool acceptUnknownLicense)
{
    if (!m_session || m_importBusy)
        return;

    joinImportThread();
    m_importBusy = true;
    m_importStage = QStringLiteral("staging");
    m_importMessage = QCoreApplication::translate("ModelHub", "Staging & validating local files…");
    m_importError.clear();
    m_currentImportId = 0;
    m_importCancelRequested.store(false);
    emit importStateChanged();

    auto *session = m_session.get();
    m_importThread = std::thread([this, session, path, kind, acceptUnknownLicense]() {
        QString error;
        const auto result = session->importLocal(path, kind, acceptUnknownLicense, &error);
        const bool cancelled = m_importCancelRequested.load()
                               || (result.has_value() && result->status == QLatin1String("cancelled"));
        if (cancelled && result.has_value() && !result->modelId.isEmpty()
            && result->status == QLatin1String("completed")) {
            session->removeModel(result->modelId);
        }

        QMetaObject::invokeMethod(this, [this, result, error, cancelled]() {
            m_importBusy = false;
            m_currentImportId = 0;
            if (cancelled) {
                m_importStage = QStringLiteral("cancelled");
                m_importMessage = QCoreApplication::translate("ModelHub", "Import cancelled — staging cleaned up");
                m_importError.clear();
                emit importStateChanged();
                emit modelSelectionChanged();
                emit modelImportFinished(QStringLiteral("cancelled"));
                return;
            }
            if (!result) {
                m_importStage = QStringLiteral("failed");
                m_importError = error.isEmpty()
                                    ? QCoreApplication::translate("ModelHub", "Local model import failed")
                                    : error;
                m_importMessage.clear();
                m_modelError = m_importError;
                emit importStateChanged();
                emit modelSelectionChanged();
                emit modelImportFinished(QStringLiteral("failed"));
                return;
            }
            // Never report success before verification confirms available status.
            if (result->status == QLatin1String("completed")
                && result->modelStatus == QLatin1String("available")) {
                m_importStage = QStringLiteral("completed");
                m_importMessage = result->message;
                m_importError.clear();
                m_modelError.clear();
                emit importStateChanged();
                emit modelSelectionChanged();
                emit modelImportFinished(QStringLiteral("completed"));
            } else {
                m_importStage = QStringLiteral("failed");
                m_importError = result->message.isEmpty()
                                    ? QCoreApplication::translate("ModelHub", "Model verification did not confirm availability")
                                    : result->message;
                m_importMessage.clear();
                m_modelError = m_importError;
                emit importStateChanged();
                emit modelSelectionChanged();
                emit modelImportFinished(QStringLiteral("failed"));
            }
        }, Qt::QueuedConnection);
    });
}

void StudioBridge::startDownloadRemote(const QString &reference,
                                       const QString &expectedSha256,
                                       quint64 expectedSize, bool consent)
{
    if (!m_session || m_importBusy)
        return;

    joinImportThread();
    m_importBusy = true;
    m_importStage = QStringLiteral("staging");
    m_importMessage = QCoreApplication::translate("ModelHub", "Preparing staged download & verification…");
    m_importError.clear();
    m_currentImportId = 0;
    m_importCancelRequested.store(false);
    emit importStateChanged();

    auto *session = m_session.get();
    m_importThread = std::thread([this, session, reference, expectedSha256, expectedSize, consent]() {
        QString error;
        const auto result = session->downloadRemote(reference, expectedSha256, expectedSize, consent, &error);
        const bool cancelled = m_importCancelRequested.load()
                               || (result.has_value() && result->status == QLatin1String("cancelled"));

        QMetaObject::invokeMethod(this, [this, result, error, cancelled]() {
            m_importBusy = false;
            m_currentImportId = 0;
            if (cancelled) {
                m_importStage = QStringLiteral("cancelled");
                m_importMessage = QCoreApplication::translate("ModelHub", "Download cancelled — staging cleaned up");
                m_importError.clear();
                emit importStateChanged();
                emit modelSelectionChanged();
                emit modelImportFinished(QStringLiteral("cancelled"));
                return;
            }
            if (!result) {
                m_importStage = QStringLiteral("failed");
                m_importError = error.isEmpty()
                                    ? QCoreApplication::translate("ModelHub", "Remote download failed")
                                    : error;
                m_importMessage.clear();
                m_modelError = m_importError;
                emit importStateChanged();
                emit modelSelectionChanged();
                emit modelImportFinished(QStringLiteral("failed"));
                return;
            }
            if (result->status == QLatin1String("completed")
                && result->modelStatus == QLatin1String("available")) {
                m_importStage = QStringLiteral("completed");
                m_importMessage = result->message;
                m_importError.clear();
                m_modelError.clear();
                emit importStateChanged();
                emit modelSelectionChanged();
                emit modelImportFinished(QStringLiteral("completed"));
            } else {
                m_importStage = QStringLiteral("failed");
                m_importError = result->message.isEmpty()
                                    ? QCoreApplication::translate("ModelHub", "Downloaded model did not pass verification")
                                    : result->message;
                m_importMessage.clear();
                m_modelError = m_importError;
                emit importStateChanged();
                emit modelSelectionChanged();
                emit modelImportFinished(QStringLiteral("failed"));
            }
        }, Qt::QueuedConnection);
    });
}

void StudioBridge::resetImportState()
{
    if (m_importBusy)
        return;
    m_importStage.clear();
    m_importMessage.clear();
    m_importError.clear();
    m_currentImportId = 0;
    emit importStateChanged();
}

QString StudioBridge::urlToLocalPath(const QUrl &url) const
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

QVariantList StudioBridge::models() const
{
    QVariantList list;
    if (!m_session)
        return list;
    const QVector<audioticon::ui::ModelInfo> found = m_session->listModels();
    list.reserve(found.size());
    for (const auto &model : found)
        list.append(modelToVariant(model));
    return list;
}

QVariantMap StudioBridge::modelStatus() const
{
    if (!m_session)
        return QVariantMap{};

    const audioticon::ui::ModelCatalogStatus status = m_session->modelStatus();
    int incompleteCount = 0;
    for (const auto &model : m_session->listModels()) {
        if (model.status == QLatin1String("incomplete"))
            ++incompleteCount;
    }
    return QVariantMap{
        {QStringLiteral("catalogRoot"), status.catalogRoot},
        {QStringLiteral("rootExists"), status.rootExists},
        {QStringLiteral("total"), status.total},
        {QStringLiteral("available"), status.available},
        {QStringLiteral("unavailable"), status.unavailable},
        {QStringLiteral("incomplete"), incompleteCount},
        {QStringLiteral("activeId"), status.activeId},
        {QStringLiteral("activeKind"), status.activeKind},
        {QStringLiteral("activePath"), status.activePath},
        {QStringLiteral("activeStatus"), status.activeStatus},
        {QStringLiteral("piperModelPath"), status.piperModelPath},
        {QStringLiteral("message"), status.message},
        {QStringLiteral("error"), m_modelError},
    };
}

QVariantMap StudioBridge::inspectModel(const QString &id) const
{
    if (!m_session)
        return QVariantMap{};
    const std::optional<audioticon::ui::ModelInfo> model = m_session->inspectModel(id);
    return model ? modelToVariant(*model) : QVariantMap{};
}

bool StudioBridge::selectModel(const QString &id)
{
    if (!m_session)
        return false;

    QString error;
    const std::optional<audioticon::ui::ModelInfo> model = m_session->selectModel(id, &error);
    if (!model) {
        m_modelError = error.isEmpty()
                           ? QStringLiteral("Nie można wybrać modelu '%1'").arg(id)
                           : error;
        emit modelSelectionChanged();
        return false;
    }

    m_modelError.clear();
    emit modelSelectionChanged();
    return true;
}

bool StudioBridge::clearModelSelection()
{
    if (!m_session)
        return false;

    QString error;
    if (!m_session->clearModelSelection(&error)) {
        m_modelError = error;
        emit modelSelectionChanged();
        return false;
    }

    m_modelError.clear();
    emit modelSelectionChanged();
    return true;
}

QString StudioBridge::activeModelId() const
{
    return m_session ? m_session->modelStatus().activeId : QString();
}

QVariantMap StudioBridge::parseDirectedText(const QString &rawText)
{
    m_plainText.clear();

    if (rawText.trimmed().isEmpty()) {
        m_segments.clear();
        emit segmentsChanged();
        return QVariantMap{
            {QStringLiteral("plain_text"), QString()},
            {QStringLiteral("segments"), QVariantList()},
            {QStringLiteral("chapters"), QVariantList()}
        };
    }

    try {
        // Wykorzystujemy DirectiveParser z bazy E1
        const audioticon::DirectedText parsed = audioticon::parseDirectedText(rawText, true);

        QVariantList segmentList;
        for (int i = 0; i < parsed.segments.size(); ++i) {
            const auto &seg = parsed.segments.at(i);
            QVariantMap item;
            item[QStringLiteral("index")] = i;
            item[QStringLiteral("text")] = seg.text;
            item[QStringLiteral("speed")] = seg.speed;
            item[QStringLiteral("dsp")] = seg.dsp;
            item[QStringLiteral("pauseMs")] = seg.pauseMs;
            segmentList.append(item);
        }

        QVariantList chapterList;
        for (const auto &ch : parsed.chapters) {
            QVariantMap cItem;
            cItem[QStringLiteral("index")] = ch.index;
            cItem[QStringLiteral("title")] = ch.title;
            chapterList.append(cItem);
        }

        m_segments = segmentList;
        // The Rust job only ever receives segments, never the directive text:
        // parsing and verbalization stay on this side (single source of truth).
        m_plainText = parsed.text;
        emit segmentsChanged();

        QVariantMap result;
        result[QStringLiteral("plain_text")] = parsed.text;
        result[QStringLiteral("segments")] = segmentList;
        result[QStringLiteral("chapters")] = chapterList;
        result[QStringLiteral("leadingPauseMs")] = parsed.leadingPauseMs;

        m_errorMessage.clear();
        emit errorMessageChanged();

        return result;
    } catch (const std::exception &e) {
        setErrorFromWorker(QString::fromUtf8(e.what()));
        return QVariantMap{};
    }
}

quint64 StudioBridge::startRender(const QString &rawText, const QString &outputDir)
{
    if (m_isBusy) {
        setErrorFromWorker(QStringLiteral("Renderowanie już trwa - anuluj poprzednie zadanie"));
        return m_jobId;
    }

    // Parse first: it fills `segments` for the UI and gives us the plain text.
    const QVariantMap parsed = parseDirectedText(rawText);
    const QString plainText = parsed.value(QStringLiteral("plain_text")).toString();
    if (plainText.trimmed().isEmpty()) {
        setErrorFromWorker(QStringLiteral("Brak tekstu do syntezy"));
        return 0;
    }

    QString error;
    const quint64 id = m_session->startJob(plainText, outputDir, &error);
    if (id == 0) {
        setErrorFromWorker(error.isEmpty() ? QStringLiteral("Rdzeń Rust odrzucił zadanie")
                                           : error);
        return 0;
    }

    m_jobId = id;
    m_outputPath.clear();
    m_errorMessage.clear();
    m_taskStatus = QStringLiteral("Queued");
    m_statusMessage = QStringLiteral("Zadanie %1 w kolejce rdzenia Rust").arg(id);
    m_taskPercent = 0.0;
    m_isBusy = true;

    emit errorMessageChanged();
    emit taskStatusChanged();
    emit isBusyChanged();

    m_pollTimer.start();
    return id;
}

void StudioBridge::cancelRender()
{
    if (!m_isBusy || m_jobId == 0)
        return;

    m_autoPlayOnFinish = false;
    m_session->cancelJob(m_jobId);
    // Publish the Cancelled state now instead of up to one tick later.
    pollJob();
}

void StudioBridge::pollJob()
{
    if (!m_session || m_jobId == 0)
        return;

    const audioticon::ui::JobSnapshot snapshot = m_session->jobState(m_jobId);
    const QString stage = audioticon::ui::jobStageName(snapshot.stage);
    const bool terminal = stage == QLatin1String("Finished") || stage == QLatin1String("Failed")
                          || stage == QLatin1String("Cancelled");

    bool statusDirty = false;
    if (m_taskStatus != stage) {
        m_taskStatus = stage;
        statusDirty = true;
    }
    if (m_statusMessage != snapshot.statusMessage) {
        m_statusMessage = snapshot.statusMessage;
        statusDirty = true;
    }
    if (!qFuzzyCompare(m_taskPercent + 1.0, snapshot.percent + 1.0)) {
        m_taskPercent = snapshot.percent;
        statusDirty = true;
    }
    if (m_outputPath != snapshot.outputPath) {
        m_outputPath = snapshot.outputPath;
        statusDirty = true;
    }

    if (statusDirty) {
        emit taskStatusChanged();
        emit progressUpdated(snapshot.currentSegment, snapshot.totalSegments, m_taskPercent,
                             m_statusMessage);
    }
    if (m_errorMessage != snapshot.errorMessage) {
        m_errorMessage = snapshot.errorMessage;
        emit errorMessageChanged();
    }

    if (terminal) {
        m_pollTimer.stop();
        if (m_isBusy) {
            m_isBusy = false;
            emit isBusyChanged();
        }
        const bool success = (stage == QLatin1String("Finished"));
        emit renderFinished(success);

        if (m_autoPlayOnFinish) {
            m_autoPlayOnFinish = false;
            if (success && !m_outputPath.isEmpty()) {
                playOutput();
            }
        }
    }
}

void StudioBridge::resetStatus()
{
    m_pollTimer.stop();
    m_jobId = 0;
    m_taskStatus = QStringLiteral("Idle");
    m_statusMessage.clear();
    m_taskPercent = 0.0;
    m_errorMessage.clear();
    m_outputPath.clear();
    m_isBusy = false;
    m_autoPlayOnFinish = false;

    if (m_player) {
        m_player->stop();
    }

    emit taskStatusChanged();
    emit errorMessageChanged();
    emit isBusyChanged();
}

void StudioBridge::updateProgressFromWorker(int step, int total, double percent,
                                            const QString &status)
{
    m_taskPercent = percent;
    m_statusMessage = status;
    emit taskStatusChanged();
    emit progressUpdated(step, total, percent, status);
}

void StudioBridge::setErrorFromWorker(const QString &error)
{
    m_pollTimer.stop();
    m_errorMessage = error;
    m_taskStatus = QStringLiteral("Failed");
    m_isBusy = false;
    m_autoPlayOnFinish = false;

    emit errorMessageChanged();
    emit taskStatusChanged();
    emit isBusyChanged();
}
