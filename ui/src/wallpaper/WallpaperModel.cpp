#include "WallpaperModel.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QSet>

WallpaperModel::WallpaperModel(const QString &org, const QString &app, QObject *parent)
    : QObject(parent), m_org(org), m_app(app)
{
    m_rotationTimer.setSingleShot(false);
    connect(&m_rotationTimer, &QTimer::timeout, this, &WallpaperModel::nextWallpaper);

    loadSettings();
    scanAssets();
    if (m_enabled)
        validateSource();
    updateRotationTimer();
}

QString WallpaperModel::defaultWallpaperUrl()
{
    return QStringLiteral("qrc:/assets/wallpapers/pexels-artyusufpatel-8998289.jpg");
}

QString WallpaperModel::normalizeSourceUrl(const QString &path)
{
    if (path.isEmpty())
        return QString();
    if (path.startsWith(QLatin1String("qrc:/")) || path.startsWith(QLatin1String("file://")))
        return path;
    return QUrl::fromLocalFile(path).toString();
}

void WallpaperModel::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit enabledChanged();
    emit motionStateChanged();
    updateRotationTimer();
    saveSettings();
}

void WallpaperModel::setMode(const QString &mode)
{
    QString normalized = mode.trimmed().toLower();
    if (normalized != QLatin1String("none") && normalized != QLatin1String("static")
        && normalized != QLatin1String("video") && normalized != QLatin1String("rotate")) {
        normalized = QStringLiteral("static");
    }
    if (m_mode == normalized)
        return;
    m_mode = normalized;
    emit modeChanged();
    updateRotationTimer();
    saveSettings();
}

void WallpaperModel::setSource(const QString &source)
{
    const QString url = normalizeSourceUrl(source);
    if (m_source == url)
        return;
    m_source = url;
    emit sourceChanged();
    saveSettings();
}

void WallpaperModel::setWallpaperOpacity(double opacity)
{
    opacity = qBound(0.0, opacity, 1.0);
    if (qFuzzyCompare(m_wallpaperOpacity, opacity))
        return;
    m_wallpaperOpacity = opacity;
    emit wallpaperOpacityChanged();
    saveSettings();
}

void WallpaperModel::setGlassOpacity(double opacity)
{
    opacity = qBound(0.30, opacity, 0.98);
    if (qFuzzyCompare(m_glassOpacity, opacity))
        return;
    m_glassOpacity = opacity;
    emit glassOpacityChanged();
    saveSettings();
}

void WallpaperModel::setBlurRadius(double radius)
{
    radius = qBound(0.0, radius, 64.0);
    if (qFuzzyCompare(m_blurRadius, radius))
        return;
    m_blurRadius = radius;
    emit blurRadiusChanged();
    saveSettings();
}

void WallpaperModel::setDarknessOverlay(double darkness)
{
    darkness = qBound(0.0, darkness, 0.95);
    if (qFuzzyCompare(m_darknessOverlay, darkness))
        return;
    m_darknessOverlay = darkness;
    emit darknessOverlayChanged();
    saveSettings();
}

void WallpaperModel::setSaturation(double saturation)
{
    saturation = qBound(0.0, saturation, 1.5);
    if (qFuzzyCompare(m_saturation, saturation))
        return;
    m_saturation = saturation;
    emit saturationChanged();
    saveSettings();
}

void WallpaperModel::setVignette(double vignette)
{
    vignette = qBound(0.0, vignette, 1.0);
    if (qFuzzyCompare(m_vignette, vignette))
        return;
    m_vignette = vignette;
    emit vignetteChanged();
    saveSettings();
}

void WallpaperModel::setPauseMotion(bool pause)
{
    if (m_pauseMotion == pause)
        return;
    m_pauseMotion = pause;
    emit pauseMotionChanged();
    emit motionStateChanged();
    updateRotationTimer();
    saveSettings();
}

void WallpaperModel::setReducedMotion(bool reduced)
{
    if (m_reducedMotion == reduced)
        return;
    m_reducedMotion = reduced;
    emit reducedMotionChanged();
    emit motionStateChanged();
    updateRotationTimer();
    saveSettings();
}

void WallpaperModel::setRotationIntervalSec(int sec)
{
    sec = qBound(5, sec, 3600);
    if (m_rotationIntervalSec == sec)
        return;
    m_rotationIntervalSec = sec;
    emit rotationIntervalSecChanged();
    updateRotationTimer();
    saveSettings();
}

void WallpaperModel::updateRotationTimer()
{
    if (m_enabled && m_mode == QLatin1String("rotate") && !effectiveMotionPaused()
        && m_availableWallpapers.size() > 1) {
        m_rotationTimer.setInterval(m_rotationIntervalSec * 1000);
        if (!m_rotationTimer.isActive())
            m_rotationTimer.start();
    } else {
        m_rotationTimer.stop();
    }
}

