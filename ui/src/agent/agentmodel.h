#pragma once

#include <QElapsedTimer>
#include <QMap>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

// The agent console's data contract.
//
// It is a demo shell, not a model client: send() answers from a small canned
// script and emits tool events on a timer, so the UI can be seen streaming
// without an API key. Replace answerFor() and the tool events with a real
// backend and nothing in the QML has to change.
class AgentModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList sessions READ sessions NOTIFY sessionsChanged)
    Q_PROPERTY(int current READ current WRITE setCurrent NOTIFY sessionsChanged)
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)
    Q_PROPERTY(bool streaming READ streaming NOTIFY streamingChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QString modelLabel READ modelLabel NOTIFY modelChanged)
    Q_PROPERTY(QVariantList tools READ tools NOTIFY toolsChanged)
    Q_PROPERTY(double contextUsed READ contextUsed NOTIFY usageChanged)
    Q_PROPERTY(double contextMax READ contextMax NOTIFY usageChanged)
    Q_PROPERTY(int tokensIn READ tokensIn NOTIFY usageChanged)
    Q_PROPERTY(int tokensOut READ tokensOut NOTIFY usageChanged)
    Q_PROPERTY(double tokensPerSecond READ tokensPerSecond NOTIFY usageChanged)
    Q_PROPERTY(int lastLatency READ lastLatency NOTIFY usageChanged)
    Q_PROPERTY(int turns READ turns NOTIFY usageChanged)
    Q_PROPERTY(int toolRuns READ toolRuns NOTIFY usageChanged)
    Q_PROPERTY(int failures READ failures NOTIFY usageChanged)
    Q_PROPERTY(QVariantMap strip READ strip NOTIFY stripChanged)
    Q_PROPERTY(QString hint READ hint NOTIFY usageChanged)
    // Same look controls as the dashboard shell: theme accent and glass density.
    Q_PROPERTY(QString accent READ accent NOTIFY themeChanged)
    Q_PROPERTY(double glass READ glass NOTIFY themeChanged)

public:
    explicit AgentModel(QObject *parent = nullptr);

    QVariantList sessions() const { return m_sessions; }
    int current() const { return m_current; }
    QVariantList messages() const { return m_messages; }
    bool streaming() const { return m_streaming; }
    QString model() const { return m_model; }
    QString modelLabel() const;
    QVariantList tools() const { return m_tools; }
    double contextUsed() const { return m_contextUsed; }
    double contextMax() const { return m_contextMax; }
    int tokensIn() const { return m_tokensIn; }
    int tokensOut() const { return m_tokensOut; }
    double tokensPerSecond() const { return m_tokensPerSecond; }
    int lastLatency() const { return m_lastLatency; }
    int turns() const { return m_turns; }
    int toolRuns() const { return m_toolRuns; }
    int failures() const { return m_failures; }
    QVariantMap strip() const { return m_strip; }
    QString hint() const;
    QString accent() const { return m_accent; }
    double glass() const { return m_glass; }

    void setCurrent(int index);
    void setModel(const QString &model);

    // Delegates resolve their row by index: modelData is not reliable for a
    // QVariantList model, and a missing required property fails silently.
    Q_INVOKABLE QVariantMap messageAt(int index) const {
        return index >= 0 && index < m_messages.size() ? m_messages[index].toMap() : QVariantMap{};
    }

    Q_INVOKABLE void send(const QString &text);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void newSession();
    Q_INVOKABLE void pickSession(int index);

signals:
    void sessionsChanged();
    void messagesChanged();
    void streamingChanged();
    void modelChanged();
    void toolsChanged();
    void usageChanged();
    void stripChanged();
    void themeChanged();

private:
    struct Step {
        enum Kind { Tool, Text, Note } kind = Text;
        QString body;
        QString tool, status;
        int ms = 0;
    };
    void append(const QVariantMap &message);
    void finishTurn();
    void tick();
    void pushStrip();
    void queueTool(const QString &tool, int ms, const QString &status = "done");
    void queueText(const QString &text);
    QVariantMap makeSession(const QString &title, const QString &subtitle, const QString &updated,
                            const QString &dot) const;
    QString answerFor(const QString &prompt) const;

    QVariantList m_sessions, m_messages, m_tools;
    QMap<int, QVariantList> m_threads;   // per-session transcripts
    QVariantMap m_strip;
    QList<Step> m_steps;
    QTimer m_ticker;
    QElapsedTimer m_turnClock;
    enum Phase { Idle, ToolRunning, Streaming } m_phase = Idle;
    int m_toolRemaining = 0;
    QString m_pending;          // text not yet streamed out of the current step
    int m_current = 0, m_tokensIn = 0, m_tokensOut = 0, m_turns = 0;
    int m_toolRuns = 0, m_failures = 0, m_lastLatency = 0;
    double m_contextUsed = 0, m_contextMax = 32768, m_tokensPerSecond = 0;
    bool m_streaming = false;
    QString m_model = "local/astra-agent-8b", m_accent = "#31f7a5";
    double m_glass = .83;
    QVariantList m_scoreHistory, m_latencyHistory;
};
