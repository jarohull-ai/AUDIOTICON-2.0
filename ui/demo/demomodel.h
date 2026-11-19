#pragma once

#include <QRandomGenerator>
#include <cmath>

#include "dashboardmodel.h"

// A self-contained demo feed, so the template builds and runs without any
// backend. Replace this class with your own DashboardModel subclass.
class DemoModel : public DashboardModel {
    Q_OBJECT
public:
    explicit DemoModel(QObject *parent = nullptr)
        : DashboardModel("astra-ui-template-demo", "astra-ui-template-demo", 90, parent) {
        setMark("\u039B");
        setTitle("A S T R A");
        setSubtitle("UI template / demo feed");
        setHeading("Command center");
        setTagline("Synthetic data. Swap in your own model and nothing else changes.");
        setFooter("DEMO FEED  /  1 Hz");
        addMetric("latency", tr("API latency"), "ms", "#4fc3ff");
        addMetric("throughput", tr("Throughput"), "MiB/s", "#2fe0d0");
        addMetric("errors", tr("Error rate"), "%", "#ff6fae");
        addMetric("queue", tr("Queue depth"), "jobs", "#b18cff");
        addMetric("workers", tr("Worker load"), "%", "#6be38b");
        addMetric("cache", tr("Cache hit rate"), "%", "#ff9f43");
        // A screenshot wants a full chart, so bin/screenshot asks for a faster feed.
        start(qEnvironmentVariable("ASTRA_UI_DEMO_INTERVAL", "1000").toInt());
    }

    void sample() override {
        ++m_step;
        const double t = m_step / 10.0;
        auto wave = [&](double base, double amplitude, double period, double noise) {
            return base + amplitude * std::sin(t * 2 * M_PI / period)
                   + noise * (m_random.generateDouble() - .5) * 2;
        };

        const double latency = std::max(1.0, wave(38, 14, 9, 6));
        const double throughput = std::max(0.0, wave(9.4, 3.1, 7, .8));
        const double errors = std::max(0.0, wave(.42, .35, 13, .12));
        const double queue = std::max(0.0, wave(24, 18, 17, 4));

        // Every ninth of the run the demo degrades on purpose: the strip turns
        // amber, then red, so the state colours can be seen without a real incident.
        const int phase = (m_step / 9) % 4;          // healthy, healthy, degraded, critical
        const bool degraded = phase == 2, critical = phase == 3;
        if (critical) {
            m_errors.push_back(2.4);
            m_health.push_back(31);
        } else if (degraded) {
            m_errors.push_back(.9);
            m_health.push_back(72);
        } else {
            m_errors.push_back(.35);
            m_health.push_back(97);
        }
        while (m_health.size() > 90) m_health.removeFirst();
        while (m_errors.size() > 90) m_errors.removeFirst();

        const double health = m_health.last();
        const QString state = critical ? "critical" : degraded ? "degraded" : "healthy";

        const double workerLoad = std::clamp(wave(46, 12, 11, 5) + (critical ? 28.0 : degraded ? 14.0 : 0.0), 0.0, 100.0);
        const double cache = std::clamp(wave(92, 3.5, 15, 1.5) - (critical ? 9.0 : degraded ? 4.0 : 0.0), 0.0, 100.0);

        publish("latency", QString::number(latency, 'f', 1), "ms",
                tr("p95 over the last minute"), latency);
        publish("throughput", QString::number(throughput, 'f', 2), "MiB/s",
                tr("\u2191 %1 MiB/s out \u00B7 3 endpoints").arg(throughput * .3, 0, 'f', 2), throughput);
        publish("errors", QString::number(critical ? 2.4 : degraded ? .9 : errors, 'f', 2), "%",
                tr("%1 requests in the last minute").arg(1200 + m_step * 3), m_errors.last());
        publish("queue", QString::number(queue, 'f', 0), "jobs",
                tr("4 workers \u00B7 oldest job %1 s").arg(queue * .7, 0, 'f', 0), queue);
        publish("workers", QString::number(workerLoad, 'f', 0), "%",
                tr("mean load across the worker pool"), workerLoad);
        publish("cache", QString::number(cache, 'f', 1), "%",
                tr("hit rate \u00B7 512 MiB hot set"), cache);

        setStrip(QVariantMap{
            {"ready", true},
            {"glyph", "\u03A0"},
            {"title", "SERVICE GOVERNOR"},
            {"subtitle", tr("Demo policy \u00B7 dry run")},
            {"score", health},
            {"scoreSuffix", "/100"},
            {"state", state},
            {"stateLabel", critical ? tr("CRITICAL") : degraded ? tr("DEGRADED") : tr("HEALTHY")},
            {"statusDetail", critical ? tr("Error budget burned \u00B7 escalation pending")
                             : degraded ? tr("Elevated error rate \u00B7 watching")
                             : tr("All SLO thresholds within limits")},
            {"metrics", QVariantList{
                 QVariantMap{{"k", "LATENCY"}, {"v", QString::number(latency, 'f', 1)}, {"u", "ms"}},
                 QVariantMap{{"k", "ERRORS"}, {"v", QString::number(m_errors.last(), 'f', 2)}, {"u", "%"}},
                 QVariantMap{{"k", "QUEUE"}, {"v", QString::number(queue, 'f', 0)}, {"u", "jobs"}},
                 QVariantMap{{"k", "HIT RATE"}, {"v", QString::number(cache, 'f', 0)}, {"u", "%"}},
             }},
            {"note", critical ? tr("Supervisor L3 \u2192 shed_load \u00B7 not executed (dry run)")
                     : degraded ? tr("Supervisor L2 \u2192 widen_workers \u00B7 not executed (dry run)")
                     : tr("Supervisor: no recommendation")},
            {"noteTone", critical ? "danger" : degraded ? "warning" : "idle"},
            {"series", varianted(m_health)},
            {"seriesMax", 100},
            {"seriesColor", "#6be38b"},
            {"seriesLabel", tr("HEALTH")},
            {"overlay", varianted(m_errors)},
            {"overlayCeiling", 3},
            {"overlayUnit", "%"},
            {"overlayColor", "#4fc3ff"},
            {"overlayLabel", tr("ERRORS")},
            {"window", tr("1.5 min")},
            {"footer", QVariantList{
                 QVariantMap{{"k", tr("POLICY demo-1")}, {"tip", tr("Governor thresholds")}},
                 QVariantMap{{"k", tr("STEP #%1").arg(m_step)}, {"tip", tr("Sample counter")}},
             }},
        });
    }

private:
    // QML iterates a QVariantList of numbers most reliably.
    static QVariantList varianted(const QList<double> &values) {
        QVariantList list;
        list.reserve(values.size());
        for (double v : values) list.append(v);
        return list;
    }
    QRandomGenerator m_random{20261117};
    QList<double> m_health, m_errors;
    int m_step = 0;
};
