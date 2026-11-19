#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QTimer>

#include "agentmodel.h"
#include "translator.h"
#include "StudioBridge.h"
#include "AudioticonCore.h"
#include "WallpaperModel.h"
#include "DoctorModel.h"

#include <cstdio>

// Agent console host. The contract is the same as the dashboard shell:
// one model as `feed`, one translator as `i18n`, load the QML.
int main(int argc, char **argv) {
    QString shot, language;
    QSize shotSize;
    bool probeFfi = false;
    bool demoMode = false;
    for (int i = 1; i < argc; i++) {
        const QString arg = argv[i];
        if (arg == "--probe-ffi")            // M4: print the Rust value and exit
            probeFfi = true;
        else if (arg == "--screenshot")
            shot = i + 1 < argc ? QString(argv[i + 1]) : QStringLiteral("/tmp/astra-agent.png");
        else if (arg == "--size" && i + 1 < argc) {
            const QStringList parts = QString(argv[++i]).split('x');
            if (parts.size() == 2)
                shotSize = QSize(parts[0].toInt(), parts[1].toInt());
        } else if (arg == "--language" && i + 1 < argc)
            language = argv[i + 1];
        else if (arg == "--demo") {          // pre-run one turn so the screenshot has content
            demoMode = true;
            qputenv("ASTRA_AGENT_DEMO", "1");
        }
    }
    if (!shot.isEmpty())
        qputenv("QT_QPA_PLATFORM", "offscreen");

    // Diagnostic path for the Rust link proof. Deliberately before
    // QGuiApplication so it works with no display and no QML.
    if (probeFfi) {
        const int rate = audioticon::ui::coreSampleRate();
        std::printf("audioticon core sample rate: %d Hz\n", rate);
        return rate == 44100 ? 0 : 3;
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName("astra-ui-agent");
    app.setOrganizationName("astra-ui-agent");
    app.setDesktopFileName("astra-ui-agent");
    QQuickStyle::setStyle("Material");

    Translator translator;
    {
        QSettings st("astra-ui-agent", "astra-ui-agent");
        translator.setChoice(st.value("language", "auto").toString());
    }
    QObject::connect(&translator, &Translator::changed, &translator, [&] {
        QSettings st("astra-ui-agent", "astra-ui-agent");
        st.setValue("language", translator.choice());
    });
    if (!language.isEmpty())
        translator.setChoice(language);
    app.installTranslator(&translator);

    AgentModel feed;
    StudioBridge studio;
    WallpaperModel wallpaper;
    DoctorModel doctor;
    qmlRegisterType<DoctorModel>("Audioticon.Doctor", 1, 0, "DoctorModel");
    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    QObject::connect(&translator, &Translator::changed, &doctor, &DoctorModel::refresh);
    QObject::connect(&studio, &StudioBridge::modelSelectionChanged, &doctor, &DoctorModel::refresh);
    QObject::connect(&translator, &Translator::changed, &wallpaper, [&wallpaper] { wallpaper.scanAssets(); });
    engine.rootContext()->setContextProperty("feed", &feed);
    engine.rootContext()->setContextProperty("studio", &studio);
    engine.rootContext()->setContextProperty("wallpaper", &wallpaper);
    engine.rootContext()->setContextProperty("doctor", &doctor);
    engine.rootContext()->setContextProperty("i18n", &translator);
    engine.load(QUrl("qrc:/qml/AgentMain.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    if (shotSize.isValid()) {
        if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
            window->resize(shotSize);
        }
    }

    // A screenshot or a demo run needs a turn on screen: feed one prompt and let
    // it stream, so the transcript, the tool rows and the strip are all populated.
    if (demoMode || qEnvironmentVariableIsSet("ASTRA_AGENT_DEMO")) {
        QTimer::singleShot(300, &app, [&feed] { feed.send(QStringLiteral("run the tests")); });
    }
    if (!shot.isEmpty()) {
        const int delayMs = (demoMode || qEnvironmentVariableIsSet("ASTRA_AGENT_DEMO")) ? 9000 : 700;
        QTimer::singleShot(delayMs, &app, [&] {
            auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
            const bool ok = window->grabWindow().save(shot);
            app.exit(ok ? 0 : 2);
        });
    }
    return app.exec();
}
