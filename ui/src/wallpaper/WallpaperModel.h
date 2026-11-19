#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QTimer>

class WallpaperModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(double wallpaperOpacity READ wallpaperOpacity WRITE setWallpaperOpacity NOTIFY wallpaperOpacityChanged)
    Q_PROPERTY(double glassOpacity READ glassOpacity WRITE setGlassOpacity NOTIFY glassOpacityChanged)
    Q_PROPERTY(double blurRadius READ blurRadius WRITE setBlurRadius NOTIFY blurRadiusChanged)
    Q_PROPERTY(double darknessOverlay READ darknessOverlay WRITE setDarknessOverlay NOTIFY darknessOverlayChanged)
    Q_PROPERTY(double saturation READ saturation WRITE setSaturation NOTIFY saturationChanged)
    Q_PROPERTY(double vignette READ vignette WRITE setVignette NOTIFY vignetteChanged)
    Q_PROPERTY(bool pauseMotion READ pauseMotion WRITE setPauseMotion NOTIFY pauseMotionChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)
    Q_PROPERTY(int rotationIntervalSec READ rotationIntervalSec WRITE setRotationIntervalSec NOTIFY rotationIntervalSecChanged)
    Q_PROPERTY(QVariantList availableWallpapers READ availableWallpapers NOTIFY availableWallpapersChanged)
    Q_PROPERTY(bool effectiveMotionPaused READ effectiveMotionPaused NOTIFY motionStateChanged)
    Q_PROPERTY(QString fallbackNotice READ fallbackNotice NOTIFY fallbackNoticeChanged)

public:
    explicit WallpaperModel(const QString &org = QStringLiteral("astra-ui-agent"),
                            const QString &app = QStringLiteral("astra-ui-agent"),
                            QObject *parent = nullptr);
    ~WallpaperModel() override = default;

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    QString mode() const { return m_mode; }
    void setMode(const QString &mode);

    QString source() const { return m_source; }
    void setSource(const QString &source);

    double wallpaperOpacity() const { return m_wallpaperOpacity; }
    void setWallpaperOpacity(double opacity);

    double glassOpacity() const { return m_glassOpacity; }
    void setGlassOpacity(double opacity);

    double blurRadius() const { return m_blurRadius; }
    void setBlurRadius(double radius);

    double darknessOverlay() const { return m_darknessOverlay; }
    void setDarknessOverlay(double darkness);

    double saturation() const { return m_saturation; }
    void setSaturation(double saturation);

    double vignette() const { return m_vignette; }
    void setVignette(double vignette);

    bool pauseMotion() const { return m_pauseMotion; }
    void setPauseMotion(bool pause);

    bool reducedMotion() const { return m_reducedMotion; }
    void setReducedMotion(bool reduced);

    int rotationIntervalSec() const { return m_rotationIntervalSec; }
    void setRotationIntervalSec(int sec);

    QVariantList availableWallpapers() const { return m_availableWallpapers; }
    bool effectiveMotionPaused() const { return !m_enabled || m_pauseMotion || m_reducedMotion; }
    QString fallbackNotice() const { return m_fallbackNotice; }

    Q_INVOKABLE void scanAssets(const QString &extraDir = QString());
    Q_INVOKABLE void selectWallpaper(const QString &path, const QString &kind = QString());
    Q_INVOKABLE void nextWallpaper();
    Q_INVOKABLE bool validateSource();
    Q_INVOKABLE void resetDefaults();
    Q_INVOKABLE void saveSettings();
    Q_INVOKABLE void loadSettings();

signals:
    void enabledChanged();
    void modeChanged();
    void sourceChanged();
    void wallpaperOpacityChanged();
    void glassOpacityChanged();
    void blurRadiusChanged();
    void darknessOverlayChanged();
    void saturationChanged();
    void vignetteChanged();
    void pauseMotionChanged();
    void reducedMotionChanged();
    void rotationIntervalSecChanged();
    void availableWallpapersChanged();
    void motionStateChanged();
    void fallbackNoticeChanged();

private:
    void updateRotationTimer();
    static QString normalizeSourceUrl(const QString &path);
    static QString defaultWallpaperUrl();

    QString m_org;
    QString m_app;

    // Domyślne wartości zaprojektowane dla prawdziwego, czytelnego szkła Studio Dark Glass:
    bool m_enabled = true;
    QString m_mode = QStringLiteral("static"); // "none" | "static" | "video" | "rotate"
    QString m_source = QStringLiteral("qrc:/assets/wallpapers/pexels-artyusufpatel-8998289.jpg");
    double m_wallpaperOpacity = 0.35;
    double m_glassOpacity = 0.72;
    double m_blurRadius = 14.0;
    double m_darknessOverlay = 0.50;
    double m_saturation = 0.85;
    double m_vignette = 0.48;
    bool m_pauseMotion = false;
    bool m_reducedMotion = false;
    int m_rotationIntervalSec = 30;
    QString m_fallbackNotice;

    QVariantList m_availableWallpapers;
    QTimer m_rotationTimer;
};
