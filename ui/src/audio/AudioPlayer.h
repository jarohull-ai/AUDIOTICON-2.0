#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QBuffer>
#include <QTimer>
#include <QAudioFormat>
#include <QAudioSink>
#include <QAudioDevice>
#include <memory>

class AudioPlayer : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playingChanged)
    Q_PROPERTY(bool isPaused READ isPaused NOTIFY pausedChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(QString currentFilePath READ currentFilePath NOTIFY currentFilePathChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)

public:
    explicit AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer() override;

    bool isPlaying() const { return m_isPlaying; }
    bool isPaused() const { return m_isPaused; }
    qint64 position() const { return m_position; }
    qint64 duration() const { return m_durationMs; }
    QString currentFilePath() const { return m_currentFilePath; }
    QString errorMessage() const { return m_errorMessage; }
    qreal volume() const { return m_volume; }
    const QAudioFormat &audioFormat() const { return m_format; }

public slots:
    bool load(const QString &filePath);
    bool play(const QString &filePath = QString());
    void pause();
    void resume();
    void stop();
    void togglePlayPause();
    void seek(qint64 positionMs);
    void setVolume(qreal volume);

signals:
    void playingChanged(bool isPlaying);
    void pausedChanged(bool isPaused);
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void playbackFinished();
    void currentFilePathChanged(const QString &filePath);
    void errorMessageChanged();
    void errorOccurred(const QString &error);
    void volumeChanged(qreal volume);

private slots:
    void handleStateChanged(QAudio::State state);
    void updatePosition();

private:
    bool parseWav(const QString &filePath);
    void cleanupSink();
    void setError(const QString &error);

    bool m_isPlaying = false;
    bool m_isPaused = false;
    qint64 m_position = 0;
    qint64 m_durationMs = 0;
    qint64 m_seekOffsetMs = 0;
    qint64 m_bytesPerSec = 0;
    int m_frameSize = 2;
    qreal m_volume = 1.0;

    QString m_currentFilePath;
    QString m_errorMessage;

    QAudioFormat m_format;
    QByteArray m_pcmData;
    QBuffer m_buffer;

    std::unique_ptr<QAudioSink> m_audioSink;
    QTimer m_positionTimer;
};
