#include "WindowsWindowController.h"
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QQuickStyle>
#include <QQuickWindow>

Q_IMPORT_QML_PLUGIN(FolderSnapPlugin)
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("FolderSnap");
    QGuiApplication::setApplicationDisplayName("FolderSnap");
    QGuiApplication::setOrganizationName("FolderSnap");
    QGuiApplication::setWindowIcon(QIcon(":/resources/icons/foldersnap-icon.png"));
    QQuickStyle::setStyle("Basic");
    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("FolderSnap", "Main");
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
