#include "AppState.h"
#include <algorithm>

AppState::AppState(QObject *parent) : QObject(parent) {}
void AppState::setSelectedSection(Section section)
{
    if (section < Section::Overview || section > Section::Settings || section == m_selectedSection)
    {
        return;
    }
    m_selectedSection = section;
    emit selectedSectionChanged();
}
void AppState::setReducedMotion(bool enabled)
{
    if (m_reducedMotion == enabled)
    {
        return;
    }
    m_reducedMotion = enabled;
    emit reducedMotionChanged();
}
void AppState::setBackgroundMotionEnabled(bool enabled)
{
    if (m_backgroundMotionEnabled == enabled)
    {
        return;
    }
    m_backgroundMotionEnabled = enabled;
    emit backgroundMotionEnabledChanged();
}
void AppState::advanceProgress()
{
    const int nextProgress = std::min(100, m_demoProgress + 25);
    if (nextProgress != m_demoProgress)
    {
        m_demoProgress = nextProgress;
        emit demoProgressChanged();
    }
}
void AppState::cycleStatus()
{
    switch (m_demoStatus)
    {
    case Status::Ready:
        m_demoStatus = Status::Active;
        break;
    case Status::Active:
        m_demoStatus = Status::Complete;
        break;
    case Status::Complete:
        m_demoStatus = Status::Ready;
        break;
    }
    emit demoStatusChanged();
}
void AppState::resetDemo()
{
    if (m_demoProgress != 25)
    {
        m_demoProgress = 25;
        emit demoProgressChanged();
    }
    if (m_demoStatus != Status::Ready)
    {
        m_demoStatus = Status::Ready;
        emit demoStatusChanged();
    }
}
