#include "DoctorModel.h"
#include "AudioticonCore.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QClipboard>
#include <QStandardPaths>
#include <QProcess>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QDateTime>
#include <QUrl>
#include <unistd.h>

DoctorModel::DoctorModel(QObject *parent)
    : QObject(parent)
{
    loadSettings();
    refresh();
}

QString DoctorModel::qtVersion() const {
    return QString::fromLatin1(qVersion());
}

QString DoctorModel::qtPlatform() const {
    return QGuiApplication::platformName();
}

int DoctorModel::coreSampleRate() const {
    return audioticon::ui::coreSampleRate();
}

QString DoctorModel::xdgDataDir() const {
    const QString env = QString::fromLocal8Bit(qgetenv("XDG_DATA_HOME"));
    if (!env.isEmpty()) return env + QStringLiteral("/audioticon");
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/audioticon");
}

QString DoctorModel::xdgModelsDir() const {
    return xdgDataDir() + QStringLiteral("/models");
}

QString DoctorModel::xdgCacheDir() const {
    const QString env = QString::fromLocal8Bit(qgetenv("XDG_CACHE_HOME"));
    if (!env.isEmpty()) return env + QStringLiteral("/audioticon");
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/audioticon");
}

QString DoctorModel::xdgConfigDir() const {
    const QString env = QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME"));
    if (!env.isEmpty()) return env + QStringLiteral("/audioticon");
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/audioticon");
}

QString DoctorModel::xdgOutputsDir() const {
    return xdgDataDir() + QStringLiteral("/outputs");
}

void DoctorModel::setFirstRunCompleted(bool completed) {
    if (m_firstRunCompleted == completed) return;
    m_firstRunCompleted = completed;
    saveSettings();
    emit firstRunCompletedChanged();
}

void DoctorModel::setInspectionMode(bool enabled) {
    if (m_inspectionMode == enabled) return;
    m_inspectionMode = enabled;
    saveSettings();
    refresh();
    emit inspectionModeChanged();
}

bool DoctorModel::isEnvironmentReady() const {
    if (m_inspectionMode) {
        return m_qtStatus == QLatin1String("ok") && m_xdgDataWritable && m_xdgModelsWritable;
    }
    return m_qtStatus == QLatin1String("ok")
        && m_ffmpegStatus == QLatin1String("ok")
        && m_hasVoiceModel
        && m_errorCount == 0;
}

void DoctorModel::loadSettings() {
    QSettings st(QStringLiteral("astra-ui-agent"), QStringLiteral("astra-ui-agent"));
    m_firstRunCompleted = st.value(QStringLiteral("first_run_completed"), false).toBool();
    m_inspectionMode = st.value(QStringLiteral("inspection_mode"), false).toBool();
    m_activeModelPath = st.value(QStringLiteral("active_model_path"), QString()).toString();
}

void DoctorModel::saveSettings() {
    QSettings st(QStringLiteral("astra-ui-agent"), QStringLiteral("astra-ui-agent"));
    st.setValue(QStringLiteral("first_run_completed"), m_firstRunCompleted);
    st.setValue(QStringLiteral("inspection_mode"), m_inspectionMode);
    st.setValue(QStringLiteral("active_model_path"), m_activeModelPath);
}

void DoctorModel::completeFirstRun() {
    setFirstRunCompleted(true);
}

void DoctorModel::resetFirstRun() {
    m_firstRunCompleted = false;
    m_inspectionMode = false;
    saveSettings();
    emit firstRunCompletedChanged();
    emit inspectionModeChanged();
    refresh();
}

void DoctorModel::refresh() {
    m_errorCount = 0;
    m_warningCount = 0;

    probeXdg();
    probeQt();
    probeAudio();
    probeFfmpeg();
    probePiper();
    probeModels();
    probeOrt();

    emit statusChanged();
}

