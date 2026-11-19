#include "agentmodel.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTime>
#include <QLocale>
#include <algorithm>
#include <cmath>

namespace {
// Four characters per token is the usual rough English estimate; it is enough
// to make the counters move in a demo, not an accounting tool.
int estimateTokens(const QString &text) { return std::max(1, int(text.size() / 4)); }

QString now() { return QLocale().toString(QTime::currentTime(), "HH:mm"); }

int chunkSize() { return 4; }
} // namespace

AgentModel::AgentModel(QObject *parent) : QObject(parent) {
    // Follow the desktop accent so the console matches the rest of the session.
    QFile theme(QDir::homePath() + "/.local/state/omarchy/current/theme/colors.toml");
    if (theme.open(QIODevice::ReadOnly)) {
        const auto match = QRegularExpression(QStringLiteral("accent\\s*=\\s*\"(#[0-9a-fA-F]{6})\""))
                               .match(QString::fromUtf8(theme.readAll()));
        if (match.hasMatch())
            m_accent = match.captured(1);
    }
    m_tools = QVariantList{
        QVariantMap{{"name", "read_file"}, {"runs", 0}, {"enabled", true}},
        QVariantMap{{"name", "run_tests"}, {"runs", 0}, {"enabled", true}},
        QVariantMap{{"name", "write_patch"}, {"runs", 0}, {"enabled", true}},
        QVariantMap{{"name", "search_web"}, {"runs", 0}, {"enabled", false}},
    };
    m_current = 0;
    m_sessions = QVariantList{
        makeSession(tr("Agent console"), tr("%1 · demo script").arg(m_model), tr("now"), "active"),
        makeSession(tr("Paradox network review"), tr("turns 12 · tools 2"), tr("17:12"), "idle"),
        makeSession(tr("ASTRA tile refactor"), tr("turns 8 · tools 1"), tr("16:40"), "idle"),
    };
    m_threads[0] = QVariantList{
        QVariantMap{{"role", "note"},
                    {"text", tr("Live shell · replies come from a canned script, tool events are simulated.")},
                    {"time", now()}},
        QVariantMap{{"role", "agent"},
                    {"text", tr("Console ready. I am wired to the local runtime and four tools. Ask me to inspect the repo, run the test suite or summarise a change.")},
                    {"time", now()}},
    };
    m_threads[1] = QVariantList{
        QVariantMap{{"role", "user"}, {"text", tr("Check the WAN latency spikes from last night.")}, {"time", "17:06"}},
        QVariantMap{{"role", "agent"}, {"text", tr("Three spikes between 02:10 and 02:26, all on the same uplink. Governor stayed in degraded, no action taken.")}, {"time", "17:12"}},
    };
    m_threads[2] = QVariantList{
        QVariantMap{{"role", "user"}, {"text", tr("Split the tile drawing out of Main.qml.")}, {"time", "16:38"}},
        QVariantMap{{"role", "agent"}, {"text", tr("Done: TelemetryTile.qml now owns the sparkline and the edit controls. Tests still pass.")}, {"time", "16:40"}},
    };
    m_messages = m_threads.value(0);

    m_ticker.setInterval(24);
    connect(&m_ticker, &QTimer::timeout, this, &AgentModel::tick);
}

// One tick of the scripted turn: run the tool row, stream a chunk, or start the
// next step. Keeping it in one place means a queued tool or a second paragraph
// cannot leave the turn half-finished.
void AgentModel::tick() {
    if (m_phase == ToolRunning) {
        m_toolRemaining -= m_ticker.interval();
        if (m_toolRemaining > 0)
            return;
        for (int i = m_messages.size() - 1; i >= 0; --i) {
            QVariantMap row = m_messages[i].toMap();
            if (row.value("role").toString() != "tool" || row.value("status").toString() != "running")
                continue;
            row["status"] = "done";
            row["detail"] = tr("%1 ms").arg(row.value("ms").toInt());
            m_messages[i] = row;
            emit messagesChanged();
            break;
        }
        ++m_toolRuns;
        m_phase = Idle;
        emit usageChanged();
        return;
    }

    if (!m_pending.isEmpty()) {
        const QString chunk = m_pending.left(chunkSize());
        m_pending.remove(0, chunk.size());
        QVariantMap row = m_messages.last().toMap();
        row["text"] = row.value("text").toString() + chunk;
        row["streaming"] = !m_pending.isEmpty();
        m_messages[m_messages.size() - 1] = row;
        ++m_tokensOut;
        emit messagesChanged();
        emit usageChanged();
        return;
    }

    if (m_steps.isEmpty()) {
        if (m_streaming)
            finishTurn();
        else
            m_ticker.stop();
        return;
    }

    const Step next = m_steps.takeFirst();
    if (next.kind == Step::Tool) {
        append(QVariantMap{{"role", "tool"},
                           {"tool", next.tool},
                           {"status", "running"},
                           {"ms", next.ms},
                           {"time", now()}});
        m_toolRemaining = next.ms;
        m_phase = ToolRunning;
    } else if (next.kind == Step::Note) {
        append(QVariantMap{{"role", "note"}, {"text", next.body}, {"time", now()}});
    } else {
        append(QVariantMap{{"role", "agent"}, {"text", ""}, {"time", now()}});
        m_pending = next.body;
        m_phase = Streaming;
    }
}

