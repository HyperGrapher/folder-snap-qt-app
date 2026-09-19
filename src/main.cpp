#include "WindowsWindowController.h"
#include "platform/windows/SingleInstance.h"
#include <QGuiApplication>
#include <QDebug>
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
    SingleInstance singleInstance;
    const SingleInstance::AcquireResult instanceResult = singleInstance.acquire();
    if (instanceResult == SingleInstance::AcquireResult::Forwarded)
    {
        return 0;
    }
    if (instanceResult == SingleInstance::AcquireResult::Failed)
    {
        qCritical("Could not acquire the FolderSnap single-instance lock.");
        return 1;
    }
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
    QObject::connect(&singleInstance, &SingleInstance::activationRequested, &windowController,
                     &WindowsWindowController::activate);
    window->show();
    if (singleInstance.takePendingActivation())
    {
        windowController.activate();
    }
    return app.exec();
}
