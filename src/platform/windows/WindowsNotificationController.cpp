#include "platform/windows/WindowsNotificationController.h"

#include <algorithm>
#include <utility>

#include <QApplication>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include "AppState.h"

namespace
{
constexpr int kScheduledNotificationDurationMs = 30000;
constexpr int kCompletionNotificationDurationMs = 5000;
} // namespace

WindowsNotificationController::WindowsNotificationController(AppState &appState, QObject *parent)
    : QObject(parent), m_appState(appState)
{
    createPopup();
    m_scheduledTimer.setSingleShot(true);
    m_completionTimer.setSingleShot(true);
    connect(&m_scheduledTimer, &QTimer::timeout, this,
            &WindowsNotificationController::startCurrentScheduledSnapshot);
    connect(&m_completionTimer, &QTimer::timeout, this,
            &WindowsNotificationController::showNextScheduledPrompt);
    connect(&m_appState, &AppState::scheduledSnapshotDue, this,
            &WindowsNotificationController::enqueueScheduledPrompt);
    connect(&m_appState, &AppState::scanStarted, this,
            &WindowsNotificationController::showScanStarted);
    connect(&m_appState, &AppState::scanProgressed, this,
            &WindowsNotificationController::showScanProgress);
    connect(&m_appState, &AppState::scanCompleted, this,
            [this](const QString &rootId, const QString &, qint64) { showScanCompleted(rootId); });
    connect(&m_appState, &AppState::scanFailed, this,
            &WindowsNotificationController::showScanFailed);
}

WindowsNotificationController::~WindowsNotificationController()
{
    m_scheduledTimer.stop();
    m_completionTimer.stop();
    delete m_popup;
}

void WindowsNotificationController::createPopup()
{
    m_popup = new QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                                       Qt::NoDropShadowWindowHint);
    m_popup->setObjectName(QStringLiteral("notificationPopup"));
    m_popup->setAttribute(Qt::WA_ShowWithoutActivating);
    m_popup->setFixedWidth(380);
    m_popup->setStyleSheet(
        QStringLiteral("QWidget#notificationPopup { background: #142126; color: #effff8; "
                       "border: 1px solid #4e7066; border-radius: 10px; }"
                       "QLabel#notificationTitle { color: #effff8; font-size: 15px; "
                       "font-weight: 600; }"
                       "QLabel#notificationMessage { color: #b8cec5; font-size: 12px; }"
                       "QPushButton { color: #effff8; background: #29463f; border: 1px solid "
                       "#587c70; border-radius: 6px; padding: 5px 10px; }"
                       "QPushButton:hover { background: #355d52; }"
                       "QPushButton#notificationClose { background: transparent; border: none; "
                       "font-size: 17px; padding: 0; }"
                       "QComboBox { color: #effff8; background: #213832; border: 1px solid "
                       "#587c70; border-radius: 6px; padding: 4px 8px; }"
                       "QProgressBar { color: #effff8; background: #20332f; border: 1px solid "
                       "#4e7066; border-radius: 5px; text-align: center; height: 12px; }"
                       "QProgressBar::chunk { background: #79c7aa; border-radius: 4px; }"));

    auto *layout = new QVBoxLayout(m_popup);
    layout->setContentsMargins(16, 13, 16, 15);
    layout->setSpacing(10);

    auto *heading = new QHBoxLayout;
    heading->setSpacing(8);
    m_titleLabel = new QLabel(m_popup);
    m_titleLabel->setObjectName(QStringLiteral("notificationTitle"));
    heading->addWidget(m_titleLabel, 1);
    m_closeButton = new QPushButton(QStringLiteral("×"), m_popup);
    m_closeButton->setObjectName(QStringLiteral("notificationClose"));
    m_closeButton->setFixedSize(26, 26);
    heading->addWidget(m_closeButton);
    layout->addLayout(heading);

    m_messageLabel = new QLabel(m_popup);
    m_messageLabel->setObjectName(QStringLiteral("notificationMessage"));
    m_messageLabel->setWordWrap(true);
    layout->addWidget(m_messageLabel);

    m_progressBar = new QProgressBar(m_popup);
    m_progressBar->setRange(0, 100);
    m_progressBar->setTextVisible(true);
    layout->addWidget(m_progressBar);

    auto *actions = new QHBoxLayout;
    actions->setSpacing(8);
    m_snoozeBox = new QComboBox(m_popup);
    m_snoozeBox->addItem(QStringLiteral("Snooze…"), -1);
    m_snoozeBox->addItem(QStringLiteral("5 minutes"), 5);
    m_snoozeBox->addItem(QStringLiteral("15 minutes"), 15);
    m_snoozeBox->addItem(QStringLiteral("30 minutes"), 30);
    actions->addWidget(m_snoozeBox);
    actions->addStretch(1);
    m_actionButton = new QPushButton(QStringLiteral("Dismiss"), m_popup);
    actions->addWidget(m_actionButton);
    layout->addLayout(actions);

    connect(m_closeButton, &QPushButton::clicked, this,
            &WindowsNotificationController::dismissCurrentNotification);
    connect(m_actionButton, &QPushButton::clicked, this,
            &WindowsNotificationController::dismissCurrentNotification);
    connect(m_snoozeBox, qOverload<int>(&QComboBox::activated), this,
            [this](int index)
            {
                const int minutes = m_snoozeBox->itemData(index).toInt();
                if (minutes > 0)
                {
                    snoozeCurrentScheduledSnapshot(minutes);
                }
            });
    hidePopup();
}

