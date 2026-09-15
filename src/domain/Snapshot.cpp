#include "Snapshot.h"

#include <limits>

#include <QSet>
#include <QUuid>

#include "domain/DomainError.h"
#include "ignore/IgnoreMatcher.h"
#include "paths/WindowsPaths.h"

namespace foldersnap
{
QString createId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void validateUuid(const QString &id)
{
    validateStorageId(id);
    const QUuid uuid(id);
    if (uuid.isNull() || uuid.variant() != QUuid::DCE || uuid.toString(QUuid::WithoutBraces) != id)
    {
        throw DomainError(ErrorCode::InvalidIdentifier, "Expected a canonical UUID: " + id);
    }
}

void validateDescription(const QString &description)
{
    if (description.toUcs4().size() > 500)
    {
        throw DomainError(ErrorCode::InvalidData,
                          "A description cannot exceed 500 Unicode code points.");
    }
}

void validateSnapshot(const Snapshot &snapshot)
{
    const SnapshotHeader &header = snapshot.header;
    validateUuid(header.snapshotId);
    validateUuid(header.rootId);
    if (QUuid(header.snapshotId).version() != QUuid::Random)
    {
        throw DomainError(ErrorCode::InvalidIdentifier, "Snapshot IDs must be version-4 UUIDs.");
    }
    const RootPath root = normalizeRootPath(header.rootPathAtCapture);
    if (root.displayPath != header.rootPathAtCapture || header.displayTitle.trimmed().isEmpty() ||
        header.completedAtUtc < header.startedAtUtc ||
        (header.trigger != SnapshotTrigger::Manual && header.trigger != SnapshotTrigger::Scheduled))
    {
        throw DomainError(ErrorCode::InvalidData, "Invalid snapshot root, title or capture times.");
    }
    validateDescription(header.description);
    const IgnoreMatcher matcher(header.ignoreConfig.rules);
    if (header.ignoreConfig.hash != IgnoreMatcher::rulesHash(header.ignoreConfig.rules))
    {
        throw DomainError(ErrorCode::InvalidData,
                          "Snapshot exclusion hash does not match its rules.");
    }
    QSet<QString> paths;
    qint64 files = 0;
    qint64 directories = 0;
    qint64 others = 0;
    qint64 bytes = 0;
    for (const SnapshotEntry &entry : snapshot.entries)
    {
        validateIdentityPath(entry.path);
        if (entry.displayPath.contains('\\') ||
            normalizeRelativePath(entry.displayPath) != entry.path || paths.contains(entry.path) ||
            entry.size < 0)
        {
            throw DomainError(ErrorCode::InvalidData,
                              "Invalid or duplicate snapshot entry: " + entry.path);
        }
        paths.insert(entry.path);
        if (entry.type == EntryType::File)
        {
            ++files;
            if (bytes > std::numeric_limits<qint64>::max() - entry.size)
            {
                throw DomainError(ErrorCode::SizeLimit, "Snapshot byte total overflows 64 bits.");
            }
            bytes += entry.size;
        }
        else
        {
            if (entry.size != 0)
            {
                throw DomainError(ErrorCode::InvalidData,
                                  "Only regular files can have a nonzero size.");
            }
            if (entry.type == EntryType::Directory)
            {
                ++directories;
            }
            else if (entry.type == EntryType::Reparse || entry.type == EntryType::Other)
            {
                ++others;
            }
            else
            {
                throw DomainError(ErrorCode::InvalidData, "Unknown snapshot entry type.");
            }
        }
        if (entry.type != EntryType::Reparse && !entry.linkTarget.isEmpty())
        {
            throw DomainError(ErrorCode::InvalidData,
                              "Only reparse entries can have link targets.");
        }
    }
    if (header.fileCount != files || header.directoryCount != directories ||
        header.otherCount != others || header.totalFileBytes != bytes)
    {
        throw DomainError(ErrorCode::InvalidData, "Snapshot summary does not match its entries.");
    }
    for (const ScanWarning &warning : header.scanWarnings)
    {
        validateIdentityPath(warning.path);
        if ((warning.operation != WarningOperation::Enumerate &&
             warning.operation != WarningOperation::Stat) ||
            (warning.category != WarningCategory::AccessDenied &&
             warning.category != WarningCategory::NotFound &&
             warning.category != WarningCategory::Io))
        {
            throw DomainError(ErrorCode::InvalidData,
                              "Unknown scan warning operation or category.");
        }
    }
}
} // namespace foldersnap
