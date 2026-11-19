#include "dashboardmodel.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <algorithm>

static QByteArray readFile(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

DashboardModel::DashboardModel(QString settingsOrganization, QString settingsApplication,
                               int historyLength, QObject *parent)
    : QObject(parent),
      m_settings(std::move(settingsOrganization), std::move(settingsApplication)),
      m_historyLength(std::max(2, historyLength)) {
    m_glass = std::clamp(m_settings.value("glass", .83).toDouble(), .45, .98);
    m_scheme = m_settings.value("palette", "omarchy").toString();
    // A saved layout wins; on first run every metric defaults on or off as
    // addMetric() asked, so no tiles need to exist before the app first runs.
    m_savedLayout = m_settings.contains("tiles");
    m_order = m_settings.value("tiles").toStringList();
    m_accent = resolveAccent();
    connect(&m_timer, &QTimer::timeout, this, &DashboardModel::tick);
}

void DashboardModel::addMetric(const QString &id, const QString &label, const QString &unit,
                               const QString &color, bool visibleByDefault) {
    m_catalog.append(QVariantMap{{"id", id}, {"label", label}, {"unit", unit}});
    if (!color.isEmpty())
        m_colors.insert(id, color);
    if (!m_savedLayout && visibleByDefault && !m_order.contains(id))
        m_order.append(id);
    emit catalogChanged();
}

void DashboardModel::start(int intervalMs) {
    m_sampling = true;
    m_timer.start(std::max(50, intervalMs));
    tick();
}

void DashboardModel::stop() {
    m_sampling = false;
    m_timer.stop();
}

void DashboardModel::tick() {
    if (!m_sampling || m_paused)
        return;
    m_data.clear();
    m_accent = resolveAccent();
    sample();
    emit changed();
}

void DashboardModel::publish(const QString &id, const QString &value, const QString &unit,
                             const QString &detail, double level, bool available) {
    auto &history = m_history[id];
    // A percentage metric charts against a fixed 0-100 axis; anything else
    // scales to its own maximum so a small series is still legible.
    m_percent.insert(id, unit == "%");
    if (available) {
        history.append(level);
        while (history.size() > m_historyLength)
            history.removeFirst();
    }
    QString label;
    for (const auto &entry : m_catalog) {
        const QVariantMap metric = entry.toMap();
        if (metric.value("id").toString() == id) {
            label = metric.value("label").toString();
            break;
        }
    }
    // One entry per metric: a repeated publish replaces it, so card() always
    // returns the newest reading and the history in it stays continuous.
    const QVariantMap entry{{"id", id},
                            {"label", label},
                            {"value", available ? value : QString::fromUtf8("\u2014")},
                            {"unit", available ? unit : QString()},
                            {"detail", detail},
                            {"level", level},
                            {"available", available},
                            {"color", m_colors.value(id)},
                            {"history", history}};
    for (auto &existing : m_data) {
        if (existing.toMap().value("id").toString() == id) {
            existing = entry;
            return;
        }
    }
    m_data.append(entry);
}

QVariantMap DashboardModel::card(const QString &id) const {
    for (const auto &entry : m_data)
        if (entry.toMap().value("id").toString() == id)
            return entry.toMap();
    return {};
}

double DashboardModel::ceiling(const QString &id) const {
    if (m_percent.value(id, false))
        return 100;
    double max = 0;
    for (const auto &sample : m_history.value(id))
        max = std::max(max, sample.toDouble());
    return max > 0 ? max * 1.2 : 1;
}

QString DashboardModel::resolveAccent() const {
    static const QRegularExpression hex("^#[0-9a-fA-F]{6}$");
    if (m_scheme == "ice")
        return "#70ddff";
    if (m_scheme == "amber")
        return "#ffbb66";
    if (hex.match(m_scheme).hasMatch())
        return m_scheme;
    // "omarchy" (the default) follows the desktop theme live, so the dashboard
    // repaints when the user switches themes instead of pinning one colour.
    const QString theme = QString::fromUtf8(
        readFile(QDir::homePath() + "/.local/state/omarchy/current/theme/colors.toml"));
    const auto match = QRegularExpression("accent\\s*=\\s*\"(#[0-9a-fA-F]{6})\"").match(theme);
    return match.hasMatch() ? match.captured(1) : QStringLiteral("#31f7a5");
}

void DashboardModel::setStrip(const QVariantMap &strip) {
    m_strip = strip;
    emit changed();
}

void DashboardModel::save() {
    m_settings.setValue("tiles", m_order);
    m_settings.setValue("glass", m_glass);
    m_settings.setValue("palette", m_scheme);
    m_settings.sync();
    emit layoutChanged();
}

void DashboardModel::setGlass(double v) {
    m_glass = std::clamp(v, .45, .98);
    save();
}

void DashboardModel::setPalette(QString v) {
    m_scheme = v;
    // Resolve immediately so the whole window recolours without waiting a tick.
    m_accent = resolveAccent();
    save();
    emit changed();
    if (m_sampling)
        tick();
}

void DashboardModel::setPaused(bool p) {
    if (m_paused == p)
        return;
    m_paused = p;
    if (m_paused) {
        m_data.clear();
        emit changed();
    } else if (m_sampling) {
        tick();
    }
}

void DashboardModel::toggle(const QString &id) {
    if (m_order.contains(id)) {
        m_order.removeAll(id);
    } else {
        for (const auto &entry : m_catalog)
            if (entry.toMap().value("id").toString() == id)
                m_order.append(id);
    }
    save();
}

void DashboardModel::move(const QString &id, int delta) {
    const int from = m_order.indexOf(id), to = from + delta;
    if (from >= 0 && to >= 0 && to < m_order.size()) {
        m_order.move(from, to);
        save();
    }
}

void DashboardModel::reset() {
    m_order.clear();
    for (const auto &entry : m_catalog)
        m_order.append(entry.toMap().value("id").toString());
    m_glass = .83;
    m_scheme = "omarchy";
    m_history.clear();
    save();
    if (m_sampling)
        tick();
}