void DoctorModel::probeQt() {
    const int rate = coreSampleRate();
    if (rate == 44100) {
        m_qtStatus = QStringLiteral("ok");
        m_qtDetails = QCoreApplication::translate("DoctorModel", "Qt %1 · Platform: %2 · C++/Rust FFI: %3 Hz")
                      .arg(qtVersion(), qtPlatform(), QString::number(rate));
    } else {
        m_qtStatus = QStringLiteral("fail");
        m_qtDetails = QCoreApplication::translate("DoctorModel", "Rust FFI link error — coreSampleRate: %1 Hz (expected 44100 Hz)").arg(rate);
        m_errorCount++;
    }
}

void DoctorModel::probeAudio() {
    const QList<QAudioDevice> outputs = QMediaDevices::audioOutputs();
    const QAudioDevice def = QMediaDevices::defaultAudioOutput();

    if (!def.isNull() || !outputs.isEmpty()) {
        m_audioDeviceName = def.isNull() ? outputs.first().description() : def.description();
        m_audioStatus = QStringLiteral("ok");
        m_audioDetails = QCoreApplication::translate("DoctorModel", "%1 (%2 devices)").arg(m_audioDeviceName, QString::number(outputs.size()));
    } else {
        // Sprawdź PipeWire / Pulse / ALSA
        const QString pwSocket = QStringLiteral("/run/user/%1/pipewire-0").arg(getuid());
        if (QFile::exists(pwSocket) || QDir(QStringLiteral("/dev/snd")).exists()) {
            m_audioStatus = QStringLiteral("ok");
            m_audioDeviceName = QCoreApplication::translate("DoctorModel", "System Audio (PipeWire/ALSA)");
            m_audioDetails = QCoreApplication::translate("DoctorModel", "System sound server detected");
        } else {
            m_audioStatus = QStringLiteral("warn");
            m_audioDeviceName = QCoreApplication::translate("DoctorModel", "No output device");
            m_audioDetails = QCoreApplication::translate("DoctorModel", "Playback will be muted");
            m_warningCount++;
        }
    }
}

void DoctorModel::probeFfmpeg() {
    m_ffmpegPath = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    const QString ffprobePath = QStandardPaths::findExecutable(QStringLiteral("ffprobe"));

    if (m_ffmpegPath.isEmpty()) {
        m_ffmpegStatus = QStringLiteral("fail");
        m_ffmpegVersion = QCoreApplication::translate("DoctorModel", "missing");
        m_ffmpegDetails = QCoreApplication::translate("DoctorModel", "ffmpeg not found in PATH — rendering and export will not work");
        m_errorCount++;
        m_hasLoudnorm = false;
        m_hasFlac = false;
        m_hasAac = false;
        return;
    }

    // Odczytaj wersję
    QProcess proc;
    proc.start(m_ffmpegPath, QStringList() << QStringLiteral("-hide_banner") << QStringLiteral("-version"));
    if (proc.waitForFinished(1000)) {
        const QString firstLine = QString::fromUtf8(proc.readLine()).trimmed();
        m_ffmpegVersion = firstLine.section(' ', 0, 2);
    } else {
        m_ffmpegVersion = QCoreApplication::translate("DoctorModel", "ffmpeg (active)");
    }

    // Sprawdź loudnorm i kodery
    QProcess fltProc;
    fltProc.start(m_ffmpegPath, QStringList() << QStringLiteral("-hide_banner") << QStringLiteral("-filters"));
    if (fltProc.waitForFinished(1000)) {
        const QString filters = QString::fromUtf8(fltProc.readAll());
        m_hasLoudnorm = filters.contains(QStringLiteral("loudnorm"));
    } else {
        m_hasLoudnorm = true;
    }

    QProcess encProc;
    encProc.start(m_ffmpegPath, QStringList() << QStringLiteral("-hide_banner") << QStringLiteral("-encoders"));
    if (encProc.waitForFinished(1000)) {
        const QString encoders = QString::fromUtf8(encProc.readAll());
        m_hasFlac = encoders.contains(QStringLiteral("flac"));
        m_hasAac = encoders.contains(QStringLiteral("aac"));
    } else {
        m_hasFlac = true;
        m_hasAac = true;
    }

    if (!m_hasLoudnorm || !m_hasFlac) {
        m_ffmpegStatus = QStringLiteral("warn");
        m_ffmpegDetails = QCoreApplication::translate("DoctorModel", "%1 · Missing: %2%3")
                          .arg(m_ffmpegVersion,
                               !m_hasLoudnorm ? QStringLiteral("loudnorm ") : QString(),
                               !m_hasFlac ? QStringLiteral("flac ") : QString());
        m_warningCount++;
    } else {
        m_ffmpegStatus = QStringLiteral("ok");
        m_ffmpegDetails = QCoreApplication::translate("DoctorModel", "%1 · loudnorm, flac, aac, ffprobe: %2")
                          .arg(m_ffmpegVersion, ffprobePath.isEmpty() ? QCoreApplication::translate("DoctorModel", "missing") : QStringLiteral("OK"));
    }
}