void WindowsNotificationController::enqueueScheduledPrompt(const QString &rootId,
                                                           const QString &displayName)
{
    if (rootId.isEmpty() || rootId == m_activePromptRootId ||
        std::any_of(m_scheduledPrompts.cbegin(), m_scheduledPrompts.cend(),
                    [&rootId](const ScheduledPrompt &prompt) { return prompt.rootId == rootId; }))
    {
        return;
    }
    const ScheduledPrompt prompt{rootId, displayName};
    if (m_popupMode == PopupMode::Hidden && m_activeScanRootId.isEmpty())
    {
        showScheduledPrompt(prompt);
        return;
    }
    m_scheduledPrompts.enqueue(prompt);
}

void WindowsNotificationController::showNextScheduledPrompt()
{
    m_completionTimer.stop();
    if (m_popupMode == PopupMode::Completed)
    {
        m_activeScanRootId.clear();
        m_popupMode = PopupMode::Hidden;
        m_popup->hide();
    }
    if (m_popupMode != PopupMode::Hidden || m_scheduledPrompts.isEmpty() ||
        !m_activeScanRootId.isEmpty())
    {
        return;
    }
    showScheduledPrompt(m_scheduledPrompts.dequeue());
}

void WindowsNotificationController::showScheduledPrompt(const ScheduledPrompt &prompt)
{
    m_scheduledTimer.stop();
    m_completionTimer.stop();
    m_activePromptRootId = prompt.rootId;
    m_popupMode = PopupMode::ScheduledPrompt;
    m_titleLabel->setText(QStringLiteral("Snapshot scheduled"));
    m_messageLabel->setText(
        QStringLiteral("%1 is ready for its scheduled metadata snapshot.").arg(prompt.displayName));
    m_progressBar->hide();
    m_snoozeBox->setCurrentIndex(0);
    m_snoozeBox->show();
    m_actionButton->setText(QStringLiteral("Dismiss"));
    m_actionButton->show();
    showPopup();
    m_scheduledTimer.start(kScheduledNotificationDurationMs);
}

