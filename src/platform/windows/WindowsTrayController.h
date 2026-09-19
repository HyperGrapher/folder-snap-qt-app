#pragma once

#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>

class AppState;
class WindowsWindowController;

class WindowsTrayController final : public QObject
{
    Q_OBJECT

  public:
    WindowsTrayController(AppState &appState, WindowsWindowController &windowController,
                          QObject *parent = nullptr);

    [[nodiscard]] bool isAvailable() const;

  private:
    void activateWindow();
    void openSettings();
    void takeSnapshot();
    void quit();
    void updateSnapshotAction();

    AppState &m_appState;
    WindowsWindowController &m_windowController;
    QMenu m_menu;
    QSystemTrayIcon m_tray;
    QAction *m_snapshotAction{nullptr};
};
