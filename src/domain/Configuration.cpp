#include "Configuration.h"

#include <QSet>
#include <QUuid>

#include "domain/DomainError.h"
#include "ignore/IgnoreMatcher.h"
#include "paths/WindowsPaths.h"

namespace foldersnap
{
void validateSchedule(const Schedule &schedule)
{
    const bool interval = schedule.kind == ScheduleKind::Interval;
    const bool calendar = schedule.kind == ScheduleKind::Daily ||
                          schedule.kind == ScheduleKind::Weekly ||
                          schedule.kind == ScheduleKind::Monthly;
    if ((!interval && !calendar && schedule.kind != ScheduleKind::Manual) ||
        (interval && schedule.intervalHours != 1 && schedule.intervalHours != 3 &&
         schedule.intervalHours != 6 && schedule.intervalHours != 12) ||
        (!interval && schedule.intervalHours != 0) ||
        (calendar && (schedule.hour < 0 || schedule.hour > 23 || schedule.minute < 0 ||
                      schedule.minute > 59)) ||
        (!calendar && (schedule.hour != 0 || schedule.minute != 0)) ||
        (schedule.kind == ScheduleKind::Weekly && (schedule.weekday < 0 || schedule.weekday > 6)) ||
        (schedule.kind != ScheduleKind::Weekly && schedule.weekday != 0) ||
        (schedule.kind == ScheduleKind::Monthly &&
         (schedule.dayOfMonth < 1 || schedule.dayOfMonth > 31)) ||
        (schedule.kind != ScheduleKind::Monthly && schedule.dayOfMonth != 0) ||
        (schedule.kind == ScheduleKind::Manual && schedule.nextDueAtUtc))
    {
        throw DomainError(ErrorCode::InvalidData, "Invalid snapshot schedule.");
    }
}

void validateRetention(int retention)
{
    if (retention != 0 && retention != 10 && retention != 25 && retention != 50 && retention != 100)
    {
        throw DomainError(ErrorCode::InvalidData,
                          "Retention must be 10, 25, 50, 100 or 0 (unlimited).");
    }
}

void validateConfiguration(const Configuration &configuration)
{
    validateRetention(configuration.defaultRetention);
    const IgnoreMatcher defaults(configuration.defaultIgnoreRules);
    QSet<QString> rootIds;
    QSet<QString> rootPaths;
    for (const WatchedRoot &root : configuration.roots)
    {
        validateUuid(root.rootId);
        const RootPath normalized = normalizeRootPath(root.path);
        if (normalized.displayPath != root.path || normalized.identityPath != root.normalizedPath ||
            root.displayName.trimmed().isEmpty() || rootIds.contains(root.rootId) ||
            rootPaths.contains(root.normalizedPath))
        {
            throw DomainError(ErrorCode::InvalidData,
                              "Invalid or duplicate watched folder: " + root.path);
        }
        rootIds.insert(root.rootId);
        rootPaths.insert(root.normalizedPath);
        validateSchedule(root.schedule);
        validateRetention(root.retention);
        const IgnoreMatcher rules(root.ignoreRules);
    }
}

void validateHistoryRecord(const HistoryRecord &record)
{
    validateUuid(record.snapshotId);
    validateUuid(record.rootId);
    if (QUuid(record.snapshotId).version() != QUuid::Random)
    {
        throw DomainError(ErrorCode::InvalidIdentifier, "Snapshot IDs must be version-4 UUIDs.");
    }
    if (normalizeRootPath(record.rootPath).displayPath != record.rootPath ||
        record.displayTitle.trimmed().isEmpty() || record.fileCount < 0 ||
        record.directoryCount < 0 || record.otherCount < 0 || record.totalFileBytes < 0 ||
        record.warningCount < 0 || record.compressedBytes < 0 ||
        (record.trigger != SnapshotTrigger::Manual && record.trigger != SnapshotTrigger::Scheduled))
    {
        throw DomainError(ErrorCode::InvalidData, "Invalid history record.");
    }
    validateDescription(record.description);
}
} // namespace foldersnap
