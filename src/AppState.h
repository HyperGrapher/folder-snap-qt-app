#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

// QML derives UiPreviewState from the presentation state while the UI is reviewed.
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
  public:
    enum class Section
    {
        Overview,
        Folders,
        Compare,
        Settings
    };
    Q_ENUM(Section)
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
    void setSelectedSection(Section section);
    void setReducedMotion(bool enabled);
    void setBackgroundMotionEnabled(bool enabled);
  signals:
    void selectedSectionChanged();
    void reducedMotionChanged();
    void backgroundMotionEnabledChanged();

  private:
    Section m_selectedSection{Section::Overview};
    bool m_reducedMotion{false};
    bool m_backgroundMotionEnabled{true};
};
