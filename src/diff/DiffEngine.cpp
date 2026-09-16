#include "diff/DiffEngine.h"

#include <algorithm>

#include "domain/DomainError.h"
#include "ignore/IgnoreMatcher.h"
#include "paths/WindowsPaths.h"

namespace foldersnap
{
namespace
{
class DiffCancelled final
{
};

void checkCancelled(const DiffEngine::CancellationCallback &cancelled)
{
    if (cancelled && cancelled())
    {
        throw DiffCancelled{};
    }
}

bool lessPath(const SnapshotEntry &left, const SnapshotEntry &right)
{
    return left.path < right.path;
}

bool sameMetadata(const SnapshotEntry &left, const SnapshotEntry &right)
{
    if (left.type != right.type)
    {
        return false;
    }
    switch (left.type)
    {
    case EntryType::File:
        return left.size == right.size && left.modifiedNs == right.modifiedNs;
    case EntryType::Directory:
        return true;
    case EntryType::Reparse:
        return left.linkTarget == right.linkTarget && left.attributes == right.attributes;
    case EntryType::Other:
        return left.modifiedNs == right.modifiedNs && left.attributes == right.attributes;
    }
    return false;
}

bool hasWarningAtOrBelow(const QList<ScanWarning> &warnings, const QString &path)
{
    for (const ScanWarning &warning : warnings)
    {
        if (warning.path.isEmpty() || isAtOrBelow(path, warning.path))
        {
            return true;
        }
    }
    return false;
}

bool isExcluded(const IgnoreConfig &ignoreConfig, const SnapshotEntry &entry)
{
    const IgnoreMatcher matcher(ignoreConfig.rules);
    return !matcher.testPath(entry.path, entry.type == EntryType::Directory).included;
}

qint64 fileBytes(const std::optional<SnapshotEntry> &entry)
{
    return entry && entry->type == EntryType::File ? entry->size : 0;
}

ChangeKind classifyMissing(const SnapshotEntry &present, const SnapshotHeader &missingHeader,
                           bool presentInAfter)
{
    if (hasWarningAtOrBelow(missingHeader.scanWarnings, present.path))
    {
        return ChangeKind::Uncertain;
    }
    if (isExcluded(missingHeader.ignoreConfig, present))
    {
        return ChangeKind::ScopeDifference;
    }
    return presentInAfter ? ChangeKind::Added : ChangeKind::Removed;
}

int kindOrder(ChangeKind kind)
{
    switch (kind)
    {
    case ChangeKind::Added:
        return 0;
    case ChangeKind::Removed:
        return 1;
    case ChangeKind::Modified:
        return 2;
    case ChangeKind::Uncertain:
        return 3;
    case ChangeKind::ScopeDifference:
        return 4;
    case ChangeKind::Unchanged:
        return 5;
    }
    return 5;
}
} // namespace

DiffResult DiffEngine::compare(const Snapshot &first, const Snapshot &second,
                               const CancellationCallback &cancelled)
{
    if (first.header.snapshotId == second.header.snapshotId || first.header.rootId.isEmpty() ||
        first.header.rootId != second.header.rootId)
    {
        throw DomainError(ErrorCode::InvalidData,
                          "Comparison requires two different snapshots from one folder.");
    }

    DiffResult result;
    try
    {
        const Snapshot *before = &first;
        const Snapshot *after = &second;
        if (after->header.completedAtUtc < before->header.completedAtUtc)
        {
            std::swap(before, after);
        }

        QList<SnapshotEntry> sortedBeforeEntries;
        QList<SnapshotEntry> sortedAfterEntries;
        const QList<SnapshotEntry> *beforeEntries = &before->entries;
        const QList<SnapshotEntry> *afterEntries = &after->entries;
        if (!before->entriesSorted ||
            !std::is_sorted(before->entries.cbegin(), before->entries.cend(), lessPath))
        {
            sortedBeforeEntries = before->entries;
            std::sort(sortedBeforeEntries.begin(), sortedBeforeEntries.end(), lessPath);
            beforeEntries = &sortedBeforeEntries;
        }
        if (!after->entriesSorted ||
            !std::is_sorted(after->entries.cbegin(), after->entries.cend(), lessPath))
        {
            sortedAfterEntries = after->entries;
            std::sort(sortedAfterEntries.begin(), sortedAfterEntries.end(), lessPath);
            afterEntries = &sortedAfterEntries;
        }
        result.summary.beforeWarningCount = before->header.scanWarnings.size();
        result.summary.afterWarningCount = after->header.scanWarnings.size();
        result.summary.ignoreRulesDiffer =
            before->header.ignoreConfig.rules != after->header.ignoreConfig.rules;

        qsizetype beforeIndex = 0;
        qsizetype afterIndex = 0;
        while (beforeIndex < beforeEntries->size() || afterIndex < afterEntries->size())
        {
            checkCancelled(cancelled);
            const bool hasBefore = beforeIndex < beforeEntries->size();
            const bool hasAfter = afterIndex < afterEntries->size();
            const QString path = !hasBefore  ? afterEntries->at(afterIndex).path
                                 : !hasAfter ? beforeEntries->at(beforeIndex).path
                                             : std::min(beforeEntries->at(beforeIndex).path,
                                                        afterEntries->at(afterIndex).path);
            const bool samePath = hasBefore && hasAfter &&
                                  beforeEntries->at(beforeIndex).path == path &&
                                  afterEntries->at(afterIndex).path == path;

            DiffEntry entry;
            entry.path = path;
            if (samePath)
            {
                entry.before = beforeEntries->at(beforeIndex++);
                entry.after = afterEntries->at(afterIndex++);
                entry.kind = sameMetadata(*entry.before, *entry.after) ? ChangeKind::Unchanged
                                                                       : ChangeKind::Modified;
                if (entry.kind == ChangeKind::Modified)
                {
                    entry.modification = entry.before->type == entry.after->type
                                             ? ModificationKind::Metadata
                                             : ModificationKind::TypeChanged;
                }
            }
            else if (hasBefore && beforeEntries->at(beforeIndex).path == path)
            {
                entry.before = beforeEntries->at(beforeIndex++);
                entry.kind = classifyMissing(*entry.before, after->header, false);
            }
            else
            {
                entry.after = afterEntries->at(afterIndex++);
                entry.kind = classifyMissing(*entry.after, before->header, true);
            }

            ++result.summary.comparedCount;
            switch (entry.kind)
            {
            case ChangeKind::Added:
                ++result.summary.addedCount;
                result.summary.addedFileBytes += fileBytes(entry.after);
                result.entries.append(std::move(entry));
                break;
            case ChangeKind::Removed:
                ++result.summary.removedCount;
                result.summary.removedFileBytes += fileBytes(entry.before);
                result.entries.append(std::move(entry));
                break;
            case ChangeKind::Modified:
                ++result.summary.modifiedCount;
                result.summary.modifiedBeforeFileBytes += fileBytes(entry.before);
                result.summary.modifiedAfterFileBytes += fileBytes(entry.after);
                result.entries.append(std::move(entry));
                break;
            case ChangeKind::Unchanged:
                ++result.summary.unchangedCount;
                break;
            case ChangeKind::Uncertain:
                ++result.summary.uncertainCount;
                result.entries.append(std::move(entry));
                break;
            case ChangeKind::ScopeDifference:
                ++result.summary.scopeDifferenceCount;
                result.entries.append(std::move(entry));
                break;
            }
        }
        result.summary.netFileBytes = after->header.totalFileBytes - before->header.totalFileBytes;
        std::sort(result.entries.begin(), result.entries.end(),
                  [&cancelled](const DiffEntry &left, const DiffEntry &right)
                  {
                      checkCancelled(cancelled);
                      const int leftOrder = kindOrder(left.kind);
                      const int rightOrder = kindOrder(right.kind);
                      if (leftOrder != rightOrder)
                      {
                          return leftOrder < rightOrder;
                      }
                      return left.path.compare(right.path, Qt::CaseInsensitive) < 0;
                  });
    }
    catch (const DiffCancelled &)
    {
        result.entries.clear();
        result.cancelled = true;
    }
    return result;
}
} // namespace foldersnap