void DoctorModel::probePiper() {
    // Sprawdź venv lub systemowy CLI piper
    const QString venvPiper = xdgDataDir() + QStringLiteral("/venv/bin/piper");
    const QString cliPiper = QStandardPaths::findExecutable(QStringLiteral("piper"));
    const QString venvPy = xdgDataDir() + QStringLiteral("/venv/bin/python3");
    const QString sysPy = QStandardPaths::findExecutable(QStringLiteral("python3"));

    if (QFile::exists(venvPiper)) {
        m_piperStatus = QStringLiteral("ok");
        m_piperPath = venvPiper;
        m_piperDetails = QCoreApplication::translate("DoctorModel", "Local Piper venv (%1)").arg(venvPiper);
    } else if (!cliPiper.isEmpty()) {
        m_piperStatus = QStringLiteral("ok");
        m_piperPath = cliPiper;
        m_piperDetails = QCoreApplication::translate("DoctorModel", "System Piper CLI (%1)").arg(cliPiper);
    } else {
        // Sprawdź moduł Pythona
        const QString py = QFile::exists(venvPy) ? venvPy : sysPy;
        bool hasPiperModule = false;
        if (!py.isEmpty()) {
            QProcess pyProc;
            pyProc.start(py, QStringList() << QStringLiteral("-c") << QStringLiteral("import piper"));
            if (pyProc.waitForFinished(1000) && pyProc.exitCode() == 0) {
                hasPiperModule = true;
            }
        }

        if (hasPiperModule) {
            m_piperStatus = QStringLiteral("ok");
            m_piperPath = py;
            m_piperDetails = QCoreApplication::translate("DoctorModel", "Python 'piper' module (%1)").arg(py);
        } else {
            m_piperStatus = QStringLiteral("warn");
            m_piperPath.clear();
            m_piperDetails = QCoreApplication::translate("DoctorModel", "Piper engine missing — synthesis disabled, director mode active");
            m_warningCount++;
        }
    }
}