QString AgentModel::modelLabel() const {
    QString shortName = m_model.section('/', -1);
    return shortName;
}

QString AgentModel::hint() const {
    return tr("ENTER  send     ESC  stop     N  new session");
}

QVariantMap AgentModel::makeSession(const QString &title, const QString &subtitle,
                                    const QString &updated, const QString &dot) const {
    return QVariantMap{{"title", title}, {"subtitle", subtitle}, {"updated", updated}, {"dot", dot}};
}

void AgentModel::append(const QVariantMap &message) {
    m_messages.append(message);
    m_threads[m_current] = m_messages;
    emit messagesChanged();
}

void AgentModel::queueTool(const QString &tool, int ms, const QString &status) {
    Step step;
    step.kind = Step::Tool;
    step.tool = tool;
    step.status = status;
    step.ms = ms;
    m_steps.append(step);
}

void AgentModel::queueText(const QString &text) {
    Step step;
    step.kind = Step::Text;
    step.body = text;
    m_steps.append(step);
}

void AgentModel::send(const QString &text) {
    const QString prompt = text.trimmed();
    if (prompt.isEmpty())
        return;
    stop();
    append(QVariantMap{{"role", "user"}, {"text", prompt}, {"time", now()}});
    m_contextUsed = std::min(m_contextMax, m_contextUsed + estimateTokens(prompt));
    m_tokensIn += estimateTokens(prompt);
    ++m_turns;
    m_turnClock.restart();
    m_steps.clear();

    // A tiny script that decides which tool rows to show before the answer.
    const QString lower = prompt.toLower();
    if (lower.contains("test"))
        queueTool("run_tests", 720);
    if (lower.contains("patch") || lower.contains("fix") || lower.contains("refactor"))
        queueTool("write_patch", 480);
    if (lower.contains("read") || lower.contains("check") || lower.contains("look"))
        queueTool("read_file", 260);
    queueText(answerFor(prompt));
    if (lower.contains("test"))
        queueText(QStringLiteral("\n\n") + tr("All 11 cases pass. I did not touch any source file this turn."));

    m_streaming = true;
    emit streamingChanged();
    m_phase = Idle;
    m_pending.clear();
    m_ticker.start();
}

void AgentModel::stop() {
    if (!m_streaming && m_phase == Idle)
        return;
    m_steps.clear();
    m_pending.clear();
    m_phase = Idle;
    m_ticker.stop();
    if (!m_messages.isEmpty()) {
        QVariantMap row = m_messages.last().toMap();
        if (row.value("role").toString() == "agent") {
            row["streaming"] = false;
            m_messages[m_messages.size() - 1] = row;
        }
    }
    if (m_streaming) {
        m_streaming = false;
        emit streamingChanged();
    }
    emit messagesChanged();
}

void AgentModel::newSession() {
    stop();
    m_sessions.append(makeSession(tr("New session"), tr("turns 0 · tools 0"), tr("now"), "active"));
    for (int i = 0; i < m_sessions.size() - 1; ++i) {
        QVariantMap s = m_sessions[i].toMap();
        s["dot"] = "idle";
        m_sessions[i] = s;
    }
    m_current = m_sessions.size() - 1;
    m_messages = QVariantList{QVariantMap{{"role", "note"},
                                          {"text", tr("New session. Context is empty; the demo script answers the same way everywhere.")},
                                          {"time", now()}}};
    m_threads[m_current] = m_messages;
    m_contextUsed = 0;
    emit sessionsChanged();
    emit messagesChanged();
    emit usageChanged();
}

void AgentModel::pickSession(int index) {
    if (index < 0 || index >= m_sessions.size() || index == m_current)
        return;
    stop();
    m_threads[m_current] = m_messages;
    m_current = index;
    m_messages = m_threads.value(index);
    for (int i = 0; i < m_sessions.size(); ++i) {
        QVariantMap s = m_sessions[i].toMap();
        s["dot"] = i == index ? "active" : "idle";
        m_sessions[i] = s;
    }
    emit sessionsChanged();
    emit messagesChanged();
}

void AgentModel::setCurrent(int index) { pickSession(index); }

void AgentModel::setModel(const QString &model) {
    if (m_model == model)
        return;
    m_model = model;
    emit modelChanged();
}

