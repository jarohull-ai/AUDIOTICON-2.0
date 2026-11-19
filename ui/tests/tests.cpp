#include <QtTest>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSettings>
#include <QTemporaryDir>

#include "agentmodel.h"
#include "dashboardmodel.h"
#include "translator.h"
#include "voice/DirectiveParser.h"
#include "voice/PolishVerbalizer.h"
#include "AudioticonCore.h"
#include "StudioBridge.h"
#include "AudioPlayer.h"
#include "WallpaperModel.h"
#include "DoctorModel.h"

#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>
#include <cmath>

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

// Restores an environment variable on scope exit, so a failed assertion cannot
// leak a temporary catalog root (or a mock flag) into the next test.
class ScopedEnv {
public:
    explicit ScopedEnv(const char *name) : m_name(name), m_old(qgetenv(name)) {}
    ~ScopedEnv()
    {
        if (m_old.isEmpty())
            qunsetenv(m_name.constData());
        else
            qputenv(m_name.constData(), m_old);
    }

private:
    QByteArray m_name;
    QByteArray m_old;
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
        qmlRegisterType<DoctorModel>("Audioticon.Doctor", 1, 0, "DoctorModel");
        qputenv("AUDIOTICON_MOCK_RENDER", "1");
        qputenv("AUDIOTICON_OFFLINE_TEST", "1");
    }

    // M4: the Qt process calls into the real Rust core through the cxx glue and
    // the staticlib. 44100 Hz is audioticon_core::DEFAULT_SAMPLE_RATE, read out
    // of the core crate rather than duplicated in the bridge.
    void ffiLinksRustCore() {
        QCOMPARE(audioticon::ui::coreSampleRate(), 44100);
    }

    // M5: a real job on the Rust tokio runtime, polled through the ~50 ms QTimer:
    // start -> progress -> finish, with the placeholder WAV written by the core.
    void jobFlowStartProgressFinish() {
        StudioBridge studio;
        QTemporaryDir output;
        QVERIFY(output.isValid());

        bool sawRunning = false;
        double maxPercent = 0.0;
        QObject::connect(&studio, &StudioBridge::taskStatusChanged, &studio, [&] {
            maxPercent = qMax(maxPercent, studio.taskPercent());
            if (studio.taskStatus() == QLatin1String("Synthesizing"))
                sawRunning = true;
        });
        QSignalSpy finished(&studio, &StudioBridge::renderFinished);

        const quint64 jobId = studio.startRender(
            QStringLiteral("To jest test pełnego przepływu. 🛑 Druga część zdania."), output.path());
        QVERIFY(jobId > 0);
        QCOMPARE(studio.jobId(), jobId);
        QVERIFY(studio.isBusy());
        QCOMPARE(studio.taskStatus(), QStringLiteral("Queued"));

        QTRY_VERIFY_WITH_TIMEOUT(sawRunning, 5000);
        QVERIFY(!studio.statusMessage().isEmpty());
        QVERIFY(studio.errorMessage().isEmpty());

        QTRY_VERIFY_WITH_TIMEOUT(!studio.isBusy(), 10000);
        QCOMPARE(studio.taskStatus(), QStringLiteral("Finished"));
        QCOMPARE(studio.taskPercent(), 100.0);
        QVERIFY(maxPercent > 0.0);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.at(0).at(0).toBool(), true);

        // The Rust side really rendered: 200 ms of 44.1 kHz mono WAV.
        QVERIFY(!studio.outputPath().isEmpty());
        QVERIFY(QFile::exists(studio.outputPath()));
        QVERIFY(QFileInfo(studio.outputPath()).size() > 44 + 8000);
        QVERIFY(studio.outputPath().contains(QStringLiteral("200ms")));
    }

    void jobCancelStopsTheJob() {
        StudioBridge studio;
        QTemporaryDir output;
        QVERIFY(output.isValid());

        QSignalSpy finished(&studio, &StudioBridge::renderFinished);
        const quint64 jobId = studio.startRender(
            QStringLiteral("Anulowanie w trakcie syntezy zadania."), output.path());
        QVERIFY(jobId > 0);

        // Wait until the worker has actually produced progress.
        QTRY_VERIFY_WITH_TIMEOUT(studio.taskPercent() > 0.0, 5000);
        studio.cancelRender();

        QVERIFY(!studio.isBusy());
        QCOMPARE(studio.taskStatus(), QStringLiteral("Cancelled"));
        const double atCancel = studio.taskPercent();
        QVERIFY(atCancel < 100.0);
        QVERIFY(studio.outputPath().isEmpty());
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.at(0).at(0).toBool(), false);

        // The worker must not resurrect the job afterwards.
        QTest::qWait(400);
        QCOMPARE(studio.taskStatus(), QStringLiteral("Cancelled"));
        QCOMPARE(studio.taskPercent(), atCancel);
        QVERIFY(!studio.isBusy());
        QCOMPARE(finished.count(), 1);
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

    // ---- M6 UI Shell & Drawer Tests ---------------------------------------

    void uiModuleSwitching() {
        AgentModel agent;
        StudioBridge studio;
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language");
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("feed", &agent);
        engine.rootContext()->setContextProperty("studio", &studio);
        engine.rootContext()->setContextProperty("i18n", &translator);
        engine.load(QUrl("qrc:/qml/AgentMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);

        // Domyślny moduł to Studio
        QCOMPARE(window->property("activeModule").toString(), QStringLiteral("studio"));

        // Przełączenie na Czat AI
        window->setProperty("activeModule", QStringLiteral("agent"));
        QCOMPARE(window->property("activeModule").toString(), QStringLiteral("agent"));

        // Weryfikacja dostępności widoku transkryptu
        auto *list = window->findChild<QObject *>("transcriptView");
        QVERIFY(list);

        // Przełączenie na pozostałe moduły
        const QStringList modules = {QStringLiteral("voicelab"), QStringLiteral("projects"),
                                     QStringLiteral("sfx"), QStringLiteral("ledger")};
        for (const auto &mod : modules) {
            window->setProperty("activeModule", mod);
            QCOMPARE(window->property("activeModule").toString(), mod);
        }

        // Powrót do Studia
        window->setProperty("activeModule", QStringLiteral("studio"));
        QCOMPARE(window->property("activeModule").toString(), QStringLiteral("studio"));
    }

    void uiDrawerVisibility() {
        AgentModel agent;
        StudioBridge studio;
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language");
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("feed", &agent);
        engine.rootContext()->setContextProperty("studio", &studio);
        engine.rootContext()->setContextProperty("i18n", &translator);
        engine.load(QUrl("qrc:/qml/AgentMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);

        // Domyślnie panel jest otwarty i przypięty
        QVERIFY(window->property("inspectorOpen").toBool());
        QVERIFY(window->property("inspectorPinned").toBool());

        // Zamknięcie drawer
        window->setProperty("inspectorOpen", false);
        QCOMPARE(window->property("inspectorOpen").toBool(), false);

        // Ponowne otwarcie i zmiana trybu na odpięty (pływający)
        window->setProperty("inspectorOpen", true);
        window->setProperty("inspectorPinned", false);
        QCOMPARE(window->property("inspectorOpen").toBool(), true);
        QCOMPARE(window->property("inspectorPinned").toBool(), false);

        // Przełączanie zakładek
        const QStringList tabs = {QStringLiteral("inspector"), QStringLiteral("queue"),
                                  QStringLiteral("models"), QStringLiteral("projects"),
                                  QStringLiteral("export"), QStringLiteral("settings"),
                                  QStringLiteral("diagnostics")};
        for (const auto &tab : tabs) {
            window->setProperty("currentInspectorTab", tab);
            QCOMPARE(window->property("currentInspectorTab").toString(), tab);
        }

        // Znalezienie obiektu inspektora w drzewie
        auto *inspector = window->findChild<QObject *>("contextInspector");
        QVERIFY(inspector);
    }

    void uiScreenshotSmokeTest() {
        AgentModel agent;
        StudioBridge studio;
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language");
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("feed", &agent);
        engine.rootContext()->setContextProperty("studio", &studio);
        engine.rootContext()->setContextProperty("i18n", &translator);
        engine.load(QUrl("qrc:/qml/AgentMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);

        // 1. Zrzut ekranu w trybie desktopowym (1180x850)
        window->resize(1180, 850);
        QTest::qWait(100);
        const QImage shot1 = window->grabWindow();
        QVERIFY(!shot1.isNull());
        QVERIFY(shot1.width() >= 1180);
        const QString shotPath = dir.path() + "/smoke_studio.png";
        QVERIFY(shot1.save(shotPath));
        QVERIFY(QFile::exists(shotPath));
        QVERIFY(QFileInfo(shotPath).size() > 5000);

        // 2. Zrzut ekranu w trybie kompaktowym (800x600)
        window->resize(800, 600);
        QTest::qWait(100);
        const QImage shot2 = window->grabWindow();
        QVERIFY(!shot2.isNull());
    }

    static QString createTestWav(const QString &filePath, int durationMs = 150, int sampleRate = 44100) {
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly))
            return QString();

        const quint16 numChannels = 1;
        const quint16 bitsPerSample = 16;
        const quint32 byteRate = sampleRate * numChannels * (bitsPerSample / 8);
        const quint16 blockAlign = numChannels * (bitsPerSample / 8);
        const quint32 totalSamples = static_cast<quint32>(static_cast<quint64>(sampleRate) * durationMs / 1000);
        const quint32 dataSize = totalSamples * blockAlign;
        const quint32 riffSize = 36 + dataSize;

        file.write("RIFF", 4);
        quint32 val32 = qToLittleEndian(riffSize);
        file.write(reinterpret_cast<const char*>(&val32), 4);
        file.write("WAVE", 4);

        file.write("fmt ", 4);
        val32 = qToLittleEndian(16u);
        file.write(reinterpret_cast<const char*>(&val32), 4);
        quint16 val16 = qToLittleEndian(static_cast<quint16>(1)); // PCM
        file.write(reinterpret_cast<const char*>(&val16), 2);
        val16 = qToLittleEndian(numChannels);
        file.write(reinterpret_cast<const char*>(&val16), 2);
        val32 = qToLittleEndian(static_cast<quint32>(sampleRate));
        file.write(reinterpret_cast<const char*>(&val32), 4);
        val32 = qToLittleEndian(byteRate);
        file.write(reinterpret_cast<const char*>(&val32), 4);
        val16 = qToLittleEndian(blockAlign);
        file.write(reinterpret_cast<const char*>(&val16), 2);
        val16 = qToLittleEndian(bitsPerSample);
        file.write(reinterpret_cast<const char*>(&val16), 2);

        file.write("data", 4);
        val32 = qToLittleEndian(dataSize);
        file.write(reinterpret_cast<const char*>(&val32), 4);

        QByteArray pcm(static_cast<int>(dataSize), 0);
        auto *samples = reinterpret_cast<qint16*>(pcm.data());
        constexpr double pi = 3.14159265358979323846;
        for (quint32 i = 0; i < totalSamples; ++i) {
            const double t = static_cast<double>(i) / sampleRate;
            samples[i] = static_cast<qint16>(std::sin(2.0 * pi * 440.0 * t) * 10000.0);
        }
        file.write(pcm);
        file.close();
        return filePath;
    }

    void audioPlayerPlaybackCycle() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString wavPath = createTestWav(tempDir.path() + QStringLiteral("/test_cycle.wav"), 200, 44100);
        QVERIFY(!wavPath.isEmpty());
        QVERIFY(QFile::exists(wavPath));

        AudioPlayer player;
        QCOMPARE(player.isPlaying(), false);
        QCOMPARE(player.isPaused(), false);
        QCOMPARE(player.position(), 0);

        // 1. Ładowanie pliku WAV i parsowanie nagłówka
        QVERIFY(player.load(wavPath));
        QCOMPARE(player.duration(), 200);
        QCOMPARE(player.currentFilePath(), wavPath);
        QCOMPARE(player.audioFormat().sampleRate(), 44100);
        QCOMPARE(player.audioFormat().channelCount(), 1);
        QCOMPARE(player.audioFormat().sampleFormat(), QAudioFormat::Int16);
        QVERIFY(player.errorMessage().isEmpty());

        // 2. Cykl play -> pause -> resume -> stop
        QSignalSpy spyPlaying(&player, &AudioPlayer::playingChanged);
        QSignalSpy spyPaused(&player, &AudioPlayer::pausedChanged);

        QVERIFY(player.play());
        QCOMPARE(player.isPlaying(), true);
        QCOMPARE(player.isPaused(), false);

        player.pause();
        QCOMPARE(player.isPlaying(), false);
        QCOMPARE(player.isPaused(), true);

        player.resume();
        QCOMPARE(player.isPlaying(), true);
        QCOMPARE(player.isPaused(), false);

        player.stop();
        QCOMPARE(player.isPlaying(), false);
        QCOMPARE(player.isPaused(), false);
        QCOMPARE(player.position(), 0);

        // 3. Odtworzenie do końca -> sygnał playbackFinished
        const QString shortWav = createTestWav(tempDir.path() + QStringLiteral("/test_short.wav"), 60, 44100);
        QVERIFY(player.load(shortWav));
        QCOMPARE(player.duration(), 60);

        QSignalSpy spyFinished(&player, &AudioPlayer::playbackFinished);
        QVERIFY(player.play());
        QCOMPARE(player.isPlaying(), true);

        QTRY_VERIFY_WITH_TIMEOUT(spyFinished.count() > 0, 5000);
        QCOMPARE(player.isPlaying(), false);
        QCOMPARE(player.position(), 60);

        // 4. Integracja ze StudioBridge: render -> playOutput
        StudioBridge studio;
        QVERIFY(studio.player() != nullptr);
        QCOMPARE(studio.isPlaying(), false);

        const quint64 id = studio.startRender(QStringLiteral("Test odtwarzacza w studiu."), tempDir.path());
        QVERIFY(id > 0);
        QTRY_VERIFY_WITH_TIMEOUT(!studio.isBusy(), 10000);
        QCOMPARE(studio.taskStatus(), QStringLiteral("Finished"));
        QVERIFY(!studio.outputPath().isEmpty());
        QVERIFY(QFile::exists(studio.outputPath()));

        QSignalSpy studioPlaySpy(&studio, &StudioBridge::isPlayingChanged);
        QVERIFY(studio.playOutput());
        QCOMPARE(studio.isPlaying(), true);
        QVERIFY(studio.playbackDuration() > 0);

        studio.pausePlayback();
        QCOMPARE(studio.isPlaying(), false);
        QCOMPARE(studio.isPaused(), true);

        studio.stopPlayback();
        QCOMPARE(studio.isPlaying(), false);
        QCOMPARE(studio.isPaused(), false);
        QCOMPARE(studio.playbackPosition(), 0);
    }

    // ---- M10: Wallpaper System & Premium Studio Readability ----------------

    void wallpaperSystemAndReadability() {
        freshScope();
        {
            WallpaperModel wp(QStringLiteral("astra-ui-test"), QStringLiteral("astra-ui-test"));
            QCOMPARE(wp.enabled(), true);
            QCOMPARE(wp.mode(), QStringLiteral("static"));
            QVERIFY(wp.wallpaperOpacity() <= 0.35); // spokojna, niska opacity domyślna
            QVERIFY(wp.darknessOverlay() >= 0.50);  // mocny overlay dla czytelności
            QVERIFY(!wp.availableWallpapers().isEmpty());

            // 1. Zmiana opacity i parametrów Glass
            wp.setWallpaperOpacity(0.42);
            wp.setGlassOpacity(0.78);
            wp.setBlurRadius(24.0);
            wp.setDarknessOverlay(0.70);
            wp.setSaturation(0.50);
            wp.setVignette(0.65);

            QCOMPARE(wp.wallpaperOpacity(), 0.42);
            QCOMPARE(wp.glassOpacity(), 0.78);
            QCOMPARE(wp.blurRadius(), 24.0);
            QCOMPARE(wp.darknessOverlay(), 0.70);
            QCOMPARE(wp.saturation(), 0.50);
            QCOMPARE(wp.vignette(), 0.65);

            // 2. Zmiana tapety (static)
            wp.selectWallpaper(QStringLiteral("qrc:/assets/wallpapers/pexels-artyusufpatel-8998289.jpg"), QStringLiteral("static"));
            QCOMPARE(wp.mode(), QStringLiteral("static"));
            QCOMPARE(wp.enabled(), true);

            // 3. Reduced motion
            wp.setReducedMotion(true);
            QCOMPARE(wp.reducedMotion(), true);
            QCOMPARE(wp.effectiveMotionPaused(), true);

            // 4. Wallpaper video bez crasha
            wp.setReducedMotion(false);
            wp.selectWallpaper(QStringLiteral("/tmp/nonexistent_demo_loop.mp4"), QStringLiteral("video"));
            QCOMPARE(wp.mode(), QStringLiteral("video"));

            // 5. Wallpaper off
            wp.setEnabled(false);
            QCOMPARE(wp.enabled(), false);
            QCOMPARE(wp.effectiveMotionPaused(), true);
        }

        // 6. Zapis i odczyt ustawień QSettings
        {
            WallpaperModel restored(QStringLiteral("astra-ui-test"), QStringLiteral("astra-ui-test"));
            QCOMPARE(restored.enabled(), false);
            QCOMPARE(restored.mode(), QStringLiteral("video"));
            QCOMPARE(restored.wallpaperOpacity(), 0.42);
            QCOMPARE(restored.glassOpacity(), 0.78);
            QCOMPARE(restored.blurRadius(), 24.0);
            QCOMPARE(restored.darknessOverlay(), 0.70);
            QCOMPARE(restored.saturation(), 0.50);
            QCOMPARE(restored.vignette(), 0.65);
        }

        // 7. Pełny render QML z tapetą statyczną, video i wyłączoną + brak utraty czytelności
        freshScope();
        AgentModel agent;
        StudioBridge studio;
        WallpaperModel wp(QStringLiteral("astra-ui-test"), QStringLiteral("astra-ui-test"));
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language");
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("feed", &agent);
        engine.rootContext()->setContextProperty("studio", &studio);
        engine.rootContext()->setContextProperty("wallpaper", &wp);
        engine.rootContext()->setContextProperty("i18n", &translator);
        engine.load(QUrl("qrc:/qml/AgentMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());

        auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        auto *bgLayer = window->findChild<QObject *>("wallpaperBackground");
        QVERIFY(bgLayer);

        // Tryb static -> obrazek widoczny, tekst Studia czytelny
        wp.setEnabled(true);
        wp.setMode(QStringLiteral("static"));
        QTest::qWait(80);
        auto *studioView = window->findChild<QObject *>("studioView");
        QVERIFY(studioView);
        QVERIFY(studioView->property("visible").toBool());

        // Tryb video bez crasha
        wp.setMode(QStringLiteral("video"));
        QTest::qWait(80);

        // Tryb off
        wp.setEnabled(false);
        QTest::qWait(60);

        // Weryfikacja responsywności 780x560, 1180x850, 1920x1080
        wp.setEnabled(true);
        wp.setMode(QStringLiteral("static"));

        const QList<QSize> sizes = { QSize(780, 560), QSize(1180, 850), QSize(1920, 1080) };
        for (const QSize &sz : sizes) {
            window->resize(sz);
            QTest::qWait(100);
            const QImage shot = window->grabWindow();
            QVERIFY(!shot.isNull());
            QVERIFY(shot.width() >= sz.width());
            QVERIFY(shot.height() >= sz.height());
        }
    }

    // M11 Regresja B2: preflight weryfikujący poprawność cxx.h
    void preflightCxxHeaderIsValid() {
        const QString backendDir = QString::fromLocal8Bit(qgetenv("AUDIOTICON_BACKEND"));
        const QString cxxPath = (backendDir.isEmpty()
                                    ? QStringLiteral("/home/jaro/.openclaw/workspace/AUDIOTICON 2.0/backend/target/cxxbridge/rust/cxx.h")
                                    : backendDir + QStringLiteral("/target/cxxbridge/rust/cxx.h"));
        QFile file(cxxPath);
        QVERIFY2(file.exists(), "cxx.h musi istnieć (B2 preflight)");
        QVERIFY2(file.size() > 1000, "cxx.h nie może być pusty ani uszkodzony (B2)");
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString content = QString::fromUtf8(file.readAll());
        QVERIFY2(content.contains(QStringLiteral("namespace rust")), "cxx.h musi definiować namespace rust (B2)");
    }

    // M11 Regresja B13: fallback bez modelu raportuje 'brak modelu głosu' i stan Failed, a nie fałszywe 'Finished'
    void fallbackWithoutModelReportsMissingVoiceModel() {
        qunsetenv("AUDIOTICON_MOCK_RENDER");
        qunsetenv("AUDIOTICON_PIPER_MODEL");
        ScopedEnv xdg("XDG_DATA_HOME");
        QTemporaryDir emptyData;
        qputenv("XDG_DATA_HOME", emptyData.path().toUtf8());

        StudioBridge studio;
        QTemporaryDir output;
        QVERIFY(output.isValid());
        QSignalSpy finishedSpy(&studio, &StudioBridge::renderFinished);

        const quint64 jobId = studio.startRender(
            QStringLiteral("Tekst testowy do sprawdzenia fallbacku bez modelu."), output.path());
        QVERIFY(jobId > 0);

        // Oczekujemy przejścia do Failed i raportowania braku modelu głosu
        QTRY_VERIFY_WITH_TIMEOUT(!studio.isBusy(), 5000);
        QCOMPARE(studio.taskStatus(), QStringLiteral("Failed"));
        QVERIFY2(studio.statusMessage().contains(QStringLiteral("brak modelu głosu"), Qt::CaseInsensitive)
                 || studio.errorMessage().contains(QStringLiteral("brak modelu głosu"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("Oczekiwano komunikatu o braku modelu głosu, otrzymano status: '%1', błąd: '%2'")
                            .arg(studio.statusMessage(), studio.errorMessage())));
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.at(0).at(0).toBool(), false); // renderFinished(false)
        QVERIFY(studio.outputPath().isEmpty());

        // Przywracamy mock render dla środowiska testowego
        qputenv("AUDIOTICON_MOCK_RENDER", "1");
    }

    // M13: FirstRunWizard przepływ stanu, resetowanie i zapisywanie
    void firstRunWizardFlow() {
        freshScope();
        DoctorModel doc;
        doc.resetFirstRun();
        QCOMPARE(doc.firstRunCompleted(), false);
        QCOMPARE(doc.inspectionMode(), false);

        QSignalSpy firstRunSpy(&doc, &DoctorModel::firstRunCompletedChanged);
        doc.completeFirstRun();
        QCOMPARE(doc.firstRunCompleted(), true);
        QCOMPARE(firstRunSpy.count(), 1);

        doc.resetFirstRun();
        QCOMPARE(doc.firstRunCompleted(), false);
        QCOMPARE(firstRunSpy.count(), 2);
    }

    // M13: Doctor bez modelu raportuje status FAIL (lub WARN), brak gotowości i lokalne komendy naprawcze
    void doctorMissingModelReportsStatus() {
        freshScope();
        qunsetenv("AUDIOTICON_PIPER_MODEL");
        DoctorModel doc;
        doc.setInspectionMode(false);
        // Wymuś sprawdzenie nieistniejącego modelu
        doc.selectModelFile(QStringLiteral("/nonexistent/model.onnx"));
        doc.refresh();
        QCOMPARE(doc.modelStatus(), QStringLiteral("fail"));
        QCOMPARE(doc.hasVoiceModel(), false);
        QVERIFY(!doc.isEnvironmentReady());
        QVERIFY(doc.errorCount() >= 1);
        QVERIFY(!doc.getRepairCommand(QStringLiteral("model")).isEmpty());
    }

    // M13: Doctor z pełnym środowiskiem i wskazanym modelem ONNX
    void doctorCompleteEnvironmentWithModel() {
        freshScope();
        QTemporaryDir tempModelDir;
        QVERIFY(tempModelDir.isValid());
        const QString dummyModel = tempModelDir.filePath(QStringLiteral("pl_PL-test-voice.onnx"));
        {
            QFile f(dummyModel);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("DUMMY_ONNX_DATA_FOR_TESTING");
            f.close();
        }

        DoctorModel doc;
        const bool selected = doc.selectModelFile(dummyModel);
        QVERIFY(selected);
        QCOMPARE(doc.hasVoiceModel(), true);
        QCOMPARE(doc.activeModelPath(), dummyModel);
        QCOMPARE(doc.modelStatus(), QStringLiteral("ok"));
        QCOMPARE(doc.coreSampleRate(), 44100);
        QCOMPARE(doc.qtStatus(), QStringLiteral("ok"));

        const QString report = doc.generateReport();
        QVERIFY(report.contains(QStringLiteral("AUDIOTICON 2.0")));
        QVERIFY(report.contains(QStringLiteral("44100 Hz")));
        QVERIFY(report.contains(QStringLiteral("pl_PL-test-voice.onnx")));

        doc.copyReportToClipboard();
        QVERIFY(!doc.getRepairCommand(QStringLiteral("ffmpeg")).isEmpty());
        QVERIFY(!doc.getRepairCommand(QStringLiteral("piper")).isEmpty());
    }

    // M13: Tryb reżyserski / inspekcji bez TTS raportuje gotowość środowiska
    void doctorInspectionModeWithoutTts() {
        freshScope();
        qunsetenv("AUDIOTICON_PIPER_MODEL");
        DoctorModel doc;
        doc.setInspectionMode(true);
        QCOMPARE(doc.inspectionMode(), true);
        QCOMPARE(doc.modelStatus(), QStringLiteral("warn")); // warn zamiast fail w trybie reżyserskim
        QVERIFY2(doc.isEnvironmentReady(), "W trybie reżyserskim środowisko powinno być gotowe bez modelu TTS");
        const QString report = doc.generateReport();
        QVERIFY(report.contains(QStringLiteral("DIRECTOR MODE"))
                || report.contains(QStringLiteral("TRYB REŻYSERSKI")));
    }

    // M13: Render widoku Doctora w QML, przełączanie modułu i screenshot offscreen
    void doctorUiModuleAndOffscreenScreenshot() {
        freshScope();
        AgentModel agent;
        StudioBridge studio;
        WallpaperModel wp(QStringLiteral("astra-ui-test"), QStringLiteral("astra-ui-test"));
        DoctorModel doc;
        doc.completeFirstRun();
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language");
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("feed", &agent);
        engine.rootContext()->setContextProperty("studio", &studio);
        engine.rootContext()->setContextProperty("wallpaper", &wp);
        engine.rootContext()->setContextProperty("doctor", &doc);
        engine.rootContext()->setContextProperty("i18n", &translator);
        engine.load(QUrl("qrc:/qml/AgentMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());

        auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        window->resize(QSize(1180, 850));
        QTest::qWait(100);

        // Przełącz na moduł doctor (Ctrl+7)
        window->setProperty("activeModule", QStringLiteral("doctor"));
        QTest::qWait(100);

        auto *doctorView = window->findChild<QObject *>("doctorView");
        QVERIFY2(doctorView, "DoctorView musi istnieć w drzewie obiektów QML");
        QVERIFY2(doctorView->property("visible").toBool(), "DoctorView musi być widoczny po przełączeniu activeModule na doctor");

        // Zrób screenshot widoku Doctora
        const QImage shot = window->grabWindow();
        QVERIFY(!shot.isNull());
        QVERIFY(shot.save(QStringLiteral("/tmp/audioticon-m13-doctor-preview.png")));

        // Test otwarcia prawego drawer z diagnostyką
        window->setProperty("currentInspectorTab", QStringLiteral("diagnostics"));
        window->setProperty("inspectorOpen", true);
        window->setProperty("inspectorPinned", true);
        QTest::qWait(100);
        QCOMPARE(window->property("currentInspectorTab").toString(), QStringLiteral("diagnostics"));
        QCOMPARE(window->property("inspectorOpen").toBool(), true);
    }

    // ---- M16 P0 Regresje UX -------------------------------------------------

    // M16 P0-1: brak pionowego overflow First Run w 780x560, 1180x850 i 1920x1080
    void firstRunNoOverflowAt780x560() {
        freshScope();
        qunsetenv("AUDIOTICON_PIPER_MODEL");
        AgentModel agent;
        StudioBridge studio;
        WallpaperModel wp(QStringLiteral("astra-ui-test"), QStringLiteral("astra-ui-test"));
        DoctorModel doc;
        doc.resetFirstRun();
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language");
        translator.setChoice(QStringLiteral("pl"));
        qApp->installTranslator(&translator);

        QQmlApplicationEngine engine;
        QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
        engine.rootContext()->setContextProperty("feed", &agent);
        engine.rootContext()->setContextProperty("studio", &studio);
        engine.rootContext()->setContextProperty("wallpaper", &wp);
        engine.rootContext()->setContextProperty("doctor", &doc);
        engine.rootContext()->setContextProperty("i18n", &translator);
        engine.load(QUrl("qrc:/qml/AgentMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());

        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);

        auto *card = window->findChild<QQuickItem *>("firstRunCard");
        auto *footer = window->findChild<QQuickItem *>("firstRunFooter");
        auto *openDocBtn = window->findChild<QQuickItem *>("firstRunOpenDoctorBtn");
        auto *noTtsBtn = window->findChild<QQuickItem *>("firstRunNoTtsBtn");
        auto *startBtn = window->findChild<QQuickItem *>("firstRunStartBtn");
        auto *modelTile = window->findChild<QQuickItem *>("firstRunModelTile");
        auto *modelSub = window->findChild<QQuickItem *>("firstRunModelSubtitle");
        QVERIFY(card && footer && openDocBtn && noTtsBtn && startBtn && modelTile && modelSub);

        const QList<QSize> sizes = { QSize(780, 560), QSize(1180, 850), QSize(1920, 1080) };
        for (const QSize &sz : sizes) {
            window->resize(sz);
            QTest::qWait(120);

            // Karta mieści się w oknie
            QVERIFY2(card->height() <= window->height(),
                     qPrintable(QString("Karta FirstRun (%1) przekracza wysokość okna (%2)").arg(card->height()).arg(window->height())));

            // Stopka i wszystkie 3 przyciski są w pełni widoczne wewnątrz karty (zero clippingu)
            const QPointF cardBottom = card->mapToScene(QPointF(0, card->height()));
            const QPointF footerBottom = footer->mapToScene(QPointF(0, footer->height()));
            QVERIFY2(footerBottom.y() <= cardBottom.y() + 1.0,
                     qPrintable(QString("Stopka FirstRun (%1) wychodzi poza dolną krawędź karty (%2) przy %3x%4")
                                .arg(footerBottom.y()).arg(cardBottom.y()).arg(sz.width()).arg(sz.height())));
            QVERIFY(openDocBtn->isVisible() && openDocBtn->width() > 40);
            QVERIFY(noTtsBtn->isVisible() && noTtsBtn->width() > 40);
            QVERIFY(startBtn->isVisible() && startBtn->width() > 40);

            // Podtytuł kafla modelu mieści się wewnątrz obrysu kafla
            const QPointF tileBottom = modelTile->mapToScene(QPointF(0, modelTile->height()));
            const QPointF subBottom = modelSub->mapToScene(QPointF(0, modelSub->height()));
            QVERIFY2(subBottom.y() <= tileBottom.y() - 2.0,
                     qPrintable(QString("Podtytuł modelu (%1) wychodzi poza dolny obrys kafla (%2)").arg(subBottom.y()).arg(tileBottom.y())));

            const QImage shot = window->grabWindow();
            QVERIFY(!shot.isNull());
            QVERIFY(shot.save(QStringLiteral("/tmp/audioticon-m16-firstrun-%1x%2.png").arg(sz.width()).arg(sz.height())));
        }
        qApp->removeTranslator(&translator);
    }

    // M16 P0-2: przełączanie języka PL <-> EN w locie (QML + Doctor C++) i persystencja po restarcie
    void i18nSwitchPlEnAndRestartPersistence() {
        freshScope();
        QTemporaryDir dir;
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, dir.path());

        {
            Translator translator(dir.path() + "/omarchy-lang");
            qApp->installTranslator(&translator);
            AgentModel agent;
            StudioBridge studio;
            WallpaperModel wp(QStringLiteral("astra-ui-test"), QStringLiteral("astra-ui-test"));
            DoctorModel doc;
            doc.resetFirstRun();
            doc.setInspectionMode(true);

            QQmlApplicationEngine engine;
            QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
            QObject::connect(&translator, &Translator::changed, &doc, &DoctorModel::refresh);
            engine.rootContext()->setContextProperty("feed", &agent);
            engine.rootContext()->setContextProperty("studio", &studio);
            engine.rootContext()->setContextProperty("wallpaper", &wp);
            engine.rootContext()->setContextProperty("doctor", &doc);
            engine.rootContext()->setContextProperty("i18n", &translator);
            engine.load(QUrl("qrc:/qml/AgentMain.qml"));
            QVERIFY(!engine.rootObjects().isEmpty());

            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
            auto *noTtsBtn = window->findChild<QObject *>("firstRunNoTtsBtn");
            QVERIFY(noTtsBtn);

            // Przełącz na EN -> angielski QML i angielski raport Doctora
            translator.setChoice(QStringLiteral("en"));
            QTest::qWait(80);
            QCOMPARE(noTtsBtn->property("text").toString(), QStringLiteral("⚙️ Run without TTS"));
            QVERIFY(doc.generateReport().contains(QStringLiteral("DIRECTOR MODE")));

            // Przełącz na PL -> polski QML i polski raport Doctora
            translator.setChoice(QStringLiteral("pl"));
            QTest::qWait(80);
            QCOMPARE(noTtsBtn->property("text").toString(), QStringLiteral("⚙️ Uruchom bez TTS"));
            QVERIFY(doc.generateReport().contains(QStringLiteral("TRYB REŻYSERSKI")));

            // Zapisz wybór do QSettings (jak w main.cpp)
            QSettings st(QStringLiteral("astra-ui-agent"), QStringLiteral("astra-ui-agent"));
            st.setValue(QStringLiteral("language"), translator.choice());
            st.sync();
            qApp->removeTranslator(&translator);
        }

        // Symulacja restartu aplikacji — odczyt persystencji z QSettings
        {
            QSettings st(QStringLiteral("astra-ui-agent"), QStringLiteral("astra-ui-agent"));
            const QString saved = st.value(QStringLiteral("language"), QStringLiteral("auto")).toString();
            QCOMPARE(saved, QStringLiteral("pl"));

            Translator restored(dir.path() + "/omarchy-lang");
            restored.setChoice(saved);
            QCOMPARE(restored.language(), QStringLiteral("pl"));
        }
    }

    // M16 P0-3: spójny parametr glassOpacity i widoczność tapety pod kaflami
    void glassOpacityUnifiedAndWallpaperVisibility() {
        freshScope();
        AgentModel agent;
        StudioBridge studio;
        WallpaperModel wp(QStringLiteral("astra-ui-test"), QStringLiteral("astra-ui-test"));
        DoctorModel doc;
        doc.completeFirstRun();
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language");

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("feed", &agent);
        engine.rootContext()->setContextProperty("studio", &studio);
        engine.rootContext()->setContextProperty("wallpaper", &wp);
        engine.rootContext()->setContextProperty("doctor", &doc);
        engine.rootContext()->setContextProperty("i18n", &translator);
        engine.load(QUrl("qrc:/qml/AgentMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());

        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        auto *navRail = window->findChild<QObject *>("navRail");
        auto *studioView = window->findChild<QObject *>("studioView");
        auto *doctorView = window->findChild<QObject *>("doctorView");
        auto *inspector = window->findChild<QObject *>("contextInspector");
        auto *staticImg = window->findChild<QObject *>("wallpaperStaticImage");
        auto *backdrop = window->findChild<QObject *>("wallpaperBaseBackdrop");
        QVERIFY(navRail && studioView && doctorView && inspector && staticImg && backdrop);

        // Tapeta statyczna widoczna pod kaflami
        wp.setEnabled(true);
        wp.setMode(QStringLiteral("static"));
        QTest::qWait(80);
        QCOMPARE(staticImg->property("visible").toBool(), true);

        // Jeden spójny parametr glassOpacity steruje wszystkimi panelami
        wp.setGlassOpacity(0.45);
        QTest::qWait(60);
        QVERIFY(qFuzzyCompare(window->property("effectiveGlass").toDouble(), 0.45));
        QVERIFY(qFuzzyCompare(navRail->property("glass").toDouble(), 0.45));
        QVERIFY(qFuzzyCompare(studioView->property("glass").toDouble(), 0.45));
        QVERIFY(qFuzzyCompare(doctorView->property("glass").toDouble(), 0.45));
        QVERIFY(qFuzzyCompare(inspector->property("glass").toDouble(), 0.45));

        // Tryb bez tapety: brak nieprzezroczystego #080c11 (alpha tła == glassOpacity < 0.99)
        wp.setMode(QStringLiteral("none"));
        QTest::qWait(60);
        const QColor bgColor = backdrop->property("color").value<QColor>();
        QVERIFY2(bgColor.alphaF() < 0.90, "Tło bez tapety musi respektować glassOpacity (brak pełnego #080c11)");
    }

    // M16 P0-4: wybór tapety, miniatury w ⚙, fallback po usunięciu assetu i screenshoty 780/1180/1920
    void wallpaperSelectionThumbnailsAndDeletedAssetFallback() {
        freshScope();
        QTemporaryDir assetDir;
        QVERIFY(assetDir.isValid());
        const QString tempImgPath = assetDir.filePath(QStringLiteral("custom_studio_wall.png"));
        {
            QImage img(64, 36, QImage::Format_RGB32);
            img.fill(QColor(20, 90, 120));
            QVERIFY(img.save(tempImgPath));
        }

        // 1. Wybór lokalnej tapety i powrót po usunięciu pliku
        {
            WallpaperModel wp(QStringLiteral("astra-ui-test"), QStringLiteral("astra-ui-test"));
            wp.selectWallpaper(tempImgPath, QStringLiteral("static"));
            QVERIFY(wp.source().contains(QStringLiteral("custom_studio_wall.png")));
            QCOMPARE(wp.mode(), QStringLiteral("static"));
            QVERIFY(wp.validateSource());

            // Usuń plik z dysku i zweryfikuj bezpieczny fallback na wbudowaną tapetę qrc
            QVERIFY(QFile::remove(tempImgPath));
            QCOMPARE(wp.validateSource(), false);
            QVERIFY(wp.source().startsWith(QStringLiteral("qrc:/assets/wallpapers/")));
            QVERIFY(!wp.fallbackNotice().isEmpty());
        }

        // 2. Spójny panel ustawień pod ⚙ z miniaturami i screenshoty 780x560, 1180x850, 1920x1080
        freshScope();
        AgentModel agent;
        StudioBridge studio;
        WallpaperModel wp(QStringLiteral("astra-ui-test"), QStringLiteral("astra-ui-test"));
        DoctorModel doc;
        doc.completeFirstRun();
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language");
        translator.setChoice(QStringLiteral("pl"));
        qApp->installTranslator(&translator);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("feed", &agent);
        engine.rootContext()->setContextProperty("studio", &studio);
        engine.rootContext()->setContextProperty("wallpaper", &wp);
        engine.rootContext()->setContextProperty("doctor", &doc);
        engine.rootContext()->setContextProperty("i18n", &translator);
        engine.load(QUrl("qrc:/qml/AgentMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());

        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        auto *settingsDrawer = window->findChild<QObject *>("unifiedSettingsDrawer");
        auto *thumbGrid = window->findChild<QObject *>("wallpaperThumbnailGrid");
        auto *langCombo = window->findChild<QObject *>("settingsLanguageCombo");
        QVERIFY(settingsDrawer && thumbGrid && langCombo);

        const QList<QSize> sizes = { QSize(780, 560), QSize(1180, 850), QSize(1920, 1080) };
        for (const QSize &sz : sizes) {
            window->resize(sz);
            QTest::qWait(120);
            const QImage shot = window->grabWindow();
            QVERIFY(!shot.isNull());
            QVERIFY(shot.save(QStringLiteral("/tmp/audioticon-m16-%1x%2.png").arg(sz.width()).arg(sz.height())));
        }
        qApp->removeTranslator(&translator);
    }

    // M17: kontrakt katalogu modeli przez FFI. Pusty katalog, jawny brak modelu
    // (bez fałszywego Finished), uszkodzony plik, wybór i persystencja wyboru.
    void modelCatalogFfiContract()
    {
        ScopedEnv xdgGuard("XDG_DATA_HOME");
        ScopedEnv mockGuard("AUDIOTICON_MOCK_RENDER");

        QTemporaryDir dataHome;
        QVERIFY(dataHome.isValid());
        qputenv("XDG_DATA_HOME", dataHome.path().toUtf8());
        qunsetenv("AUDIOTICON_MOCK_RENDER");

        const QString appDir = dataHome.path() + QStringLiteral("/audioticon");
        const QString modelsDir = appDir + QStringLiteral("/models");

        // 1. Pusty katalog: brak modeli i jawny status unavailable.
        {
            audioticon::ui::CoreSession session;
            QVERIFY2(session.listModels().isEmpty(), "pusty katalog nie może zwracać modeli");

            const audioticon::ui::ModelCatalogStatus status = session.modelStatus();
            QCOMPARE(status.catalogRoot, modelsDir);
            QCOMPARE(status.total, 0);
            QCOMPARE(status.available, 0);
            QCOMPARE(status.activeStatus, QStringLiteral("unavailable"));
            QVERIFY(status.activeId.isEmpty());
            QVERIFY(status.piperModelPath.isEmpty());
            QVERIFY(status.message.contains(QStringLiteral("Brak modeli")));

            QVERIFY(!session.inspectModel(QStringLiteral("piper/nie-ma")).has_value());
            QString selectError;
            QVERIFY(!session.selectModel(QStringLiteral("piper/nie-ma"), &selectError));
            QVERIFY(!selectError.isEmpty());

            // B13: bez modelu render kończy się błędem, nie fałszywym sukcesem.
            const quint64 jobId =
                session.startJob(QStringLiteral("Test bez modelu."), dataHome.path() + "/out");
            QVERIFY(jobId > 0);
            audioticon::ui::JobSnapshot snapshot;
            for (int i = 0; i < 120; ++i) {
                snapshot = session.jobState(jobId);
                if (snapshot.stage == audioticon::ui::JobStage::Failed
                    || snapshot.stage == audioticon::ui::JobStage::Finished)
                    break;
                QTest::qWait(50);
            }
            QVERIFY2(snapshot.stage == audioticon::ui::JobStage::Failed,
                     "brak modelu nie może kończyć się Finished");
            QVERIFY(snapshot.errorMessage.contains(QStringLiteral("Brak modelu głosu")));
            QVERIFY(snapshot.outputPath.isEmpty());
        }

        // 2. Poprawny model Piper obok uszkodzonego pliku.
        QVERIFY(QDir().mkpath(modelsDir + QStringLiteral("/piper")));
        {
            QFile weights(modelsDir + QStringLiteral("/piper/pl_PL-darkman-medium.onnx"));
            QVERIFY(weights.open(QIODevice::WriteOnly));
            QCOMPARE(weights.write(QByteArray(4096, '\x08')), qint64(4096));
            weights.close();

            QFile config(modelsDir + QStringLiteral("/piper/pl_PL-darkman-medium.onnx.json"));
            QVERIFY(config.open(QIODevice::WriteOnly));
            config.write(R"({"language":{"code":"pl_PL"},"license":"CC0-1.0"})");
            config.close();

            QFile broken(modelsDir + QStringLiteral("/piper/pl_PL-broken-medium.onnx"));
            QVERIFY(broken.open(QIODevice::WriteOnly));
            broken.close();
            QFile brokenConfig(modelsDir + QStringLiteral("/piper/pl_PL-broken-medium.onnx.json"));
            QVERIFY(brokenConfig.open(QIODevice::WriteOnly));
            brokenConfig.write("{}");
            brokenConfig.close();
        }

        const QString goodId = QStringLiteral("piper/pl_PL-darkman-medium");
        const QString brokenId = QStringLiteral("piper/pl_PL-broken-medium");

        audioticon::ui::ModelInfo good;
        {
            audioticon::ui::CoreSession session;
            const QVector<audioticon::ui::ModelInfo> models = session.listModels();
            QCOMPARE(models.size(), 2);

            bool sawGood = false;
            bool sawBroken = false;
            for (const auto &model : models) {
                if (model.id == goodId) {
                    sawGood = true;
                    good = model;
                } else if (model.id == brokenId) {
                    sawBroken = true;
                    QCOMPARE(model.status, QStringLiteral("corrupt"));
                    QVERIFY(!model.isAvailable());
                    QVERIFY(!model.detail.isEmpty());
                }
            }
            QVERIFY(sawGood && sawBroken);

            QCOMPARE(good.kind, QStringLiteral("piper_tts"));
            QCOMPARE(good.format, QStringLiteral("onnx"));
            QCOMPARE(good.language, QStringLiteral("pl_PL"));
            QCOMPARE(good.license, QStringLiteral("CC0-1.0"));
            QCOMPARE(good.sizeBytes, qint64(4096));
            QVERIFY(good.isAvailable());
            QVERIFY(good.checksumSha256.isEmpty());
            const std::optional<audioticon::ui::ModelInfo> inspected = session.inspectModel(goodId);
            QVERIFY(inspected.has_value());
            QCOMPARE(inspected->path, good.path);

            // Uszkodzony model nie może zostać wybrany.
            QString brokenError;
            QVERIFY(!session.selectModel(brokenId, &brokenError));
            QVERIFY(!brokenError.isEmpty());
            QVERIFY(session.modelStatus().activeId.isEmpty());

            // Wybór poprawnego modelu i status.
            QString selectError;
            const std::optional<audioticon::ui::ModelInfo> selected =
                session.selectModel(goodId, &selectError);
            QVERIFY2(selected.has_value(), qPrintable(selectError));
            QCOMPARE(selected->id, goodId);

            const audioticon::ui::ModelCatalogStatus status = session.modelStatus();
            QCOMPARE(status.activeId, goodId);
            QCOMPARE(status.activeKind, QStringLiteral("piper_tts"));
            QCOMPARE(status.activeStatus, QStringLiteral("available"));
            QCOMPARE(status.activePath, good.path);
            QCOMPARE(status.available, 1);
            QCOMPARE(status.unavailable, 1);
            QCOMPARE(status.piperModelPath, good.path);
        }

        // 3. Persystencja wyboru: nowy silnik widzi wybór z pliku.
        {
            audioticon::ui::CoreSession reopened;
            QCOMPARE(reopened.modelStatus().activeId, goodId);
            QCOMPARE(reopened.modelStatus().piperModelPath, good.path);
            QVERIFY(QFile::exists(appDir + QStringLiteral("/model_selection.json")));

            QString clearError;
            QVERIFY2(reopened.clearModelSelection(&clearError), qPrintable(clearError));
            QVERIFY(reopened.modelStatus().activeId.isEmpty());
            QVERIFY(!QFile::exists(appDir + QStringLiteral("/model_selection.json")));
        }
    }

    // M18: źródła, presety bez pobierania, import lokalny i odrzucenia.
    void modelImportContract()
    {
        ScopedEnv xdgGuard("XDG_DATA_HOME");
        ScopedEnv mockGuard("AUDIOTICON_MOCK_RENDER");

        QTemporaryDir dataHome;
        QVERIFY(dataHome.isValid());
        qputenv("XDG_DATA_HOME", dataHome.path().toUtf8());
        qunsetenv("AUDIOTICON_MOCK_RENDER");

        const QString modelsDir = dataHome.path() + QStringLiteral("/audioticon/models");
        QString error;

        // 1. Cztery źródła i presety: listowanie nic nie pobiera.
        {
            audioticon::ui::CoreSession session;
            const QVector<audioticon::ui::ModelSourceInfo> sources = session.listSources();
            QCOMPARE(sources.size(), 4);
            QCOMPARE(sources.first().kind, QStringLiteral("curated"));
            QVERIFY(sources.first().requiresExplicitConsent);
            QVERIFY(!sources.first().network);

            const QVector<audioticon::ui::CuratedPreset> presets = session.listCuratedPresets();
            QVERIFY(presets.size() >= 8);
            bool sawDarkman = false;
            for (const auto &preset : presets) {
                if (preset.id != QStringLiteral("piper/pl_PL-darkman-medium"))
                    continue;
                sawDarkman = true;
                QCOMPARE(preset.kind, QStringLiteral("piper_tts"));
                QCOMPARE(preset.format, QStringLiteral("onnx"));
                QCOMPARE(preset.origin, QStringLiteral("Portable Local Studio"));
                QVERIFY(!preset.author.isEmpty());
                QVERIFY(!preset.license.isEmpty());
                QVERIFY(preset.sizeBytes > 0);
                QVERIFY(!preset.files.isEmpty());
                QVERIFY(!preset.installed);
                QCOMPARE(preset.installedStatus, QStringLiteral("not_installed"));
            }
            QVERIFY2(sawDarkman, "brak presetu Portable Local Studio");

            QVERIFY(session.listModels().isEmpty());
            QVERIFY(!QDir(modelsDir + QStringLiteral("/.staging")).exists());

            // 2. Podgląd przed pobraniem: źródło, autor, licencja, rozmiar, format.
            const std::optional<audioticon::ui::RemoteCandidate> candidate =
                session.inspectRemoteCandidate("preset:piper/pl_PL-darkman-medium", &error);
            QVERIFY2(candidate.has_value(), qPrintable(error));
            QVERIFY(candidate->curated);
            QVERIFY(candidate->downloadable);
            QCOMPARE(candidate->format, QStringLiteral("onnx"));
            QVERIFY(!candidate->author.isEmpty());
            QVERIFY(!candidate->license.isEmpty());
            QVERIFY(candidate->sizeBytes > 0);
            QCOMPARE(candidate->files.size(), 2);
            QVERIFY(!session.inspectRemoteCandidate("hf:../../evil.gguf", &error).has_value());

            // 3. Pobranie wymaga jawnej zgody; potem jawny brak sieci.
            QVERIFY(!session
                         .downloadRemote("preset:piper/pl_PL-darkman-medium", QString(), 0, false,
                                         &error)
                         .has_value());
            QVERIFY2(error.contains(QStringLiteral("zgody")), qPrintable(error));

            error.clear();
            QVERIFY(!session
                         .downloadRemote("preset:piper/pl_PL-darkman-medium", QString(), 0, true,
                                         &error)
                         .has_value());
            QVERIFY2(error.contains(QStringLiteral("sieci")), qPrintable(error));
            QVERIFY(!QDir(modelsDir + QStringLiteral("/.staging")).exists());
            QVERIFY(!session.cancelImport(9999));
            QCOMPARE(session.activeImportId(), quint64(0));
        }

        // 4. Import lokalny poprawnego Pipera i persystencja po restarcie.
        const QString sourceDir = dataHome.path() + QStringLiteral("/src-piper");
        QVERIFY(QDir().mkpath(sourceDir));
        {
            QFile weights(sourceDir + QStringLiteral("/pl_PL-darkman-medium.onnx"));
            QVERIFY(weights.open(QIODevice::WriteOnly));
            QCOMPARE(weights.write(QByteArray(4096, '\x08')), qint64(4096));
            weights.close();
            QFile config(sourceDir + QStringLiteral("/pl_PL-darkman-medium.onnx.json"));
            QVERIFY(config.open(QIODevice::WriteOnly));
            config.write(R"({"language":{"code":"pl_PL"},"license":"CC0-1.0"})");
            config.close();
        }
        {
            audioticon::ui::CoreSession session;
            error.clear();
            const std::optional<audioticon::ui::ModelImportResult> imported =
                session.importLocal(sourceDir, QString(), false, &error);
            QVERIFY2(imported.has_value(), qPrintable(error));
            QCOMPARE(imported->status, QStringLiteral("completed"));
            QCOMPARE(imported->modelId, QStringLiteral("piper/pl_PL-darkman-medium"));
            QCOMPARE(imported->modelStatus, QStringLiteral("available"));
            QVERIFY(imported->source.startsWith(QStringLiteral("local:")));
            QCOMPARE(session.listModels().size(), 1);
        }

        {
            audioticon::ui::CoreSession reopened;
            const QVector<audioticon::ui::ModelInfo> models = reopened.listModels();
            QCOMPARE(models.size(), 1);
            QCOMPARE(models.first().id, QStringLiteral("piper/pl_PL-darkman-medium"));
            QCOMPARE(models.first().license, QStringLiteral("CC0-1.0"));
            QVERIFY(models.first().isAvailable());

            // 5. Weryfikacja checksumu (bez sidecara: tylko suma bieżąca).
            error.clear();
            const std::optional<audioticon::ui::ModelVerifyResult> verified =
                reopened.verifyModel(QStringLiteral("piper/pl_PL-darkman-medium"), &error);
            QVERIFY2(verified.has_value(), qPrintable(error));
            QCOMPARE(verified->status, QStringLiteral("available"));
            QVERIFY(!verified->checked);
            QVERIFY(!verified->actualSha256.isEmpty());

            // 6. Path traversal w id jest odrzucane.
            QVERIFY(!reopened.removeModel(QStringLiteral("piper/../llm/x")));
            QCOMPARE(reopened.listModels().size(), 1);

            // 7. Usunięcie modelu i czysty katalog.
            QVERIFY(reopened.removeModel(QStringLiteral("piper/pl_PL-darkman-medium")));
            QVERIFY(reopened.listModels().isEmpty());
            QVERIFY(!QFile::exists(modelsDir + QStringLiteral("/piper/pl_PL-darkman-medium.onnx")));
        }

        // 8. Odrzucenie złego formatu i niezgodnego checksumu.
        {
            audioticon::ui::CoreSession session;
            const QString bad = dataHome.path() + QStringLiteral("/not-a-model.txt");
            QFile file(bad);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("hello");
            file.close();
            error.clear();
            QVERIFY(!session.importLocal(bad, QString(), false, &error).has_value());
            QVERIFY(!error.isEmpty());

            const QString shaDir = dataHome.path() + QStringLiteral("/src-bad-sha");
            QVERIFY(QDir().mkpath(shaDir));
            QFile weights(shaDir + QStringLiteral("/pl_PL-gosia-medium.onnx"));
            QVERIFY(weights.open(QIODevice::WriteOnly));
            weights.write(QByteArray(4096, '\x09'));
            weights.close();
            QFile config(shaDir + QStringLiteral("/pl_PL-gosia-medium.onnx.json"));
            QVERIFY(config.open(QIODevice::WriteOnly));
            config.write(R"({"language":{"code":"pl_PL"},"license":"CC0-1.0"})");
            config.close();
            QFile sidecar(shaDir + QStringLiteral("/pl_PL-gosia-medium.onnx.sha256"));
            QVERIFY(sidecar.open(QIODevice::WriteOnly));
            sidecar.write(QByteArray(64, '0') + "  pl_PL-gosia-medium.onnx\n");
            sidecar.close();

            error.clear();
            QVERIFY(!session.importLocal(shaDir, QString(), false, &error).has_value());
            QVERIFY2(error.contains(QStringLiteral("checksum")), qPrintable(error));
            QVERIFY(session.listModels().isEmpty());
        }
    }

    // M19: Model Hub UI — pusty katalog, presety curated, modal jawnej zgody,
    // model lokalny / uszkodzony / brak metadanych / incomplete, status aktywnego
    // modelu, anulowanie importu oraz screenshoty 780x560, 1180x850, 1920x1080.
    void modelHubUiEmptyCuratedAndConsentModal()
    {
        freshScope();
        ScopedEnv xdgGuard("XDG_DATA_HOME");
        ScopedEnv mockGuard("AUDIOTICON_MOCK_RENDER");

        QTemporaryDir dataHome;
        QVERIFY(dataHome.isValid());
        qputenv("XDG_DATA_HOME", dataHome.path().toUtf8());
        qunsetenv("AUDIOTICON_MOCK_RENDER");

        AgentModel agent;
        StudioBridge studio;
        WallpaperModel wp(QStringLiteral("astra-ui-test"), QStringLiteral("astra-ui-test"));
        DoctorModel doc;
        doc.completeFirstRun();
        QTemporaryDir langDir;
        Translator translator(langDir.path() + "/language");
        translator.setChoice(QStringLiteral("pl"));
        qApp->installTranslator(&translator);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("feed", &agent);
        engine.rootContext()->setContextProperty("studio", &studio);
        engine.rootContext()->setContextProperty("wallpaper", &wp);
        engine.rootContext()->setContextProperty("doctor", &doc);
        engine.rootContext()->setContextProperty("i18n", &translator);
        engine.load(QUrl("qrc:/qml/AgentMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());

        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        window->setProperty("activeModule", QStringLiteral("models"));
        QTest::qWait(80);

        auto *hub = window->findChild<QObject *>("modelHubView");
        QVERIFY(hub);
        QCOMPARE(hub->property("visible").toBool(), true);

        // 1. Pusty katalog lokalny
        hub->setProperty("activeSection", QStringLiteral("local"));
        QTest::qWait(60);
        const QVariantList localEmpty = hub->property("localModels").toList();
        QVERIFY(localEmpty.isEmpty());
        auto *emptyNotice = window->findChild<QObject *>("emptyCatalogNotice");
        QVERIFY(emptyNotice);
        QCOMPARE(emptyNotice->property("visible").toBool(), true);
        auto *activeLabel = window->findChild<QObject *>("activeModelLabel");
        QVERIFY(activeLabel);
        QVERIFY(activeLabel->property("text").toString().contains(QStringLiteral("Nie wybrano")));

        // 2. Presety curated — obecne bez pobierania i bez .staging
        hub->setProperty("activeSection", QStringLiteral("curated"));
        QTest::qWait(60);
        const QVariantList presets = hub->property("presets").toList();
        QVERIFY2(presets.size() >= 8, "Brak presetów curated w Model Hub");
        QVERIFY(!QDir(dataHome.path() + QStringLiteral("/audioticon/models/.staging")).exists());

        // Filtrowanie typów (Piper / Whisper / LLM / Wszystkie)
        hub->setProperty("activeKindFilter", QStringLiteral("piper_tts"));
        QTest::qWait(30);
        const QVariantList piperPresets = hub->property("filteredPresets").toList();
        QVERIFY(piperPresets.size() >= 5);
        hub->setProperty("activeKindFilter", QStringLiteral("whisper_asr"));
        QTest::qWait(30);
        QVERIFY(hub->property("filteredPresets").toList().size() >= 2);
        hub->setProperty("activeKindFilter", QStringLiteral("local_llm"));
        QTest::qWait(30);
        QVERIFY(hub->property("filteredPresets").toList().size() >= 1);
        hub->setProperty("activeKindFilter", QStringLiteral("all"));
        QTest::qWait(30);

        // 3. Modal jawnej zgody przed pobraniem zdalnym
        QVariant opened = false;
        QVERIFY(QMetaObject::invokeMethod(
            hub, "openConsentForReference",
            Q_RETURN_ARG(QVariant, opened),
            Q_ARG(QVariant, QStringLiteral("preset:piper/pl_PL-darkman-medium")),
            Q_ARG(QVariant, QString()),
            Q_ARG(QVariant, 63201294)));
        QCOMPARE(opened.toBool(), true);
        QTest::qWait(80);

        auto *consentModal = window->findChild<QObject *>("remoteConsentModal");
        auto *consentCheck = window->findChild<QObject *>("remoteConsentCheckbox");
        auto *consentConfirm = window->findChild<QObject *>("remoteConsentConfirmBtn");
        auto *consentAuthor = window->findChild<QObject *>("consentAuthorValue");
        auto *consentLicense = window->findChild<QObject *>("consentLicenseValue");
        auto *consentSource = window->findChild<QObject *>("consentSourceValue");
        auto *netWarning = window->findChild<QObject *>("remoteNetworkWarning");
        QVERIFY(consentModal && consentCheck && consentConfirm && consentAuthor
                && consentLicense && consentSource && netWarning);
        QCOMPARE(consentModal->property("visible").toBool(), true);
        QTRY_COMPARE(consentModal->property("opened").toBool(), true);
        QVERIFY(!consentAuthor->property("text").toString().isEmpty());
        QVERIFY(!consentLicense->property("text").toString().isEmpty());
        QVERIFY(!consentSource->property("text").toString().isEmpty());

        // Bez jawnego zaznaczenia zgody przycisk potwierdzenia jest zablokowany
        QCOMPARE(consentCheck->property("checked").toBool(), false);
        QCOMPARE(consentConfirm->property("enabled").toBool(), false);

        // Zrzut ekranu modalu zgody Glass (1180x850)
        window->resize(1180, 850);
        QTest::qWait(100);
        const QImage consentShot = window->grabWindow();
        QVERIFY(!consentShot.isNull());
        QVERIFY(consentShot.save(QStringLiteral("/tmp/audioticon-m19-consent-1180x850.png")));

        // Zaznaczenie zgody odblokowuje przycisk; kliknięcie w trybie offline kończy się
        // jawnym błędem sieciowym (nigdy fałszywym sukcesem)
        consentCheck->setProperty("checked", true);
        QTest::qWait(40);
        QCOMPARE(consentConfirm->property("enabled").toBool(), true);

        studio.startDownloadRemote(QStringLiteral("preset:piper/pl_PL-darkman-medium"),
                                   QString(), 63201294, true);
        for (int i = 0; i < 60 && studio.importBusy(); ++i)
            QTest::qWait(30);
        QCOMPARE(studio.importBusy(), false);
        QCOMPARE(studio.importStage(), QStringLiteral("failed"));
        QVERIFY2(studio.importError().contains(QStringLiteral("sieci")),
                 qPrintable(studio.importError()));
        QVERIFY(studio.models().isEmpty());

        qApp->removeTranslator(&translator);
    }

    void modelHubLocalStatesCancelAndScreenshots()
    {
        freshScope();
        ScopedEnv xdgGuard("XDG_DATA_HOME");
        ScopedEnv mockGuard("AUDIOTICON_MOCK_RENDER");

        QTemporaryDir dataHome;
        QVERIFY(dataHome.isValid());
        qputenv("XDG_DATA_HOME", dataHome.path().toUtf8());
        qunsetenv("AUDIOTICON_MOCK_RENDER");

        const QString modelsDir = dataHome.path() + QStringLiteral("/audioticon/models");
        QVERIFY(QDir().mkpath(modelsDir + QStringLiteral("/piper")));
        QVERIFY(QDir().mkpath(modelsDir + QStringLiteral("/.staging/import-42")));

        // 1. Model poprawny (available)
        {
            QFile w(modelsDir + QStringLiteral("/piper/pl_PL-darkman-medium.onnx"));
            QVERIFY(w.open(QIODevice::WriteOnly));
            QCOMPARE(w.write(QByteArray(4096, '\x08')), qint64(4096));
            w.close();
            QFile c(modelsDir + QStringLiteral("/piper/pl_PL-darkman-medium.onnx.json"));
            QVERIFY(c.open(QIODevice::WriteOnly));
            c.write(R"({"language":{"code":"pl_PL"},"license":"CC0-1.0"})");
            c.close();
        }
        // 2. Model uszkodzony (corrupt — 0 B)
        {
            QFile w(modelsDir + QStringLiteral("/piper/pl_PL-broken-medium.onnx"));
            QVERIFY(w.open(QIODevice::WriteOnly));
            w.close();
            QFile c(modelsDir + QStringLiteral("/piper/pl_PL-broken-medium.onnx.json"));
            QVERIFY(c.open(QIODevice::WriteOnly));
            c.write("{}");
            c.close();
        }
        // 3. Model bez metadanych (missing_metadata — brak .onnx.json)
        {
            QFile w(modelsDir + QStringLiteral("/piper/pl_PL-nometa-medium.onnx"));
            QVERIFY(w.open(QIODevice::WriteOnly));
            QCOMPARE(w.write(QByteArray(4096, '\x07')), qint64(4096));
            w.close();
        }
        // 4. Przerwany import w .staging (incomplete)
        {
            QFile intent(modelsDir + QStringLiteral("/.staging/import-42/intent.json"));
            QVERIFY(intent.open(QIODevice::WriteOnly));
            intent.write(R"({"import_id":42,"id":"piper/pl_PL-gosia-medium","kind":"piper_tts","format":"onnx","files":["pl_PL-gosia-medium.onnx","pl_PL-gosia-medium.onnx.json"],"source":"curated:preset:piper/pl_PL-gosia-medium","started_at_utc":"2026-11-19T09:00:00Z"})");
            intent.close();
        }

        AgentModel agent;
        StudioBridge studio;
        WallpaperModel wp(QStringLiteral("astra-ui-test"), QStringLiteral("astra-ui-test"));
        DoctorModel doc;
        doc.completeFirstRun();
        QTemporaryDir langDir;
        Translator translator(langDir.path() + "/language");
        translator.setChoice(QStringLiteral("pl"));
        qApp->installTranslator(&translator);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("feed", &agent);
        engine.rootContext()->setContextProperty("studio", &studio);
        engine.rootContext()->setContextProperty("wallpaper", &wp);
        engine.rootContext()->setContextProperty("doctor", &doc);
        engine.rootContext()->setContextProperty("i18n", &translator);
        engine.load(QUrl("qrc:/qml/AgentMain.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());

        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        window->setProperty("activeModule", QStringLiteral("models"));
        auto *hub = window->findChild<QObject *>("modelHubView");
        QVERIFY(hub);
        hub->setProperty("activeSection", QStringLiteral("local"));
        QTest::qWait(100);

        const QVariantList localList = hub->property("localModels").toList();
        QCOMPARE(localList.size(), 4);

        QSet<QString> statuses;
        for (const QVariant &v : localList)
            statuses.insert(v.toMap().value(QStringLiteral("status")).toString());
        QVERIFY(statuses.contains(QStringLiteral("available")));
        QVERIFY(statuses.contains(QStringLiteral("corrupt")));
        QVERIFY(statuses.contains(QStringLiteral("missing_metadata")));
        QVERIFY(statuses.contains(QStringLiteral("incomplete")));

        // Karta uszkodzonego, bez metadanych i incomplete nie pozwala wybrać jako aktywnego
        auto *localRepeater = window->findChild<QObject *>("localModelsRepeater");
        QVERIFY(localRepeater);
        QCOMPARE(localRepeater->property("count").toInt(), 4);

        QObject *corruptCard = nullptr;
        QObject *nometaCard = nullptr;
        QObject *goodCard = nullptr;
        QObject *incompleteCard = nullptr;
        for (int i = 0; i < 4; ++i) {
            QQuickItem *item = nullptr;
            QVERIFY(QMetaObject::invokeMethod(localRepeater, "itemAt",
                                              Q_RETURN_ARG(QQuickItem *, item),
                                              Q_ARG(int, i)));
            QVERIFY(item);
            const QString oname = item->objectName();
            if (oname == QStringLiteral("localCard_piper/pl_PL-broken-medium")) corruptCard = item;
            else if (oname == QStringLiteral("localCard_piper/pl_PL-nometa-medium")) nometaCard = item;
            else if (oname == QStringLiteral("localCard_piper/pl_PL-darkman-medium")) goodCard = item;
            else if (oname == QStringLiteral("localCard_piper/pl_PL-gosia-medium")) incompleteCard = item;
        }
        QVERIFY2(corruptCard && nometaCard && goodCard && incompleteCard,
                 qPrintable(QString("Cards found: corrupt=%1 nometa=%2 good=%3 incomplete=%4")
                            .arg(corruptCard != nullptr).arg(nometaCard != nullptr)
                            .arg(goodCard != nullptr).arg(incompleteCard != nullptr)));
        auto *corruptSelect = corruptCard->findChild<QObject *>("selectModelBtn");
        auto *nometaSelect = nometaCard->findChild<QObject *>("selectModelBtn");
        auto *goodSelect = goodCard->findChild<QObject *>("selectModelBtn");
        auto *incompleteSelect = incompleteCard->findChild<QObject *>("selectModelBtn");
        QVERIFY(corruptSelect && nometaSelect && goodSelect && incompleteSelect);
        QCOMPARE(corruptSelect->property("enabled").toBool(), false);
        QCOMPARE(nometaSelect->property("enabled").toBool(), false);
        QCOMPARE(incompleteSelect->property("enabled").toBool(), false);
        QCOMPARE(goodSelect->property("enabled").toBool(), true);

        // Wybór poprawnego modelu -> status aktywnego modelu w UI
        QVERIFY(studio.selectModel(QStringLiteral("piper/pl_PL-darkman-medium")));
        QTest::qWait(80);
        QCOMPARE(studio.activeModelId(), QStringLiteral("piper/pl_PL-darkman-medium"));
        auto *activeLabel = window->findChild<QObject *>("activeModelLabel");
        QVERIFY(activeLabel);
        QVERIFY(activeLabel->property("text").toString().contains(QStringLiteral("piper/pl_PL-darkman-medium")));

        // Weryfikacja szczegółowa
        const QVariantMap verified = studio.verifyModelDetailed(QStringLiteral("piper/pl_PL-darkman-medium"));
        QCOMPARE(verified.value(QStringLiteral("ok")).toBool(), true);
        QVERIFY(!verified.value(QStringLiteral("actualSha256")).toString().isEmpty());

        // Import anulowany: uruchomienie i natychmiastowe anulowanie nie pokazuje sukcesu
        const QString srcDir = dataHome.path() + QStringLiteral("/src-cancel");
        QVERIFY(QDir().mkpath(srcDir));
        {
            QFile f(srcDir + QStringLiteral("/pl_PL-cancel-medium.onnx"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(QByteArray(4096, '\x05'));
            f.close();
            QFile c(srcDir + QStringLiteral("/pl_PL-cancel-medium.onnx.json"));
            QVERIFY(c.open(QIODevice::WriteOnly));
            c.write(R"({"language":{"code":"pl_PL"},"license":"MIT"})");
            c.close();
        }
        studio.startImportLocal(srcDir, QString(), false);
        QVERIFY(studio.cancelActiveImport());
        for (int i = 0; i < 60 && studio.importBusy(); ++i)
            QTest::qWait(25);
        QCOMPARE(studio.importStage(), QStringLiteral("cancelled"));
        QVERIFY(!studio.inspectModel(QStringLiteral("piper/pl_PL-cancel-medium")).contains(QStringLiteral("id")));
        auto *importBanner = window->findChild<QObject *>("importStatusBanner");
        QVERIFY(importBanner);
        QCOMPARE(importBanner->property("visible").toBool(), true);

        // Screenshoty Model Hub w 780x560, 1180x850 i 1920x1080
        studio.resetImportState();
        QTest::qWait(50);
        const QList<QSize> sizes = { QSize(780, 560), QSize(1180, 850), QSize(1920, 1080) };
        for (const QSize &sz : sizes) {
            window->resize(sz);
            QTest::qWait(120);
            const QImage shot = window->grabWindow();
            QVERIFY(!shot.isNull());
            QVERIFY(shot.save(QStringLiteral("/tmp/audioticon-m19-modelhub-%1x%2.png")
                              .arg(sz.width()).arg(sz.height())));
        }

        qApp->removeTranslator(&translator);
    }
};

QTEST_MAIN(Tests)
#include "tests.moc"