void WallpaperModel::scanAssets(const QString &extraDir)
{
    QVariantList list;
    QSet<QString> seenNames;

    // 1. Wbudowana domyślna tapeta statyczna (qrc)
    {
        QVariantMap item;
        item[QStringLiteral("name")] = QCoreApplication::translate("WallpaperModel", "Studio Cyan Turbine (Default)");
        item[QStringLiteral("fileName")] = QStringLiteral("pexels-artyusufpatel-8998289.jpg");
        item[QStringLiteral("path")] = defaultWallpaperUrl();
        item[QStringLiteral("kind")] = QStringLiteral("static");
        item[QStringLiteral("origin")] = QStringLiteral("built-in");
        list.append(item);
        seenNames.insert(QStringLiteral("pexels-artyusufpatel-8998289.jpg"));
    }

    QStringList searchDirs;
    const QString appDir = QCoreApplication::applicationDirPath();
    searchDirs << appDir + QStringLiteral("/../assets/wallpapers")
               << appDir + QStringLiteral("/assets/wallpapers")
               << QDir::currentPath() + QStringLiteral("/assets/wallpapers");

    if (!extraDir.isEmpty())
        searchDirs << extraDir;

    const QString downloadsDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (!downloadsDir.isEmpty())
        searchDirs << downloadsDir;

    const QStringList staticExts = {QStringLiteral("jpg"), QStringLiteral("jpeg"),
                                    QStringLiteral("png"), QStringLiteral("webp")};
    const QStringList videoExts = {QStringLiteral("mp4"), QStringLiteral("webm"),
                                   QStringLiteral("mkv")};

    for (const QString &dirPath : searchDirs) {
        QDir dir(dirPath);
        if (!dir.exists())
            continue;

        const bool isDownloads = (dirPath == downloadsDir);
        const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Readable, QDir::Name);
        for (const QFileInfo &fi : entries) {
            const QString fname = fi.fileName();
            if (seenNames.contains(fname))
                continue;

            const QString ext = fi.suffix().toLower();
            QString kind;
            if (staticExts.contains(ext))
                kind = QStringLiteral("static");
            else if (videoExts.contains(ext))
                kind = QStringLiteral("video");
            else
                continue;

            seenNames.insert(fname);
            QVariantMap item;
            item[QStringLiteral("name")] = fi.completeBaseName();
            item[QStringLiteral("fileName")] = fname;
            item[QStringLiteral("path")] = QUrl::fromLocalFile(fi.absoluteFilePath()).toString();
            item[QStringLiteral("kind")] = kind;
            item[QStringLiteral("origin")] = isDownloads ? QStringLiteral("downloads") : QStringLiteral("assets");
            item[QStringLiteral("sizeMb")] = qRound((fi.size() / (1024.0 * 1024.0)) * 10.0) / 10.0;
            list.append(item);
        }
    }

    m_availableWallpapers = list;
    emit availableWallpapersChanged();
    updateRotationTimer();
}

void WallpaperModel::selectWallpaper(const QString &path, const QString &kind)
{
    const QString url = normalizeSourceUrl(path);
    QString effectiveKind = kind;
    if (effectiveKind.isEmpty()) {
        const QString lower = url.toLower();
        if (lower.endsWith(QLatin1String(".mp4")) || lower.endsWith(QLatin1String(".webm"))
            || lower.endsWith(QLatin1String(".mkv"))) {
            effectiveKind = QStringLiteral("video");
        } else {
            effectiveKind = QStringLiteral("static");
        }
    }

    setEnabled(true);
    setSource(url);
    if (effectiveKind == QLatin1String("video")) {
        setMode(QStringLiteral("video"));
    } else if (m_mode != QLatin1String("rotate")) {
        setMode(QStringLiteral("static"));
    }
}

void WallpaperModel::nextWallpaper()
{
    // Rotacja między statycznymi tapetami (pomijając usunięte pliki)
    QStringList staticPaths;
    for (const QVariant &v : m_availableWallpapers) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("kind")).toString() == QLatin1String("static")) {
            const QString p = m.value(QStringLiteral("path")).toString();
            if (p.startsWith(QLatin1String("qrc:/"))
                || QFileInfo::exists(QUrl(p).toLocalFile())) {
                staticPaths.append(p);
            }
        }
    }
    if (staticPaths.isEmpty()) {
        validateSource();
        return;
    }

    const int idx = staticPaths.indexOf(m_source);
    const int nextIdx = (idx + 1) % staticPaths.size();
    setSource(staticPaths.at(nextIdx));
}

bool WallpaperModel::validateSource()
{
    if (m_source.isEmpty() || m_source.startsWith(QLatin1String("qrc:/"))) {
        if (!m_fallbackNotice.isEmpty()) {
            m_fallbackNotice.clear();
            emit fallbackNoticeChanged();
        }
        return true;
    }

    const QString localPath = m_source.startsWith(QLatin1String("file://"))
                                  ? QUrl(m_source).toLocalFile()
                                  : m_source;
    if (QFileInfo::exists(localPath)) {
        if (!m_fallbackNotice.isEmpty()) {
            m_fallbackNotice.clear();
            emit fallbackNoticeChanged();
        }
        return true;
    }

    // Usunięty lub brakujący zasób — bezpieczny powrót do wbudowanej tapety
    m_source = defaultWallpaperUrl();
    if (m_mode == QLatin1String("video")) {
        m_mode = QStringLiteral("static");
        emit modeChanged();
    }
    m_fallbackNotice = QCoreApplication::translate(
        "WallpaperModel",
        "Selected wallpaper file was removed; reverted to built-in studio wallpaper.");
    emit sourceChanged();
    emit fallbackNoticeChanged();
    scanAssets();
    saveSettings();
    return false;
}

