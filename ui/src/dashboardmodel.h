#pragma once

#include <QElapsedTimer>
#include <QMap>
#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

// The data contract of the ASTRA dashboard shell.
//
// Subclass it, describe your metrics in the constructor with addMetric(), and
// fill in sample() with publish() calls. The UI does the rest: layout, history
// charts, persistence, palette, glass density, pause state and the state strip.
//
//     class MyModel : public DashboardModel {
//     public:
//         MyModel() : DashboardModel("myapp", "myapp") {
//             addMetric("latency", tr("API latency"), "ms", "#4fc3ff");
//             start(1000);                       // sample every second
//         }
//         void sample() override {
//             publish("latency", QString::number(api.p95(), 'f', 0), "ms",
//                     tr("p95 over the last minute"), api.p95(), true);
//             setStrip(myStrip);                 // optional bottom strip
//         }
//     };
class DashboardModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList cards READ cards NOTIFY changed)
    Q_PROPERTY(QVariantList catalog READ catalog NOTIFY catalogChanged)
    Q_PROPERTY(QStringList order READ order NOTIFY layoutChanged)
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString palette READ palette WRITE setPalette NOTIFY layoutChanged)
    Q_PROPERTY(double glass READ glass WRITE setGlass NOTIFY layoutChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY changed)
    Q_PROPERTY(QVariantMap strip READ strip NOTIFY changed)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString subtitle READ subtitle WRITE setSubtitle NOTIFY titleChanged)
    Q_PROPERTY(QString mark READ mark WRITE setMark NOTIFY titleChanged)
    Q_PROPERTY(QString heading READ heading WRITE setHeading NOTIFY titleChanged)
    Q_PROPERTY(QString tagline READ tagline WRITE setTagline NOTIFY titleChanged)
    Q_PROPERTY(QString footer READ footer WRITE setFooter NOTIFY titleChanged)
    Q_PROPERTY(QString hint READ hint WRITE setHint NOTIFY titleChanged)

public:
    // settingsOrganization/settingsApplication keep the layout per app in
    // QSettings; historyLength is how many samples a chart keeps.
    explicit DashboardModel(QString settingsOrganization, QString settingsApplication,
                            int historyLength = 90, QObject *parent = nullptr);

    QVariantList cards() const { return m_data; }
    QVariantList catalog() const { return m_catalog; }
    QStringList order() const { return m_order; }
    QString accent() const { return m_accent; }
    QString palette() const { return m_scheme; }
    double glass() const { return m_glass; }
    bool paused() const { return m_paused; }
    QVariantMap strip() const { return m_strip; }

    QString title() const { return m_title; }
    QString subtitle() const { return m_subtitle; }
    QString mark() const { return m_mark; }
    QString heading() const { return m_heading; }
    QString tagline() const { return m_tagline; }
    QString footer() const { return m_footer; }
    QString hint() const { return m_hint; }
    void setTitle(const QString &v) { if (m_title != v) { m_title = v; emit titleChanged(); } }
    void setSubtitle(const QString &v) { if (m_subtitle != v) { m_subtitle = v; emit titleChanged(); } }
    void setMark(const QString &v) { if (m_mark != v) { m_mark = v; emit titleChanged(); } }
    void setHeading(const QString &v) { if (m_heading != v) { m_heading = v; emit titleChanged(); } }
    void setTagline(const QString &v) { if (m_tagline != v) { m_tagline = v; emit titleChanged(); } }
    void setFooter(const QString &v) { if (m_footer != v) { m_footer = v; emit titleChanged(); } }
    void setHint(const QString &v) { if (m_hint != v) { m_hint = v; emit titleChanged(); } }

    void setPalette(QString v);
    void setGlass(double v);
    void setPaused(bool p);

    // Declares a metric: id, tile caption, default chart colour (empty means
    // "use the accent"), and whether it is visible on first run.
    void addMetric(const QString &id, const QString &label, const QString &unit,
                   const QString &color = {}, bool visibleByDefault = true);

    // One reading. level is what the chart plots; for percentage metrics pass
    // 0-100 and the chart scales to 100 automatically. available=false greys the
    // tile and prints "no data" instead of a value.
    void publish(const QString &id, const QString &value, const QString &unit,
                 const QString &detail, double level, bool available = true);

    // Replaces the bottom strip wholesale; an empty map hides it. See
    // StateStrip.qml for the keys.
    void setStrip(const QVariantMap &strip);

    Q_INVOKABLE void toggle(const QString &id);
    Q_INVOKABLE void move(const QString &id, int delta);
    Q_INVOKABLE void reset();
    Q_INVOKABLE QVariantMap card(const QString &id) const;

    // Charts keep historyLength samples; a percentage chart scales to 100, any
    // other metric scales to its own maximum.
    Q_INVOKABLE double ceiling(const QString &id) const;

    // Starts the sampling timer; the interval is in milliseconds.
    void start(int intervalMs = 1000);
    void stop();
    virtual void sample() {}

signals:
    void changed();
    void layoutChanged();
    void catalogChanged();
    void titleChanged();

protected:
    // Samples are taken from the timer and dropped while paused.
    void tick();
    QSettings m_settings;
    QVariantList m_catalog;

private:
    void save();
    QString resolveAccent() const;

    QTimer m_timer;
    QVariantList m_data;
    QStringList m_order;
    QMap<QString, QVariantList> m_history;
    QMap<QString, bool> m_percent;
    QMap<QString, QString> m_colors;
    QVariantMap m_strip;
    QString m_accent = "#31f7a5", m_scheme = "omarchy";
    double m_glass = .83;
    bool m_paused = false, m_sampling = false, m_savedLayout = false;
    int m_historyLength;
    QString m_title = "Dashboard", m_subtitle = "Live telemetry", m_mark = "Δ";
    QString m_heading = "Command center", m_tagline = "Everything you watch. One view.";
    QString m_footer = "LOCAL SENSORS  /  1 Hz";
    QString m_hint = "E  layout     SPACE  pause     ?  help";
};
