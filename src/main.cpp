#include "AppState.h"
#include "WindowsWindowController.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QQuickStyle>
#include <QQuickWindow>

Q_IMPORT_QML_PLUGIN(AuraPlugin)
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("Aura");
    QGuiApplication::setOrganizationName("AuraDemo");
    QQuickStyle::setStyle("Basic");
    AppState appState;
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"appState", QVariant::fromValue(&appState)}});
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Aura", "Main");
    if (engine.rootObjects().isEmpty())
    {
        return 1;
    }
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window)
    {
        return 1;
    }
    WindowsWindowController windowController(*window);
    window->setProperty("windowController", QVariant::fromValue(&windowController));
    window->show();
    return app.exec();
}
