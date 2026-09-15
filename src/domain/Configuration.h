#pragma once

#include <optional>

#include <QList>
#include <QString>
#include <QStringList>

#include "domain/Snapshot.h"
#include "domain/Timestamp.h"

namespace foldersnap
{
enum class ScheduleKind
{
    Manual,
    Interval,
    Daily,
    Weekly,
    Monthly
};

struct Schedule
{
    ScheduleKind kind{ScheduleKind::Manual};
    std::optional<UtcTimestamp> nextDueAtUtc;
    int intervalHours{0};
    int hour{0};
    int minute{0};
    int weekday{0};
    int dayOfMonth{0};
    bool operator==(const Schedule &) const = default;
};

struct WatchedRoot
{
    QString rootId;
    QString displayName;
    QString path;
    QString normalizedPath;
    bool archived{false};
    Schedule schedule;
    QStringList ignoreRules;
    int retention{50};
    std::optional<UtcTimestamp> lastSnapshotUtc;
    QString lastScanError;
    bool operator==(const WatchedRoot &) const = default;
};

struct Configuration
{
    QList<WatchedRoot> roots;
    int defaultRetention{50};
    QStringList defaultIgnoreRules{"node_modules/", "build/", ".git/"};
    bool launchAtStartup{false};
    bool notifyScheduledSuccess{false};
    bool closeToTray{true};
    bool operator==(const Configuration &) const = default;
};

struct HistoryRecord
{
    QString snapshotId;
    QString rootId;
    QString rootPath;
    QString displayTitle;
    UtcTimestamp completedAtUtc;
    SnapshotTrigger trigger{SnapshotTrigger::Manual};
    QString description;
    qint64 fileCount{0};
    qint64 directoryCount{0};
    qint64 otherCount{0};
    qint64 totalFileBytes{0};
    qint64 warningCount{0};
    qint64 compressedBytes{0};
    bool payloadAvailable{false}; // Runtime-only; never serialized.
    bool operator==(const HistoryRecord &) const = default;
};

void validateSchedule(const Schedule &schedule);
void validateRetention(int retention);
void validateConfiguration(const Configuration &configuration);
void validateHistoryRecord(const HistoryRecord &record);
} // namespace foldersnap
