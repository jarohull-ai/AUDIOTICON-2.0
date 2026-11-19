#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QTimer>

#include "dashboardmodel.h"
#include "demomodel.h"
#include "translator.h"

// The demo host: create your model, hand it to QML as `feed`, hand the
// translator over as `i18n`, and load Main.qml. That is the whole contract.
int main(int argc, char **argv) {
    QString shot, language;
    for (int i = 1; i < argc; i++) {
        const QString arg = argv[i];
        if (arg == "--screenshot")
            shot = i + 1 < argc ? QString(argv[i + 1]) : QStringLiteral("/tmp/astra-ui.png");
        else if (arg == "--language" && i + 1 < argc)
            language = argv[i + 1];
    }
    if (!shot.isEmpty()) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        // Sample ten times faster so the screenshot has a full 90-point history.
        qputenv("ASTRA_UI_DEMO_INTERVAL", "100");
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName("astra-ui-template-demo");
    app.setOrganizationName("astra-ui-template-demo");
    app.setDesktopFileName("astra-ui-template-demo");
    QQuickStyle::setStyle("Material");

    // The picker in settings writes here; "auto" follows Omarchy's own language.
    Translator translator;
    {
        QSettings st("astra-ui-template-demo", "astra-ui-template-demo");
        translator.setChoice(st.value("language", "auto").toString());
    }
    QObject::connect(&translator, &Translator::changed, &translator, [&] {
        QSettings st("astra-ui-template-demo", "astra-ui-template-demo");
        st.setValue("language", translator.choice());
    });
    if (!language.isEmpty())   // pin the language for screenshots and tests
        translator.setChoice(language);
    app.installTranslator(&translator);

    DemoModel feed;
    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("feed", &feed);
    engine.rootContext()->setContextProperty("i18n", &translator);
    engine.load(QUrl("qrc:/qml/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    if (!shot.isEmpty()) {
        // Give the graphs a few seconds of history so the screenshot is not blank.
        QTimer::singleShot(10000, &app, [&] {
            auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
            const bool ok = window->grabWindow().save(shot);
            app.exit(ok ? 0 : 2);
        });
    }
    return app.exec();
}
