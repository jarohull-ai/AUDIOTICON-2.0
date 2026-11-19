#include "StudioBridge.h"

#include "voice/DirectiveParser.h"
#include "voice/PolishVerbalizer.h"

#include <QDebug>

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
    return QVariantMap{
        {QStringLiteral("catalogRoot"), status.catalogRoot},
        {QStringLiteral("rootExists"), status.rootExists},
        {QStringLiteral("total"), status.total},
        {QStringLiteral("available"), status.available},
        {QStringLiteral("unavailable"), status.unavailable},
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