void DoctorModel::probeModels() {
    m_installedModelsCount = 0;
    QString foundModel;

    // 1. Sprawdź AUDIOTICON_PIPER_MODEL w środowisku
    const QString envModel = QString::fromLocal8Bit(qgetenv("AUDIOTICON_PIPER_MODEL"));
    if (!envModel.isEmpty() && QFile::exists(envModel)) {
        foundModel = envModel;
    }

    // 2. Sprawdź m_activeModelPath z QSettings
    if (foundModel.isEmpty() && !m_activeModelPath.isEmpty() && QFile::exists(m_activeModelPath)) {
        foundModel = m_activeModelPath;
    }

    // 3. Sprawdź katalog modeli XDG
    const QDir modelsDir(xdgModelsDir());
    if (modelsDir.exists()) {
        const QStringList filters = { QStringLiteral("*.onnx") };
        const QFileInfoList files = modelsDir.entryInfoList(filters, QDir::Files | QDir::Readable);
        m_installedModelsCount = files.size();

        if (foundModel.isEmpty() && !files.isEmpty()) {
            // Szukaj pl_PL-bass lub pierwszego
            for (const QFileInfo &fi : files) {
                if (fi.fileName().contains(QStringLiteral("pl_PL-bass"), Qt::CaseInsensitive)) {
                    foundModel = fi.absoluteFilePath();
                    break;
                }
            }
            if (foundModel.isEmpty()) {
                foundModel = files.first().absoluteFilePath();
            }
        }
    }

    if (!foundModel.isEmpty()) {
        m_activeModelPath = foundModel;
        const QFileInfo fi(foundModel);
        m_activeModelName = fi.fileName();
        m_activeModelSize = fi.size();
        m_hasVoiceModel = true;
        m_modelStatus = QStringLiteral("ok");

        const bool hasJson = QFile::exists(foundModel + QStringLiteral(".json"))
                          || QFile::exists(foundModel.left(foundModel.lastIndexOf('.')) + QStringLiteral(".onnx.json"));
        m_modelDetails = QStringLiteral("%1 (%2 MB)%3")
                         .arg(m_activeModelName,
                              QString::number(m_activeModelSize / (1024.0 * 1024.0), 'f', 1),
                              hasJson ? QString() : QCoreApplication::translate("DoctorModel", " · [missing .json]"));
        qputenv("AUDIOTICON_PIPER_MODEL", foundModel.toLocal8Bit());
    } else {
        m_activeModelName.clear();
        m_activeModelSize = 0;
        m_hasVoiceModel = false;
        m_modelStatus = m_inspectionMode ? QStringLiteral("warn") : QStringLiteral("fail");
        m_modelDetails = QCoreApplication::translate("DoctorModel", "No voice model in %1").arg(xdgModelsDir());
        if (!m_inspectionMode) {
            m_errorCount++;
        } else {
            m_warningCount++;
        }
    }
}

void DoctorModel::probeOrt() {
    const QString envOrt = QString::fromLocal8Bit(qgetenv("ORT_DYLIB_PATH"));
    if (!envOrt.isEmpty() && QFile::exists(envOrt)) {
        m_ortStatus = QStringLiteral("ok");
        m_ortPath = envOrt;
        m_ortDetails = QStringLiteral("ORT_DYLIB_PATH: %1").arg(envOrt);
        return;
    }

    // Sprawdź systemowe lokalizacje
    const QStringList candidates = {
        xdgDataDir() + QStringLiteral("/venv/lib"),
        QStringLiteral("/usr/lib"),
        QStringLiteral("/usr/local/lib"),
        QStringLiteral("/usr/lib/x86_64-linux-gnu")
    };

    QString found;
    for (const QString &dirPath : candidates) {
        QDir d(dirPath);
        if (!d.exists()) continue;
        const QStringList entries = d.entryList(QStringList() << QStringLiteral("libonnxruntime.so*"), QDir::Files);
        if (!entries.isEmpty()) {
            found = d.absoluteFilePath(entries.first());
            break;
        }
    }

    if (!found.isEmpty()) {
        m_ortStatus = QStringLiteral("ok");
        m_ortPath = found;
        m_ortDetails = QCoreApplication::translate("DoctorModel", "Detected %1").arg(found);
    } else {
        m_ortStatus = QStringLiteral("optional");
        m_ortPath.clear();
        m_ortDetails = QCoreApplication::translate("DoctorModel", "Optional — Piper uses bundled ORT, Kokoro voices disabled");
    }
}

