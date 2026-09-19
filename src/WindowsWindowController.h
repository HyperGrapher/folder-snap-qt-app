#pragma once
#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QQuickWindow>

class WindowsWindowController final : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
    Q_PROPERTY(bool exposed READ isExposed NOTIFY exposedChanged)
    Q_PROPERTY(qreal titleHeight MEMBER m_titleHeight)
    Q_PROPERTY(qreal titleButtonsWidth MEMBER m_titleButtonsWidth)
  public:
    explicit WindowsWindowController(QQuickWindow &window);
    ~WindowsWindowController() override;
    [[nodiscard]] bool isExposed() const;
    void activate();
    void setCloseToTray(bool enabled);
    void quit();
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
  signals:
    void exposedChanged();

  private:
    void updateCorners();
    QQuickWindow &m_window;
    WId m_handle{0};
    qreal m_titleHeight{48};
    qreal m_titleButtonsWidth{138};
    bool m_usesNativeRounding{false};
    bool m_reportedCornerFailure{false};
    bool m_closeToTray{false};
    bool m_quitRequested{false};
};
