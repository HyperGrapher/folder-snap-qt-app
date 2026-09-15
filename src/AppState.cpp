#include "AppState.h"

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