void DoctorModel::probeXdg() {
    const QStringList dirs = { xdgDataDir(), xdgModelsDir(), xdgCacheDir(), xdgOutputsDir() };
    for (const QString &p : dirs) {
        QDir d;
        d.mkpath(p);
    }

    // Test zapisu
    auto testWritable = [](const QString &dirPath) -> bool {
        const QString testFile = dirPath + QStringLiteral("/.write_test_audioticon");
        QFile f(testFile);
        if (f.open(QIODevice::WriteOnly)) {
            f.write("test", 4);
            f.close();
            f.remove();
            return true;
        }
        return false;
    };

    m_xdgDataWritable = testWritable(xdgDataDir());
    m_xdgModelsWritable = testWritable(xdgModelsDir());
    m_xdgCacheWritable = testWritable(xdgCacheDir());

    if (!m_xdgDataWritable || !m_xdgModelsWritable || !m_xdgCacheWritable) {
        m_errorCount++;
    }
}

bool DoctorModel::selectModelFile(const QString &filePath) {
    QString local = filePath;
    if (local.startsWith(QStringLiteral("file://"))) {
        local = QUrl(filePath).toLocalFile();
    }
    QFileInfo fi(local);
    if (!fi.exists() || !fi.isFile()) {
        return false;
    }

    m_activeModelPath = fi.absoluteFilePath();
    m_activeModelName = fi.fileName();
    m_activeModelSize = fi.size();
    m_hasVoiceModel = true;
    m_modelStatus = QStringLiteral("ok");
    m_modelDetails = QStringLiteral("%1 (%2 MB)").arg(m_activeModelName, QString::number(m_activeModelSize / (1024.0 * 1024.0), 'f', 1));
    qputenv("AUDIOTICON_PIPER_MODEL", m_activeModelPath.toLocal8Bit());

    saveSettings();
    refresh();
    return true;
}

QString DoctorModel::getRepairCommand(const QString &subsystem) const {
    if (subsystem == QLatin1String("ffmpeg")) {
        return QStringLiteral("sudo pacman -S ffmpeg");
    } else if (subsystem == QLatin1String("piper")) {
        return QStringLiteral("python3 -m venv \"%1/venv\" && \"%1/venv/bin/pip\" install piper-tts").arg(xdgDataDir());
    } else if (subsystem == QLatin1String("model")) {
        return QStringLiteral("mkdir -p \"%1\" && cp ~/Downloads/pl_PL-*.onnx* \"%1/\"").arg(xdgModelsDir());
    } else if (subsystem == QLatin1String("audio")) {
        return QStringLiteral("systemctl --user restart pipewire pipewire-pulse wireplumber");
    } else if (subsystem == QLatin1String("qt")) {
        return QStringLiteral("sudo pacman -S qt6-base qt6-declarative qt6-multimedia qt6-svg");
    } else if (subsystem == QLatin1String("ort")) {
        return QStringLiteral("export ORT_DYLIB_PATH=/sciezka/do/libonnxruntime.so");
    }
    return QString();
}