void WallpaperModel::resetDefaults()
{
    m_enabled = true;
    m_mode = QStringLiteral("static");
    m_source = defaultWallpaperUrl();
    m_wallpaperOpacity = 0.35;
    m_glassOpacity = 0.72;
    m_blurRadius = 14.0;
    m_darknessOverlay = 0.50;
    m_saturation = 0.85;
    m_vignette = 0.48;
    m_pauseMotion = false;
    m_reducedMotion = false;
    m_rotationIntervalSec = 30;
    m_fallbackNotice.clear();

    emit enabledChanged();
    emit modeChanged();
    emit sourceChanged();
    emit wallpaperOpacityChanged();
    emit glassOpacityChanged();
    emit blurRadiusChanged();
    emit darknessOverlayChanged();
    emit saturationChanged();
    emit vignetteChanged();
    emit pauseMotionChanged();
    emit reducedMotionChanged();
    emit rotationIntervalSecChanged();
    emit motionStateChanged();
    emit fallbackNoticeChanged();

    updateRotationTimer();
    saveSettings();
}

void WallpaperModel::saveSettings()
{
    QSettings st(m_org, m_app);
    st.beginGroup(QStringLiteral("wallpaper"));
    st.setValue(QStringLiteral("enabled"), m_enabled);
    st.setValue(QStringLiteral("mode"), m_mode);
    st.setValue(QStringLiteral("source"), m_source);
    st.setValue(QStringLiteral("wallpaperOpacity"), m_wallpaperOpacity);
    st.setValue(QStringLiteral("glassOpacity"), m_glassOpacity);
    st.setValue(QStringLiteral("blurRadius"), m_blurRadius);
    st.setValue(QStringLiteral("darknessOverlay"), m_darknessOverlay);
    st.setValue(QStringLiteral("saturation"), m_saturation);
    st.setValue(QStringLiteral("vignette"), m_vignette);
    st.setValue(QStringLiteral("pauseMotion"), m_pauseMotion);
    st.setValue(QStringLiteral("reducedMotion"), m_reducedMotion);
    st.setValue(QStringLiteral("rotationIntervalSec"), m_rotationIntervalSec);
    st.endGroup();
    st.sync();
}

void WallpaperModel::loadSettings()
{
    QSettings st(m_org, m_app);
    st.beginGroup(QStringLiteral("wallpaper"));
    if (st.contains(QStringLiteral("enabled")))
        m_enabled = st.value(QStringLiteral("enabled"), true).toBool();
    if (st.contains(QStringLiteral("mode")))
        m_mode = st.value(QStringLiteral("mode"), QStringLiteral("static")).toString();
    if (st.contains(QStringLiteral("source")))
        m_source = st.value(QStringLiteral("source"), m_source).toString();
    if (st.contains(QStringLiteral("wallpaperOpacity")))
        m_wallpaperOpacity = qBound(0.0, st.value(QStringLiteral("wallpaperOpacity"), 0.35).toDouble(), 1.0);
    if (st.contains(QStringLiteral("glassOpacity")))
        m_glassOpacity = qBound(0.30, st.value(QStringLiteral("glassOpacity"), 0.72).toDouble(), 0.98);
    if (st.contains(QStringLiteral("blurRadius")))
        m_blurRadius = qBound(0.0, st.value(QStringLiteral("blurRadius"), 14.0).toDouble(), 64.0);
    if (st.contains(QStringLiteral("darknessOverlay")))
        m_darknessOverlay = qBound(0.0, st.value(QStringLiteral("darknessOverlay"), 0.50).toDouble(), 0.95);
    if (st.contains(QStringLiteral("saturation")))
        m_saturation = qBound(0.0, st.value(QStringLiteral("saturation"), 0.85).toDouble(), 1.5);
    if (st.contains(QStringLiteral("vignette")))
        m_vignette = qBound(0.0, st.value(QStringLiteral("vignette"), 0.48).toDouble(), 1.0);
    if (st.contains(QStringLiteral("pauseMotion")))
        m_pauseMotion = st.value(QStringLiteral("pauseMotion"), false).toBool();
    if (st.contains(QStringLiteral("reducedMotion")))
        m_reducedMotion = st.value(QStringLiteral("reducedMotion"), false).toBool();
    if (st.contains(QStringLiteral("rotationIntervalSec")))
        m_rotationIntervalSec = qBound(5, st.value(QStringLiteral("rotationIntervalSec"), 30).toInt(), 3600);
    st.endGroup();
}
