#include "AudioPlayer.h"

#include <QFile>
#include <QFileInfo>
#include <QMediaDevices>
#include <QtEndian>
#include <QDebug>
#include <cstring>

namespace {
constexpr int kPositionUpdateIntervalMs = 25;
}

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent)
{
    m_positionTimer.setInterval(kPositionUpdateIntervalMs);
    connect(&m_positionTimer, &QTimer::timeout, this, &AudioPlayer::updatePosition);
}

AudioPlayer::~AudioPlayer()
{
    stop();
    cleanupSink();
}

void AudioPlayer::cleanupSink()
{
    m_positionTimer.stop();
    if (m_audioSink) {
        m_audioSink->stop();
        m_audioSink.reset();
    }
    if (m_buffer.isOpen()) {
        m_buffer.close();
    }
}

void AudioPlayer::setError(const QString &error)
{
    m_errorMessage = error;
    qWarning() << "AudioPlayer error:" << error;
    emit errorMessageChanged();
    emit errorOccurred(error);
}

bool AudioPlayer::parseWav(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(QStringLiteral("Nie można otworzyć pliku: %1").arg(filePath));
        return false;
    }

    const qint64 fileSize = file.size();
    if (fileSize < 44) {
        setError(QStringLiteral("Plik jest za krótki na poprawny format WAV: %1 B").arg(fileSize));
        return false;
    }

    char riffHeader[12];
    if (file.read(riffHeader, 12) != 12) {
        setError(QStringLiteral("Błąd odczytu nagłówka RIFF"));
        return false;
    }

    if (std::memcmp(riffHeader, "RIFF", 4) != 0 || std::memcmp(riffHeader + 8, "WAVE", 4) != 0) {
        setError(QStringLiteral("Nieprawidłowy nagłówek WAV (oczekiwano RIFF/WAVE)"));
        return false;
    }

    bool fmtFound = false;
    bool dataFound = false;
    QByteArray pcmData;
    QAudioFormat format;
    qint64 bytesPerSec = 0;
    int frameSize = 2;

    while (file.pos() < fileSize) {
        char chunkId[4];
        if (file.read(chunkId, 4) != 4)
            break;

        quint32 chunkSizeRaw = 0;
        if (file.read(reinterpret_cast<char*>(&chunkSizeRaw), 4) != 4)
            break;
        const quint32 chunkSize = qFromLittleEndian(chunkSizeRaw);

        if (std::memcmp(chunkId, "fmt ", 4) == 0) {
            if (chunkSize < 16) {
                setError(QStringLiteral("Za krótki chunk 'fmt ': %1").arg(chunkSize));
                return false;
            }

            quint16 audioFormat = 0;
            quint16 numChannels = 0;
            quint32 sampleRate = 0;
            quint32 byteRate = 0;
            quint16 blockAlign = 0;
            quint16 bitsPerSample = 0;

            file.read(reinterpret_cast<char*>(&audioFormat), 2);
            file.read(reinterpret_cast<char*>(&numChannels), 2);
            file.read(reinterpret_cast<char*>(&sampleRate), 4);
            file.read(reinterpret_cast<char*>(&byteRate), 4);
            file.read(reinterpret_cast<char*>(&blockAlign), 2);
            file.read(reinterpret_cast<char*>(&bitsPerSample), 2);

            audioFormat = qFromLittleEndian(audioFormat);
            numChannels = qFromLittleEndian(numChannels);
            sampleRate = qFromLittleEndian(sampleRate);
            byteRate = qFromLittleEndian(byteRate);
            blockAlign = qFromLittleEndian(blockAlign);
            bitsPerSample = qFromLittleEndian(bitsPerSample);

            format.setSampleRate(sampleRate);
            format.setChannelCount(numChannels);

            if (audioFormat == 1) { // PCM integer
                if (bitsPerSample == 16) {
                    format.setSampleFormat(QAudioFormat::Int16);
                } else if (bitsPerSample == 8) {
                    format.setSampleFormat(QAudioFormat::UInt8);
                } else if (bitsPerSample == 32) {
                    format.setSampleFormat(QAudioFormat::Int32);
                } else {
                    setError(QStringLiteral("Nieobsługiwana głębia bitowa PCM: %1 bit").arg(bitsPerSample));
                    return false;
                }
            } else if (audioFormat == 3) { // IEEE Float
                if (bitsPerSample == 32) {
                    format.setSampleFormat(QAudioFormat::Float);
                } else {
                    setError(QStringLiteral("Nieobsługiwany format zmiennoprzecinkowy: %1 bit").arg(bitsPerSample));
                    return false;
                }
            } else {
                setError(QStringLiteral("Nieobsługiwany format kompresji WAV: %1").arg(audioFormat));
                return false;
            }

            frameSize = numChannels * (bitsPerSample / 8);
            bytesPerSec = sampleRate * frameSize;
            fmtFound = true;

            if (chunkSize > 16) {
                file.seek(file.pos() + (chunkSize - 16));
            }
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            const qint64 remaining = fileSize - file.pos();
            const qint64 toRead = qMin<qint64>(chunkSize, remaining);
            pcmData = file.read(toRead);
            dataFound = true;
            break;
        } else {
            // Pomijamy inne chunki (np. LIST, JUNK, bext)
            file.seek(file.pos() + chunkSize);
        }

        // Każdy chunk WAV jest wyrównany do słowa (parzystej liczby bajtów)
        if (chunkSize % 2 != 0) {
            file.seek(file.pos() + 1);
        }
    }

    if (!fmtFound || !dataFound) {
        setError(QStringLiteral("Brak wymaganego formatu lub danych audio w pliku WAV"));
        return false;
    }

    m_format = format;
    m_pcmData = std::move(pcmData);
    m_frameSize = frameSize;
    m_bytesPerSec = bytesPerSec;
    m_durationMs = (m_bytesPerSec > 0) ? (m_pcmData.size() * 1000) / m_bytesPerSec : 0;

    if (m_buffer.isOpen()) {
        m_buffer.close();
    }
    m_buffer.setBuffer(&m_pcmData);
    if (!m_buffer.open(QIODevice::ReadOnly)) {
        setError(QStringLiteral("Nie można otworzyć wewnętrznego bufora PCM"));
        return false;
    }

    m_currentFilePath = filePath;
    emit currentFilePathChanged(m_currentFilePath);
    emit durationChanged(m_durationMs);

    m_errorMessage.clear();
    emit errorMessageChanged();
    return true;
}