QString DoctorModel::generateReport() const {
    auto trDoc = [](const char *s) { return QCoreApplication::translate("DoctorModel", s); };
    const QString yesStr = trDoc("YES");
    const QString missingStr = trDoc("MISSING");
    const QString noneStr = trDoc("none");
    const QString writeOk = trDoc("writable OK");
    const QString writeErr = trDoc("WRITE ERROR");

    QString out;
    QTextStream ts(&out);
    ts << "========================================================\n";
    ts << "         " << trDoc("AUDIOTICON 2.0 — SYSTEM DIAGNOSTICS (DOCTOR)") << "\n";
    ts << "========================================================\n";
    ts << trDoc("Report date:") << "    " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    ts << trDoc("Environment:") << "    " << (m_inspectionMode
        ? trDoc("DIRECTOR MODE (READY WITHOUT TTS)")
        : (isEnvironmentReady() ? trDoc("READY") : trDoc("REQUIRES REPAIR"))) << "\n";
    ts << trDoc("Inspection mode:") << " " << (m_inspectionMode ? trDoc("YES (no TTS)") : trDoc("NO")) << "\n";
    ts << trDoc("Errors:") << " " << m_errorCount << " | " << trDoc("Warnings:") << " " << m_warningCount << "\n\n";

    ts << "[1/6] " << trDoc("Qt & C++/Rust FFI Runtime:") << "\n";
    ts << "  Status:        [" << m_qtStatus.toUpper() << "]\n";
    ts << "  " << trDoc("Qt version:") << "    " << qtVersion() << "\n";
    ts << "  " << trDoc("Platform:") << "      " << qtPlatform() << "\n";
    ts << "  FFI Core Rate: " << coreSampleRate() << " Hz\n";
    ts << "  " << trDoc("Details:") << "       " << m_qtDetails << "\n\n";

    ts << "[2/6] " << trDoc("Audio Device (QAudioSink):") << "\n";
    ts << "  Status:        [" << m_audioStatus.toUpper() << "]\n";
    ts << "  " << trDoc("Device:") << "        " << m_audioDeviceName << "\n";
    ts << "  " << trDoc("Details:") << "       " << m_audioDetails << "\n\n";

    ts << "[3/6] " << trDoc("FFmpeg & Audio Tools:") << "\n";
    ts << "  Status:        [" << m_ffmpegStatus.toUpper() << "]\n";
    ts << "  " << trDoc("Path:") << "          " << (m_ffmpegPath.isEmpty() ? noneStr : m_ffmpegPath) << "\n";
    ts << "  " << trDoc("Version:") << "       " << m_ffmpegVersion << "\n";
    ts << "  " << trDoc("Filters/Encoders:") << " loudnorm=" << (m_hasLoudnorm ? yesStr : missingStr)
       << ", flac=" << (m_hasFlac ? yesStr : missingStr)
       << ", aac=" << (m_hasAac ? yesStr : missingStr) << "\n\n";

    ts << "[4/6] " << trDoc("Piper-TTS Synthesis Engine:") << "\n";
    ts << "  Status:        [" << m_piperStatus.toUpper() << "]\n";
    ts << "  " << trDoc("Path:") << "          " << (m_piperPath.isEmpty() ? noneStr : m_piperPath) << "\n";
    ts << "  " << trDoc("Details:") << "       " << m_piperDetails << "\n\n";

    ts << "[5/6] " << trDoc("Voice Model:") << "\n";
    ts << "  Status:        [" << m_modelStatus.toUpper() << "]\n";
    ts << "  " << trDoc("Active model:") << "  " << (m_activeModelPath.isEmpty() ? noneStr : m_activeModelPath) << "\n";
    ts << "  " << trDoc("Size:") << "          " << (m_activeModelSize > 0 ? QString::number(m_activeModelSize / (1024*1024)) + " MB" : "-") << "\n";
    ts << "  " << trDoc("Model count:") << "   " << m_installedModelsCount << "\n\n";

    ts << "[6/6] " << trDoc("ONNX Runtime (Optional):") << "\n";
    ts << "  Status:        [" << m_ortStatus.toUpper() << "]\n";
    ts << "  " << trDoc("Path:") << "          " << (m_ortPath.isEmpty() ? noneStr : m_ortPath) << "\n";
    ts << "  " << trDoc("Details:") << "       " << m_ortDetails << "\n\n";

    ts << trDoc("Local directories (XDG):") << "\n";
    ts << "  Data:    " << xdgDataDir() << " (" << (m_xdgDataWritable ? writeOk : writeErr) << ")\n";
    ts << "  Models:  " << xdgModelsDir() << " (" << (m_xdgModelsWritable ? writeOk : writeErr) << ")\n";
    ts << "  Cache:   " << xdgCacheDir() << " (" << (m_xdgCacheWritable ? writeOk : writeErr) << ")\n";
    ts << "  Outputs: " << xdgOutputsDir() << "\n";
    ts << "========================================================\n";
    return out;
}

void DoctorModel::copyReportToClipboard() {
    QClipboard *cb = QGuiApplication::clipboard();
    if (cb) {
        cb->setText(generateReport());
    }
}
