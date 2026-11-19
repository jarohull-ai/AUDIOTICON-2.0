#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QFileInfo>
#include <QDir>
#include <QSettings>

// DoctorModel — natywny model diagnostyki środowiska, ścieżek XDG,
// weryfikacji FFI/FFmpeg/Piper/modeli oraz sterowania FirstRunWizard i trybem inspekcji.
class DoctorModel : public QObject {
    Q_OBJECT

    // Statusy podsystemów: "ok" | "warn" | "fail" | "optional"
    Q_PROPERTY(QString qtStatus READ qtStatus NOTIFY statusChanged)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(QString qtPlatform READ qtPlatform CONSTANT)
    Q_PROPERTY(int coreSampleRate READ coreSampleRate CONSTANT)
    Q_PROPERTY(QString qtDetails READ qtDetails NOTIFY statusChanged)

    Q_PROPERTY(QString audioStatus READ audioStatus NOTIFY statusChanged)
    Q_PROPERTY(QString audioDeviceName READ audioDeviceName NOTIFY statusChanged)
    Q_PROPERTY(QString audioDetails READ audioDetails NOTIFY statusChanged)

    Q_PROPERTY(QString ffmpegStatus READ ffmpegStatus NOTIFY statusChanged)
    Q_PROPERTY(QString ffmpegVersion READ ffmpegVersion NOTIFY statusChanged)
    Q_PROPERTY(QString ffmpegPath READ ffmpegPath NOTIFY statusChanged)
    Q_PROPERTY(bool hasLoudnorm READ hasLoudnorm NOTIFY statusChanged)
    Q_PROPERTY(bool hasFlac READ hasFlac NOTIFY statusChanged)
    Q_PROPERTY(bool hasAac READ hasAac NOTIFY statusChanged)
    Q_PROPERTY(QString ffmpegDetails READ ffmpegDetails NOTIFY statusChanged)

    Q_PROPERTY(QString piperStatus READ piperStatus NOTIFY statusChanged)
    Q_PROPERTY(QString piperPath READ piperPath NOTIFY statusChanged)
    Q_PROPERTY(QString piperDetails READ piperDetails NOTIFY statusChanged)

    Q_PROPERTY(QString modelStatus READ modelStatus NOTIFY statusChanged)
    Q_PROPERTY(QString activeModelPath READ activeModelPath NOTIFY statusChanged)
    Q_PROPERTY(QString activeModelName READ activeModelName NOTIFY statusChanged)
    Q_PROPERTY(qint64 activeModelSize READ activeModelSize NOTIFY statusChanged)
    Q_PROPERTY(int installedModelsCount READ installedModelsCount NOTIFY statusChanged)
    Q_PROPERTY(bool hasVoiceModel READ hasVoiceModel NOTIFY statusChanged)
    Q_PROPERTY(QString modelDetails READ modelDetails NOTIFY statusChanged)

    Q_PROPERTY(QString ortStatus READ ortStatus NOTIFY statusChanged)
    Q_PROPERTY(QString ortPath READ ortPath NOTIFY statusChanged)
    Q_PROPERTY(QString ortDetails READ ortDetails NOTIFY statusChanged)

    // Ścieżki XDG
    Q_PROPERTY(QString xdgDataDir READ xdgDataDir CONSTANT)
    Q_PROPERTY(QString xdgModelsDir READ xdgModelsDir CONSTANT)
    Q_PROPERTY(QString xdgCacheDir READ xdgCacheDir CONSTANT)
    Q_PROPERTY(QString xdgConfigDir READ xdgConfigDir CONSTANT)
    Q_PROPERTY(QString xdgOutputsDir READ xdgOutputsDir CONSTANT)

    Q_PROPERTY(bool xdgDataWritable READ xdgDataWritable NOTIFY statusChanged)
    Q_PROPERTY(bool xdgModelsWritable READ xdgModelsWritable NOTIFY statusChanged)
    Q_PROPERTY(bool xdgCacheWritable READ xdgCacheWritable NOTIFY statusChanged)

    // Stany workflow (First-Run & Inspection Mode)
    Q_PROPERTY(bool firstRunCompleted READ firstRunCompleted WRITE setFirstRunCompleted NOTIFY firstRunCompletedChanged)
    Q_PROPERTY(bool inspectionMode READ inspectionMode WRITE setInspectionMode NOTIFY inspectionModeChanged)
    Q_PROPERTY(bool isEnvironmentReady READ isEnvironmentReady NOTIFY statusChanged)
    Q_PROPERTY(int errorCount READ errorCount NOTIFY statusChanged)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY statusChanged)