void AgentModel::finishTurn() {
    m_streaming = false;
    m_phase = Idle;
    m_lastLatency = int(m_turnClock.elapsed());
    m_tokensPerSecond = m_tokensOut > 0 && m_lastLatency > 0
                            ? m_tokensOut * 1000.0 / std::max(1, m_lastLatency) : 0;
    m_contextUsed = std::min(m_contextMax, m_contextUsed + m_tokensOut);
    QVariantMap s = m_sessions[m_current].toMap();
    s["subtitle"] = tr("turns %1 · tools %2").arg(m_turns).arg(m_toolRuns);
    s["updated"] = tr("now");
    m_sessions[m_current] = s;
    emit sessionsChanged();
    emit streamingChanged();
    emit usageChanged();
    pushStrip();
}

void AgentModel::pushStrip() {
    const double contextPct = m_contextMax > 0 ? 100 * m_contextUsed / m_contextMax : 0;
    // A deliberately simple Governor: start at 100 and take points away for
    // context pressure and tool failures, so the strip has something honest to
    // show without inventing a health number.
    double score = 100 - std::max(0.0, contextPct - 50) * .8 - m_failures * 6;
    score = std::clamp(score, 0.0, 100.0);
    const bool critical = score < 50, degraded = score < 75;
    m_scoreHistory.append(score);
    m_latencyHistory.append(m_lastLatency);
    while (m_scoreHistory.size() > 90) m_scoreHistory.removeFirst();
    while (m_latencyHistory.size() > 90) m_latencyHistory.removeFirst();

    m_strip = QVariantMap{
        {"ready", true},
        {"glyph", "\u03A0"},
        {"title", tr("TASK GOVERNOR")},
        {"subtitle", tr("Agent runtime \u00B7 dry run")},
        {"score", score},
        {"scoreSuffix", "/100"},
        {"state", critical ? "critical" : degraded ? "degraded" : "healthy"},
        {"stateLabel", critical ? tr("CRITICAL") : degraded ? tr("DEGRADED") : tr("HEALTHY")},
        {"statusDetail", critical ? tr("Context pressure high \u00B7 compaction recommended")
                         : degraded ? tr("Tools failed this session \u00B7 watching")
                         : tr("Context and tool budget within limits")},
        {"metrics", QVariantList{
             QVariantMap{{"k", tr("CONTEXT")}, {"v", QString::number(contextPct, 'f', 0)}, {"u", "%"}},
             QVariantMap{{"k", tr("TOKENS/S")}, {"v", QString::number(m_tokensPerSecond, 'f', 1)}, {"u", ""}},
             QVariantMap{{"k", tr("TOOLS")}, {"v", QString::number(m_toolRuns)}, {"u", ""}},
             QVariantMap{{"k", tr("FAILURES")}, {"v", QString::number(m_failures)}, {"u", ""}},
         }},
        {"note", critical ? tr("Governor L3 \u2192 compact_context \u00B7 not executed (dry run)")
                 : degraded ? tr("Governor L2 \u2192 retry_tools \u00B7 not executed (dry run)")
                 : tr("Governor: no recommendation")},
        {"noteTone", critical ? "danger" : degraded ? "warning" : "idle"},
        {"series", m_scoreHistory},
        {"seriesMax", 100},
        {"seriesColor", "#6be38b"},
        {"seriesLabel", tr("HEALTH")},
        {"overlay", m_latencyHistory},
        {"overlayCeiling", std::max(1200.0, std::ceil(m_lastLatency * 1.2 / 100) * 100)},
        {"overlayColor", "#4fc3ff"},
        {"overlayLabel", tr("LATENCY")},
        {"overlayUnit", "ms"},
        {"window", tr("last 90 turns")},
        {"footer", QVariantList{
             QVariantMap{{"k", tr("TURN #%1").arg(m_turns)}, {"tip", tr("Turns this session")}},
             QVariantMap{{"k", m_model}, {"tip", tr("Active model")}},
         }},
    };
    emit stripChanged();
}

QString AgentModel::answerFor(const QString &prompt) const {
    const QString lower = prompt.toLower();
    if (lower.contains("test"))
        return tr("Ran the suite in the workspace root.");
    if (lower.contains("patch") || lower.contains("fix") || lower.contains("refactor"))
        return tr("I patched the smallest thing that made the test pass, then re-read the diff. "
                  "Nothing outside the reported file changed.");
    if (lower.contains("status") || lower.contains("summary") || lower.contains("summar"))
        return tr("Six tiles, one strip, a translator and eleven tests. The shell is stable; "
                  "only the feed and the question counters moved since the last turn.");
    if (lower.contains("latency") || lower.contains("network") || lower.contains("paradox"))
        return tr("The uplink moved between 14 and 46 ms with no loss. That is inside the "
                  "Governor window, so nothing was armed.");
    if (lower.contains("hello") || lower.contains("hi") || lower.contains("cze"))
        return tr("Here. Give me a file, a failing test or a symptom and I will start there.");
    return tr("Understood. In the demo this is a canned answer: %1. Point AgentModel::answerFor() "
              "at your own runtime and the console needs no changes.")
        .arg(prompt.left(60));
}
