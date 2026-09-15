#pragma once
#include <QObject>
#include <QtQml/qqmlregistration.h>

// QML constructs a generated subclass of creatable QObject types.
class AppState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Section selectedSection READ selectedSection WRITE setSelectedSection NOTIFY
                   selectedSectionChanged)
    Q_PROPERTY(
        bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)
    Q_PROPERTY(bool backgroundMotionEnabled READ backgroundMotionEnabled WRITE
                   setBackgroundMotionEnabled NOTIFY backgroundMotionEnabledChanged)
    Q_PROPERTY(int demoProgress READ demoProgress NOTIFY demoProgressChanged)
    Q_PROPERTY(Status demoStatus READ demoStatus NOTIFY demoStatusChanged)
  public:
    enum class Section
    {
        Overview,
        Collection,
        Activity,
        Settings
    };
    Q_ENUM(Section)
    enum class Status
    {
        Ready,
        Active,
        Complete
    };
    Q_ENUM(Status)
    explicit AppState(QObject *parent = nullptr);
    [[nodiscard]] Section selectedSection() const
    {
        return m_selectedSection;
    }
    [[nodiscard]] bool reducedMotion() const
    {
        return m_reducedMotion;
    }
    [[nodiscard]] bool backgroundMotionEnabled() const
    {
        return m_backgroundMotionEnabled;
    }
    [[nodiscard]] int demoProgress() const
    {
        return m_demoProgress;
    }
    [[nodiscard]] Status demoStatus() const
    {
        return m_demoStatus;
    }
    void setSelectedSection(Section section);
    void setReducedMotion(bool enabled);
    void setBackgroundMotionEnabled(bool enabled);
    Q_INVOKABLE void advanceProgress();
    Q_INVOKABLE void cycleStatus();
    Q_INVOKABLE void resetDemo();
  signals:
    void selectedSectionChanged();
    void reducedMotionChanged();
    void backgroundMotionEnabledChanged();
    void demoProgressChanged();
    void demoStatusChanged();

  private:
    Section m_selectedSection{Section::Overview};
    bool m_reducedMotion{false};
    bool m_backgroundMotionEnabled{true};
    int m_demoProgress{25};
    Status m_demoStatus{Status::Ready};
};