bool AudioPlayer::load(const QString &filePath)
{
    stop();
    return parseWav(filePath);
}

bool AudioPlayer::play(const QString &filePath)
{
    if (!filePath.isEmpty() && filePath != m_currentFilePath) {
        if (!load(filePath))
            return false;
    } else if (m_pcmData.isEmpty()) {
        if (!m_currentFilePath.isEmpty()) {
            if (!load(m_currentFilePath))
                return false;
        } else {
            setError(QStringLiteral("Brak wybranego pliku audio do odtworzenia"));
            return false;
        }
    }

    // Jeśli byliśmy zapauzowani i nie zmieniono pliku, wznawiamy
    if (m_isPaused && m_audioSink) {
        resume();
        return true;
    }

    stop();

    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    try {
        m_audioSink = std::make_unique<QAudioSink>(device, m_format, this);
    } catch (...) {
        setError(QStringLiteral("Wyjątek podczas inicjalizacji QAudioSink"));
        return false;
    }

    if (!m_audioSink || m_audioSink->isNull()) {
        setError(QStringLiteral("Nie można zainicjalizować urządzenia wyjściowego audio"));
        cleanupSink();
        return false;
    }

    m_audioSink->setVolume(m_volume);
    connect(m_audioSink.get(), &QAudioSink::stateChanged, this, &AudioPlayer::handleStateChanged);

    if (m_buffer.isOpen()) {
        m_buffer.seek(0);
    }
    m_seekOffsetMs = 0;
    m_position = 0;
    emit positionChanged(0);

    m_audioSink->start(&m_buffer);

    if (m_audioSink->error() != QAudio::NoError) {
        setError(QStringLiteral("Błąd uruchomienia strumienia audio: %1").arg(static_cast<int>(m_audioSink->error())));
        cleanupSink();
        return false;
    }

    m_isPlaying = true;
    m_isPaused = false;
    emit playingChanged(true);
    emit pausedChanged(false);

    m_positionTimer.start();
    return true;
}

void AudioPlayer::pause()
{
    if (m_isPlaying && m_audioSink) {
        m_audioSink->suspend();
        m_positionTimer.stop();
        m_isPlaying = false;
        m_isPaused = true;
        emit playingChanged(false);
        emit pausedChanged(true);
    }
}

