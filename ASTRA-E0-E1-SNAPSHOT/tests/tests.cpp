#include <QtTest>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QTemporaryDir>

#include "agentmodel.h"
#include "dashboardmodel.h"
#include "translator.h"
#include "voice/DirectiveParser.h"
#include "voice/PolishVerbalizer.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

// A minimal model: two percentage metrics and one that is not a percentage,
// so both chart axes get exercised.
class TestModel : public DashboardModel {
public:
    TestModel() : DashboardModel("astra-ui-test", "astra-ui-test") {
        addMetric("cpu", "Processor", "%", "#4fc3ff");
        addMetric("mem", "Memory", "%", "#b18cff");
        addMetric("rate", "Rate", "MiB/s", "#2fe0d0", false);
        sample();
    }
    void sample() override {
        publish("cpu", "42.0", "%", "detail", 42.0);
        publish("mem", "18.0", "%", QString(), 18.0);
        publish("rate", "3.20", "MiB/s", QString(), 3.2);
    }
};

class Tests : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_dir;

    // Every test gets an empty settings scope, so a previous run cannot decide
    // what this run considers the default layout.
    void freshScope() { QSettings("astra-ui-test", "astra-ui-test").clear(); }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_dir.path());
        // Must happen before any QML engine imports Qt Quick Controls.
        QQuickStyle::setStyle("Material");
    }

    void catalogAndDefaults() {
        freshScope();
        TestModel m;
        QCOMPARE(m.catalog().size(), 3);
        // visibleByDefault=false keeps "rate" out of the first-run layout.
        QCOMPARE(m.order(), QStringList({"cpu", "mem"}));
    }

    void toggleMoveReset() {
        freshScope();
        TestModel m;
        m.toggle("rate");
        QCOMPARE(m.order(), QStringList({"cpu", "mem", "rate"}));
        m.move("rate", -1);
        QCOMPARE(m.order(), QStringList({"cpu", "rate", "mem"}));
        m.toggle("cpu");
        QVERIFY(!m.order().contains("cpu"));
        m.reset();
        QCOMPARE(m.order(), QStringList({"cpu", "mem", "rate"}));
    }

    void chartsAxes() {
        freshScope();
        TestModel m;
        // A percentage chart is pinned to 0-100; anything else follows the data.
        QCOMPARE(m.ceiling("cpu"), 100.0);
        QCOMPARE(m.ceiling("rate"), 3.2 * 1.2);
        QCOMPARE(m.card("cpu").value("value").toString(), QString("42.0"));
        QCOMPARE(m.card("cpu").value("history").toList().size(), 1);
    }

    void historyIsBounded() {
        QTemporaryDir dir;
        DashboardModel m("astra-ui-test-2", dir.path() + "/settings");
        m.addMetric("x", "X", "%");
        for (int i = 0; i < 120; i++)
            m.publish("x", QString::number(i), "%", QString(), i);
        QCOMPARE(m.card("x").value("history").toList().size(), 90);
    }

    void unavailableGreysOut() {
        QTemporaryDir dir;
        DashboardModel m("astra-ui-test-3", dir.path() + "/settings");
        m.addMetric("x", "X", "%");
        m.publish("x", "", "", "sensor missing", 0, false);
        const QVariantMap card = m.card("x");
        QCOMPARE(card.value("available").toBool(), false);
        QCOMPARE(card.value("value").toString(), QString::fromUtf8("\u2014"));
        QCOMPARE(card.value("unit").toString(), QString());
    }

    void persistence() {
        QTemporaryDir dir;
        {
            DashboardModel m("astra-ui-test-4", dir.path() + "/settings");
            m.addMetric("a", "A", "%");
            m.addMetric("b", "B", "%", QString(), false);
            m.toggle("b");
            m.setGlass(.6);
            m.setPalette("ice");
            m.move("b", -1);
        }
        DashboardModel restored("astra-ui-test-4", dir.path() + "/settings");
        restored.addMetric("a", "A", "%");
        restored.addMetric("b", "B", "%", QString(), false);
        QCOMPARE(restored.order(), QStringList({"b", "a"}));
        QCOMPARE(restored.glass(), .6);
        QCOMPARE(restored.accent(), QString("#70ddff"));
    }

    void stripRoundTrip() {
        freshScope();
        TestModel m;
        QVERIFY(m.strip().isEmpty());
        m.setStrip(QVariantMap{{"ready", true}, {"score", 97}, {"state", "healthy"}});
        QCOMPARE(m.strip().value("score").toInt(), 97);
        QCOMPARE(m.strip().value("state").toString(), QString("healthy"));
    }

    void languageSwitch() {
        QTemporaryDir dir;
        Translator t(dir.path() + "/language");
        QCoreApplication::installTranslator(&t);
        t.setChoice("pl");
        QCOMPARE(t.language(), QString("pl"));
        QCOMPARE(QCoreApplication::translate("x", "Edit layout"), QString("Edytuj układ"));
        QCOMPARE(QCoreApplication::translate("x", "HISTORY / 90 s"), QString("HISTORIA / 90 s"));
        t.setChoice("en");
        QCOMPARE(QCoreApplication::translate("x", "Edit layout"), QString("Edit layout"));
        t.setChoice("auto");
        QCOMPARE(t.language(), QString("en"));
        t.setChoice("not a code");
        QCOMPARE(t.choice(), QString("auto"));
        QCoreApplication::removeTranslator(&t);
    }

    void rendersAndResponds() {
        freshScope();
        TestModel model;
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language");
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("feed", &model);
        engine.rootContext()->setContextProperty("i18n", &translator);
        engine.load(QUrl("qrc:/qml/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QTest::qWait(200);
        // The shell's own shortcuts: edit mode, pause.
        QTest::keyClick(window, Qt::Key_E);
        QCOMPARE(window->property("editing").toBool(), true);
        QTest::keyClick(window, Qt::Key_Space);
        QVERIFY(model.paused());
        // It must still draw when squeezed to the minimum size.
        window->resize(720, 600);
        QTest::qWait(150);
        QVERIFY(!window->grabWindow().isNull());
    }


    // ---- voice core (ported from the donor) -------------------------------

    // The C++ port has to agree with Portable Local Studio word for word: the
    // golden file is generated from the donor module by tests/generate-golden.cjs.
    void directiveGolden() {
        QFile file(QStringLiteral(":/golden/tts-golden.json"));
        QVERIFY2(file.open(QIODevice::ReadOnly), "tests/data/tts-golden.json is missing from the resources");
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QVERIFY(doc.isObject());
        const QJsonArray cases = doc.object().value("cases").toArray();
        QVERIFY2(cases.size() >= 40, "the golden set shrank");

        for (const QJsonValue &value : cases) {
            const QJsonObject c = value.toObject();
            const QString name = c.value("name").toString();
            const QString input = c.value("input").toString();
            // QVERIFY2 keeps the pointer, so the text has to outlive the call.
            static QByteArray messageBuffer;
            const auto message = [&name](const QString &what, const QString &got, const QString &want) {
                messageBuffer = QStringLiteral("%1 / %2: got [%3], want [%4]")
                                    .arg(name, what, got, want).toUtf8();
                return messageBuffer.constData();
            };

            QVERIFY2(audioticon::verbalizePolish(input) == c.value("verbalized").toString(),
                     message("verbalizePolish", audioticon::verbalizePolish(input),
                             c.value("verbalized").toString()));
            QCOMPARE(audioticon::hasSpeakableText(input), c.value("speakable").toBool());
            QVERIFY2(audioticon::preparePlainText(input) == c.value("plain").toString(),
                     message("preparePlainText", audioticon::preparePlainText(input),
                             c.value("plain").toString()));

            const audioticon::DirectedText parsed = audioticon::parseDirectedText(input);
            QVERIFY2(parsed.text == c.value("text").toString(),
                     message("text", parsed.text, c.value("text").toString()));
            QCOMPARE(parsed.leadingPauseMs, c.value("leadingPauseMs").toInt());

            const QJsonArray segments = c.value("segments").toArray();
            QVERIFY2(parsed.segments.size() == segments.size(),
                     message("segment count", QString::number(parsed.segments.size()),
                             QString::number(segments.size())));
            for (int i = 0; i < segments.size(); ++i) {
                const QJsonObject want = segments.at(i).toObject();
                const audioticon::VoiceSegment got = parsed.segments.at(i);
                QVERIFY2(got.text == want.value("text").toString(),
                         message(QStringLiteral("segment %1 text").arg(i), got.text,
                                 want.value("text").toString()));
                QVERIFY2(qAbs(got.speed - want.value("speed").toDouble()) < 1e-9,
                         message(QStringLiteral("segment %1 speed").arg(i), QString::number(got.speed),
                                 QString::number(want.value("speed").toDouble())));
                QVERIFY2(got.dsp == want.value("dsp").toString(),
                         message(QStringLiteral("segment %1 dsp").arg(i), got.dsp,
                                 want.value("dsp").toString()));
                QVERIFY2(got.pauseMs == want.value("pauseMs").toInt(),
                         message(QStringLiteral("segment %1 pause").arg(i),
                                 QString::number(got.pauseMs),
                                 QString::number(want.value("pauseMs").toInt())));
            }

            const QJsonArray chapters = c.value("chapters").toArray();
            QVERIFY2(parsed.chapters.size() == chapters.size(),
                     message("chapter count", QString::number(parsed.chapters.size()),
                             QString::number(chapters.size())));
            for (int i = 0; i < chapters.size(); ++i) {
                const QJsonObject want = chapters.at(i).toObject();
                QCOMPARE(parsed.chapters.at(i).index, want.value("index").toInt());
                QVERIFY2(parsed.chapters.at(i).title == want.value("title").toString(),
                         message(QStringLiteral("chapter %1 title").arg(i), parsed.chapters.at(i).title,
                                 want.value("title").toString()));
            }

            const QJsonArray directives = c.value("directives").toArray();
            QVERIFY2(parsed.directives.size() == directives.size(),
                     message("directive count", QString::number(parsed.directives.size()),
                             QString::number(directives.size())));
            for (int i = 0; i < directives.size(); ++i) {
                const QJsonObject want = directives.at(i).toObject();
                const QVariantMap got = parsed.directives.at(i).toMap();
                QVERIFY2(got.value("kind").toString() == want.value("kind").toString(),
                         message(QStringLiteral("directive %1 kind").arg(i),
                                 got.value("kind").toString(), want.value("kind").toString()));
                QCOMPARE(got.value("pauseMs").toInt(), want.value("pauseMs").toInt());
                QVERIFY2(got.value("dsp").toString() == want.value("dsp").toString(),
                         message(QStringLiteral("directive %1 dsp").arg(i),
                                 got.value("dsp").toString(), want.value("dsp").toString()));
            }
        }
    }

    // The palette comes from the grammar, so the UI never keeps a second copy of it.
    void directivePaletteMatchesDonor() {
        QFile file(QStringLiteral(":/golden/tts-golden.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonArray palette = QJsonDocument::fromJson(file.readAll()).object().value("palette").toArray();
        const QVariantList described = audioticon::describeDirectives();
        QCOMPARE(described.size(), palette.size());
        for (int i = 0; i < palette.size(); ++i) {
            const QJsonObject want = palette.at(i).toObject();
            const QVariantMap got = described.at(i).toMap();
            QCOMPARE(got.value("token").toString(), want.value("token").toString());
            QCOMPARE(got.value("kind").toString(), want.value("kind").toString());
            QCOMPARE(got.value("pauseMs").toInt(), want.value("pauseMs").toInt());
            QCOMPARE(got.value("dsp").toString(), want.value("dsp").toString());
            QCOMPARE(got.value("label").toString(), want.value("label").toString());
        }
    }

    // ---- agent console ----------------------------------------------------

    void agentTurnStreamsAndShowsTools() {
        AgentModel agent;
        const int before = agent.messages().size();
        QCOMPARE(agent.streaming(), false);
        agent.send(QStringLiteral("run the tests"));
        QVERIFY(agent.streaming());
        // The script runs a tool row, streams an answer, then closes the turn.
        QTRY_VERIFY_WITH_TIMEOUT(!agent.streaming(), 6000);
        QCOMPARE(agent.turns(), 1);
        QVERIFY(agent.toolRuns() >= 1);
        QVERIFY(agent.messages().size() > before);
        QStringList roles;
        for (const auto &row : agent.messages())
            roles << row.toMap().value("role").toString();
        QVERIFY(roles.contains("user"));
        QVERIFY(roles.contains("tool"));
        QVERIFY(roles.contains("agent"));
        // Every streamed bubble is closed out, none is left mid-sentence.
        for (const auto &row : agent.messages()) {
            const QVariantMap m = row.toMap();
            if (m.value("role").toString() == "agent")
                QCOMPARE(m.value("streaming").toBool(), false);
        }
        QVERIFY(agent.strip().value("ready").toBool());
        QVERIFY(agent.strip().value("score").toDouble() <= 100);
        QVERIFY(!agent.strip().value("stateLabel").toString().isEmpty());
        QVERIFY(agent.strip().value("series").toList().size() >= 1);
    }

    void agentStopHaltsTheStream() {
        AgentModel agent;
        agent.send(QStringLiteral("please rewrite this"));
        QTest::qWait(80);
        const int frozen = agent.messages().size();
        agent.stop();
        QVERIFY(!agent.streaming());
        QTest::qWait(150);
        QCOMPARE(agent.messages().size(), frozen);
    }

    void agentSessionsKeepTheirOwnTranscript() {
        AgentModel agent;
        const int first = agent.sessions().size();
        const int firstCount = agent.messages().size();
        agent.newSession();
        QCOMPARE(agent.sessions().size(), first + 1);
        QCOMPARE(agent.current(), first);
        QCOMPARE(agent.messages().size(), 1);
        agent.pickSession(0);
        QCOMPARE(agent.current(), 0);
        QCOMPARE(agent.messages().size(), firstCount);
        QCOMPARE(agent.messageAt(0).value("role").toString(), QString("note"));
        QVERIFY(agent.messageAt(999).isEmpty());
    }

    void agentConsoleRenders() {
        AgentModel agent;
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language");
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("feed", &agent);
        engine.rootContext()->setContextProperty("i18n", &translator);
        engine.load(QUrl("qrc:/qml/AgentMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QTest::qWait(150);
        auto *list = window->findChild<QObject *>("transcriptView");
        QVERIFY(list);
        QCOMPARE(list->property("count").toInt(), agent.messages().size());
        // The strip only exists once a turn has run.
        QVERIFY(agent.strip().isEmpty());
        agent.send(QStringLiteral("test"));
        QTRY_VERIFY_WITH_TIMEOUT(!agent.streaming(), 6000);
        QVERIFY(!agent.strip().isEmpty());
        window->resize(800, 620);
        QTest::qWait(150);
        QVERIFY(!window->grabWindow().isNull());
    }
};

QTEST_MAIN(Tests)
#include "tests.moc"
