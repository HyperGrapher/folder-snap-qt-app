#pragma once

#include <QQueue>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

class AppState;
class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QWidget;

class WindowsNotificationController final : public QObject
{
    Q_OBJECT

  public:
    explicit WindowsNotificationController(AppState &appState, QObject *parent = nullptr);
    ~WindowsNotificationController() override;

  private:
    struct ScheduledPrompt
    {
        QString rootId;
        QString displayName;
    };

    enum class PopupMode
    {
        Hidden,
        ScheduledPrompt,
        Scanning,
        Completed
    };

    void createPopup();
    void enqueueScheduledPrompt(const QString &rootId, const QString &displayName);
    void suppressScheduledNotifications(const QString &rootId);
    void updateNotificationPreference();
    void showNextScheduledPrompt();
    void showScheduledPrompt(const ScheduledPrompt &prompt);
    void showScanStarted(const QString &rootId);
    void showScanProgress(const QString &rootId, int progress);
    void showScanCompleted(const QString &rootId);
    void showScanFailed(const QString &rootId, const QString &error);
    void startCurrentScheduledSnapshot();
    void snoozeCurrentScheduledSnapshot(int minutes);
    void dismissCurrentNotification();
    void hidePopup();
    void showPopup();
    void positionPopup();

    AppState &m_appState;
    QWidget *m_popup{nullptr};
    QLabel *m_titleLabel{nullptr};
    QLabel *m_messageLabel{nullptr};
    QProgressBar *m_progressBar{nullptr};
    QComboBox *m_snoozeBox{nullptr};
    QPushButton *m_actionButton{nullptr};
    QPushButton *m_closeButton{nullptr};
    QQueue<ScheduledPrompt> m_scheduledPrompts;
    QSet<QString> m_suppressedScheduledRoots;
    QTimer m_scheduledTimer;
    QTimer m_completionTimer;
    QString m_activePromptRootId;
    QString m_activeScanRootId;
    PopupMode m_popupMode{PopupMode::Hidden};
};