void AudioPlayer::resume()
{
    if (m_isPaused && m_audioSink) {
        if (m_audioSink->state() == QAudio::SuspendedState) {
            m_audioSink->resume();
        } else {
            m_audioSink->start(&m_buffer);
        }
        m_isPlaying = true;
        m_isPaused = false;
        emit playingChanged(true);
        emit pausedChanged(false);
        m_positionTimer.start();
    }
}

void AudioPlayer::stop()
{
    m_positionTimer.stop();
    if (m_audioSink) {
        m_audioSink->stop();
    }
    if (m_buffer.isOpen()) {
        m_buffer.seek(0);
    }
    m_seekOffsetMs = 0;
    m_position = 0;
    emit positionChanged(0);

    const bool wasPlaying = m_isPlaying;
    const bool wasPaused = m_isPaused;
    m_isPlaying = false;
    m_isPaused = false;
    if (wasPlaying)
        emit playingChanged(false);
    if (wasPaused)
        emit pausedChanged(false);
}

void AudioPlayer::togglePlayPause()
{
    if (m_isPlaying) {
        pause();
    } else if (m_isPaused) {
        resume();
    } else {
        play();
    }
}

void AudioPlayer::seek(qint64 positionMs)
{
    if (!m_buffer.isOpen() || m_pcmData.isEmpty())
        return;

    positionMs = qBound<qint64>(0, positionMs, m_durationMs);
    qint64 bytePos = 0;
    if (m_bytesPerSec > 0) {
        bytePos = (positionMs * m_bytesPerSec) / 1000;
        if (m_frameSize > 0)
            bytePos -= (bytePos % m_frameSize);
    }
    bytePos = qBound<qint64>(0, bytePos, m_pcmData.size());

    m_seekOffsetMs = positionMs;
    m_position = positionMs;
    emit positionChanged(m_position);

    if (m_isPlaying && m_audioSink) {
        m_audioSink->stop();
        m_buffer.seek(bytePos);
        m_audioSink->start(&m_buffer);
    } else {
        m_buffer.seek(bytePos);
    }
}

void AudioPlayer::setVolume(qreal volume)
{
    volume = qBound<qreal>(0.0, volume, 1.0);
    if (!qFuzzyCompare(m_volume, volume)) {
        m_volume = volume;
        if (m_audioSink)
            m_audioSink->setVolume(m_volume);
        emit volumeChanged(m_volume);
    }
}

void AudioPlayer::updatePosition()
{
    if (!m_audioSink || !m_isPlaying)
        return;

    qint64 posMs = 0;
    if (m_bytesPerSec > 0) {
        const qint64 processedMs = m_audioSink->processedUSecs() / 1000;
        posMs = m_seekOffsetMs + qMax(processedMs, 0LL);
        if (posMs > m_durationMs)
            posMs = m_durationMs;
    }

    if (posMs != m_position) {
        m_position = posMs;
        emit positionChanged(m_position);
    }
}

void AudioPlayer::handleStateChanged(QAudio::State state)
{
    switch (state) {
    case QAudio::ActiveState:
        if (!m_isPlaying) {
            m_isPlaying = true;
            m_isPaused = false;
            emit playingChanged(true);
            emit pausedChanged(false);
            m_positionTimer.start();
        }
        break;
    case QAudio::SuspendedState:
        if (m_isPlaying || !m_isPaused) {
            m_isPlaying = false;
            m_isPaused = true;
            m_positionTimer.stop();
            emit playingChanged(false);
            emit pausedChanged(true);
        }
        break;
    case QAudio::StoppedState:
        m_positionTimer.stop();
        if (m_audioSink && m_audioSink->error() != QAudio::NoError) {
            setError(QStringLiteral("Błąd odtwarzacza audio: %1").arg(static_cast<int>(m_audioSink->error())));
        }
        if (m_isPlaying || m_isPaused) {
            m_isPlaying = false;
            m_isPaused = false;
            emit playingChanged(false);
            emit pausedChanged(false);
        }
        break;
    case QAudio::IdleState:
        // Wszystkie dane z bufora zostały przekazane do DAC
        if (m_isPlaying) {
            m_positionTimer.stop();
            m_position = m_durationMs;
            emit positionChanged(m_position);

            m_isPlaying = false;
            m_isPaused = false;
            emit playingChanged(false);
            emit pausedChanged(false);

            if (m_audioSink) {
                m_audioSink->stop();
            }

            emit playbackFinished();
        }
        break;
    }
}
