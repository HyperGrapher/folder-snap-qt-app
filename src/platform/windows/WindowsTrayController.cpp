#include "platform/windows/WindowsTrayController.h"

#include <QAction>
#include <QIcon>

#include "AppState.h"
#include "WindowsWindowController.h"

WindowsTrayController::WindowsTrayController(AppState &appState,
                                             WindowsWindowController &windowController,
                                             QObject *parent)
    : QObject(parent), m_appState(appState), m_windowController(windowController), m_menu(),
      m_tray(QIcon(QStringLiteral(":/resources/icons/foldersnap-icon.png")), nullptr)
{
    QAction *openAction = m_menu.addAction(QStringLiteral("Open FolderSnap"), this,
                                           &WindowsTrayController::activateWindow);
    m_menu.setDefaultAction(openAction);
    m_menu.addSeparator();
    m_snapshotAction = m_menu.addAction(QStringLiteral("Take Snapshot Now"), this,
                                        &WindowsTrayController::takeSnapshot);
    m_menu.addAction(QStringLiteral("Open Settings"), this, &WindowsTrayController::openSettings);
    m_menu.addSeparator();
    m_menu.addAction(QStringLiteral("Quit FolderSnap"), this, &WindowsTrayController::quit);

    m_tray.setToolTip(QStringLiteral("FolderSnap — local metadata snapshots"));
    m_tray.setContextMenu(&m_menu);
    connect(&m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason)
            {
                if (reason == QSystemTrayIcon::Trigger)
                {
                    activateWindow();
                }
            });
    connect(&m_appState, &AppState::scanningChanged, this,
            &WindowsTrayController::updateSnapshotAction);
    connect(&m_appState, &AppState::rootsChanged, this,
            &WindowsTrayController::updateSnapshotAction);
    connect(&m_appState, &AppState::rootIndexChanged, this,
            &WindowsTrayController::updateSnapshotAction);
    updateSnapshotAction();

    if (isAvailable())
    {
        m_tray.show();
    }
}

bool WindowsTrayController::isAvailable() const
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void WindowsTrayController::activateWindow()
{
    m_windowController.activate();
}

void WindowsTrayController::openSettings()
{
    m_appState.setSelectedSection(AppState::Section::Settings);
    activateWindow();
}

void WindowsTrayController::takeSnapshot()
{
    m_appState.takeSnapshot();
    activateWindow();
}

void WindowsTrayController::quit()
{
    m_windowController.quit();
}

void WindowsTrayController::updateSnapshotAction()
{
    const QVariantMap root = m_appState.currentRoot();
    const bool canSnapshot = !m_appState.scanning() && !root.isEmpty() &&
                             !root.value(QStringLiteral("archived")).toBool();
    m_snapshotAction->setEnabled(canSnapshot);
}