public:
    explicit DoctorModel(QObject *parent = nullptr);
    ~DoctorModel() override = default;

    QString qtStatus() const { return m_qtStatus; }
    QString qtVersion() const;
    QString qtPlatform() const;
    int coreSampleRate() const;
    QString qtDetails() const { return m_qtDetails; }

    QString audioStatus() const { return m_audioStatus; }
    QString audioDeviceName() const { return m_audioDeviceName; }
    QString audioDetails() const { return m_audioDetails; }

    QString ffmpegStatus() const { return m_ffmpegStatus; }
    QString ffmpegVersion() const { return m_ffmpegVersion; }
    QString ffmpegPath() const { return m_ffmpegPath; }
    bool hasLoudnorm() const { return m_hasLoudnorm; }
    bool hasFlac() const { return m_hasFlac; }
    bool hasAac() const { return m_hasAac; }
    QString ffmpegDetails() const { return m_ffmpegDetails; }

    QString piperStatus() const { return m_piperStatus; }
    QString piperPath() const { return m_piperPath; }
    QString piperDetails() const { return m_piperDetails; }

    QString modelStatus() const { return m_modelStatus; }
    QString activeModelPath() const { return m_activeModelPath; }
    QString activeModelName() const { return m_activeModelName; }
    qint64 activeModelSize() const { return m_activeModelSize; }
    int installedModelsCount() const { return m_installedModelsCount; }
    bool hasVoiceModel() const { return m_hasVoiceModel; }
    QString modelDetails() const { return m_modelDetails; }

    QString ortStatus() const { return m_ortStatus; }
    QString ortPath() const { return m_ortPath; }
    QString ortDetails() const { return m_ortDetails; }

    QString xdgDataDir() const;
    QString xdgModelsDir() const;
    QString xdgCacheDir() const;
    QString xdgConfigDir() const;
    QString xdgOutputsDir() const;

    bool xdgDataWritable() const { return m_xdgDataWritable; }
    bool xdgModelsWritable() const { return m_xdgModelsWritable; }
    bool xdgCacheWritable() const { return m_xdgCacheWritable; }

    bool firstRunCompleted() const { return m_firstRunCompleted; }
    Q_INVOKABLE void setFirstRunCompleted(bool completed);

    bool inspectionMode() const { return m_inspectionMode; }
    Q_INVOKABLE void setInspectionMode(bool enabled);

    bool isEnvironmentReady() const;
    int errorCount() const { return m_errorCount; }
    int warningCount() const { return m_warningCount; }

    // Akcje Q_INVOKABLE
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QString generateReport() const;
    Q_INVOKABLE void copyReportToClipboard();
    Q_INVOKABLE bool selectModelFile(const QString &filePath);
    Q_INVOKABLE QString getRepairCommand(const QString &subsystem) const;
    Q_INVOKABLE void resetFirstRun();
    Q_INVOKABLE void completeFirstRun();

signals:
    void statusChanged();
    void firstRunCompletedChanged();
    void inspectionModeChanged();

private:
    void probeQt();
    void probeAudio();
    void probeFfmpeg();
    void probePiper();
    void probeModels();
    void probeOrt();
    void probeXdg();
    void loadSettings();
    void saveSettings();

    QString m_qtStatus = QStringLiteral("ok");
    QString m_qtDetails;

    QString m_audioStatus = QStringLiteral("ok");
    QString m_audioDeviceName;
    QString m_audioDetails;

    QString m_ffmpegStatus = QStringLiteral("ok");
    QString m_ffmpegVersion;
    QString m_ffmpegPath;
    bool m_hasLoudnorm = true;
    bool m_hasFlac = true;
    bool m_hasAac = true;
    QString m_ffmpegDetails;

    QString m_piperStatus = QStringLiteral("ok");
    QString m_piperPath;
    QString m_piperDetails;

    QString m_modelStatus = QStringLiteral("warn");
    QString m_activeModelPath;
    QString m_activeModelName;
    qint64 m_activeModelSize = 0;
    int m_installedModelsCount = 0;
    bool m_hasVoiceModel = false;
    QString m_modelDetails;

    QString m_ortStatus = QStringLiteral("optional");
    QString m_ortPath;
    QString m_ortDetails;

    bool m_xdgDataWritable = true;
    bool m_xdgModelsWritable = true;
    bool m_xdgCacheWritable = true;

    bool m_firstRunCompleted = false;
    bool m_inspectionMode = false;

    int m_errorCount = 0;
    int m_warningCount = 0;
};