void WindowsNotificationController::showScanStarted(const QString &rootId)
{
    if (m_popupMode == PopupMode::ScheduledPrompt && !m_activePromptRootId.isEmpty())
    {
        m_scheduledPrompts.prepend({m_activePromptRootId, QStringLiteral("Scheduled snapshot")});
    }
    m_activePromptRootId.clear();
    m_scheduledTimer.stop();
    m_completionTimer.stop();
    m_activeScanRootId = rootId;
    m_popupMode = PopupMode::Scanning;
    m_titleLabel->setText(QStringLiteral("Scanning snapshot"));
    m_messageLabel->setText(QStringLiteral("Folder metadata scan in progress…"));
    m_progressBar->setValue(0);
    m_progressBar->show();
    m_snoozeBox->hide();
    m_actionButton->hide();
    showPopup();
}

void WindowsNotificationController::showScanProgress(const QString &rootId, int progress)
{
    if (m_popupMode != PopupMode::Scanning || rootId != m_activeScanRootId)
    {
        return;
    }
    m_progressBar->setValue(std::clamp(progress, 0, 100));
}

void WindowsNotificationController::showScanCompleted(const QString &rootId)
{
    if (!m_activeScanRootId.isEmpty() && rootId != m_activeScanRootId)
    {
        return;
    }
    m_activeScanRootId = rootId;
    m_popupMode = PopupMode::Completed;
    m_titleLabel->setText(QStringLiteral("Snapshot scan complete"));
    m_messageLabel->setText(QStringLiteral("Snapshot scan complete"));
    m_progressBar->hide();
    m_snoozeBox->hide();
    m_actionButton->hide();
    showPopup();
    m_completionTimer.start(kCompletionNotificationDurationMs);
}

void WindowsNotificationController::showScanFailed(const QString &rootId, const QString &error)
{
    if (!m_activeScanRootId.isEmpty() && rootId != m_activeScanRootId)
    {
        return;
    }
    m_activeScanRootId = rootId;
    m_popupMode = PopupMode::Completed;
    m_titleLabel->setText(QStringLiteral("Snapshot scan failed"));
    m_messageLabel->setText(error.isEmpty() ? QStringLiteral("The metadata scan failed.") : error);
    m_progressBar->hide();
    m_snoozeBox->hide();
    m_actionButton->hide();
    showPopup();
    m_completionTimer.start(kCompletionNotificationDurationMs);
}

void WindowsNotificationController::startCurrentScheduledSnapshot()
{
    if (m_activePromptRootId.isEmpty())
    {
        return;
    }
    const QString rootId = std::exchange(m_activePromptRootId, {});
    hidePopup();
    m_appState.startScheduledSnapshot(rootId);
}

void WindowsNotificationController::snoozeCurrentScheduledSnapshot(int minutes)
{
    if (m_activePromptRootId.isEmpty())
    {
        return;
    }
    const QString rootId = std::exchange(m_activePromptRootId, {});
    hidePopup();
    m_appState.snoozeScheduledSnapshot(rootId, minutes);
    showNextScheduledPrompt();
}

void WindowsNotificationController::dismissCurrentNotification()
{
    if (m_popupMode == PopupMode::ScheduledPrompt)
    {
        startCurrentScheduledSnapshot();
        return;
    }
    const bool completed = m_popupMode == PopupMode::Completed;
    hidePopup();
    if (completed)
    {
        m_activeScanRootId.clear();
        showNextScheduledPrompt();
    }
}

void WindowsNotificationController::hidePopup()
{
    m_scheduledTimer.stop();
    m_completionTimer.stop();
    m_popupMode = PopupMode::Hidden;
    m_popup->hide();
}

void WindowsNotificationController::showPopup()
{
    positionPopup();
    m_popup->show();
    m_popup->raise();
}

void WindowsNotificationController::positionPopup()
{
    QScreen *screen = QApplication::primaryScreen();
    if (!screen)
    {
        return;
    }
    m_popup->adjustSize();
    const QRect available = screen->availableGeometry();
    const int x = available.right() - m_popup->width() - 20;
    const int y = available.bottom() - m_popup->height() - 20;
    m_popup->move(std::max(available.left(), x), std::max(available.top(), y));
}
