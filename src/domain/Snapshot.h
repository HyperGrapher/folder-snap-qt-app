#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QtTypes>

#include "domain/Timestamp.h"

namespace foldersnap
{
inline constexpr int kSchemaVersion = 2;

enum class EntryType
{
    File,
    Directory,
    Reparse,
    Other
};
enum class SnapshotTrigger
{
    Manual,
    Scheduled
};
enum class WarningOperation
{
    Enumerate,
    Stat
};
enum class WarningCategory
{
    AccessDenied,
    NotFound,
    Io
};

struct SnapshotEntry
{
    QString path;
    QString displayPath;
    EntryType type{EntryType::File};
    qint64 size{0};
    qint64 modifiedNs{0};
    qint64 createdNs{0};
    quint32 attributes{0};
    QString linkTarget;
    bool operator==(const SnapshotEntry &) const = default;
};

struct ScanWarning
{
    QString path;
    WarningOperation operation{WarningOperation::Enumerate};
    WarningCategory category{WarningCategory::Io};
    QString message;
    bool operator==(const ScanWarning &) const = default;
};

struct IgnoreConfig
{
    QStringList rules;
    QString hash;
    bool operator==(const IgnoreConfig &) const = default;
};

struct SnapshotHeader
{
    QString snapshotId;
    QString rootId;
    QString rootPathAtCapture;
    QString displayTitle;
    UtcTimestamp startedAtUtc;
    UtcTimestamp completedAtUtc;
    SnapshotTrigger trigger{SnapshotTrigger::Manual};
    QString description;
    qint64 fileCount{0};
    qint64 directoryCount{0};
    qint64 otherCount{0};
    qint64 totalFileBytes{0};
    IgnoreConfig ignoreConfig;
    QList<ScanWarning> scanWarnings;
    bool operator==(const SnapshotHeader &) const = default;
};

struct Snapshot
{
    SnapshotHeader header;
    QList<SnapshotEntry> entries;
    bool entriesSorted{true}; // Runtime-only hint calculated by the decoder.
};

[[nodiscard]] QString createId();
void validateUuid(const QString &id);
void validateDescription(const QString &description);
void validateSnapshot(const Snapshot &snapshot);
} // namespace foldersnap
