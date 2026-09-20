#include "export/ExportBuilder.h"

#include <optional>

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QStringList>

#include "diff/ComparisonTree.h"
#include "domain/DomainError.h"
#include "domain/Timestamp.h"

namespace foldersnap
{
namespace
{
constexpr int kExportSchemaVersion = 1;
constexpr char kUtf8Bom[] = "\xEF\xBB\xBF";
constexpr char kReportDataMarker[] = "/* FOLDERSNAP_REPORT_DATA */";

void checkCancelled(const ExportBuilder::CancellationCallback &cancelled)
{
    if (cancelled && cancelled())
    {
        throw DomainError(ErrorCode::Cancelled, "Export was cancelled.");
    }
}

QString entryTypeName(EntryType type)
{
    switch (type)
    {
    case EntryType::File:
        return "file";
    case EntryType::Directory:
        return "directory";
    case EntryType::Reparse:
        return "reparse";
    case EntryType::Other:
        return "other";
    }
    return {};
}

QString changeName(ChangeKind kind)
{
    switch (kind)
    {
    case ChangeKind::Added:
        return "added";
    case ChangeKind::Removed:
        return "removed";
    case ChangeKind::Modified:
        return "modified";
    case ChangeKind::Unchanged:
        return "unchanged";
    case ChangeKind::Uncertain:
        return "uncertain";
    case ChangeKind::ScopeDifference:
        return "scope_difference";
    }
    return {};
}

QString modificationName(ModificationKind kind)
{
    switch (kind)
    {
    case ModificationKind::None:
        return {};
    case ModificationKind::Metadata:
        return "metadata";
    case ModificationKind::TypeChanged:
        return "type_changed";
    }
    return {};
}

QJsonValue timestampValue(qint64 nanoseconds)
{
    return nanoseconds == 0 ? QJsonValue(QJsonValue::Null)
                            : QJsonValue(formatTimestamp({nanoseconds}));
}

QString timestampText(qint64 nanoseconds)
{
    return nanoseconds == 0 ? QString{} : formatTimestamp({nanoseconds});
}

QJsonObject entryDto(const SnapshotEntry &entry)
{
    return {{"path", entry.path},
            {"displayPath", entry.displayPath},
            {"type", entryTypeName(entry.type)},
            {"sizeBytes", QString::number(entry.size)},
            {"createdAtUtc", timestampValue(entry.createdNs)},
            {"modifiedAtUtc", timestampValue(entry.modifiedNs)},
            {"attributes", QString::number(entry.attributes)},
            {"linkTarget", entry.linkTarget}};
}

QJsonObject optionalEntryDto(const std::optional<SnapshotEntry> &entry)
{
    return entry ? entryDto(*entry) : QJsonObject{};
}

QString csvField(QString value)
{
    if (!value.isEmpty() && QStringLiteral("=+-@").contains(value.front()))
    {
        value.prepend('\'');
    }
    if (value.contains(',') || value.contains('"') || value.contains('\r') || value.contains('\n'))
    {
        value.replace('"', "\"\"");
        return '"' + value + '"';
    }
    return value;
}

void appendCsvRow(QByteArray &csv, const QStringList &fields)
{
    QStringList escaped;
    escaped.reserve(fields.size());
    for (const QString &field : fields)
    {
        escaped.append(csvField(field));
    }
    csv.append(escaped.join(',').toUtf8());
    csv.append("\r\n");
}

QString optionalType(const std::optional<SnapshotEntry> &entry)
{
    return entry ? entryTypeName(entry->type) : QString{};
}

QString optionalSize(const std::optional<SnapshotEntry> &entry)
{
    return entry ? QString::number(entry->size) : QString{};
}

QString optionalCreated(const std::optional<SnapshotEntry> &entry)
{
    return entry ? timestampText(entry->createdNs) : QString{};
}

QString optionalModified(const std::optional<SnapshotEntry> &entry)
{
    return entry ? timestampText(entry->modifiedNs) : QString{};
}

QString parentPath(const QString &path)
{
    const qsizetype separator = path.lastIndexOf('/');
    return separator < 0 ? QString{} : path.left(separator);
}

QJsonObject snapshotFolderSizes(const Snapshot &snapshot,
                                const ExportBuilder::CancellationCallback &cancelled)
{
    QHash<QString, qint64> sizes;
    qsizetype entryIndex = 0;
    for (const SnapshotEntry &entry : snapshot.entries)
    {
        if ((entryIndex++ % 256) == 0)
        {
            checkCancelled(cancelled);
        }
        if (entry.type == EntryType::Directory)
        {
            sizes.insert(entry.path, sizes.value(entry.path));
            continue;
        }
        if (entry.type != EntryType::File)
        {
            continue;
        }
        QString parent = parentPath(entry.path);
        while (!parent.isEmpty())
        {
            sizes[parent] += entry.size;
            parent = parentPath(parent);
        }
    }

    QJsonObject result;
    for (auto iterator = sizes.cbegin(); iterator != sizes.cend(); ++iterator)
    {
        result[iterator.key()] = QJsonObject{{"sizeBytes", QString::number(iterator.value())}};
    }
    return result;
}
} // namespace

QJsonObject ExportBuilder::snapshotDto(const Snapshot &snapshot,
                                       const CancellationCallback &cancelled)
{
    validateSnapshot(snapshot);
    checkCancelled(cancelled);
    const SnapshotHeader &header = snapshot.header;
    QJsonArray entries;
    qsizetype entryIndex = 0;
    for (const SnapshotEntry &entry : snapshot.entries)
    {
        if ((entryIndex++ % 256) == 0)
        {
            checkCancelled(cancelled);
        }
        entries.append(entryDto(entry));
    }
    QJsonArray warnings;
    for (const ScanWarning &warning : header.scanWarnings)
    {
        warnings.append(QJsonObject{
            {"path", warning.path},
            {"operation", warning.operation == WarningOperation::Enumerate ? "enumerate" : "stat"},
            {"category", static_cast<int>(warning.category)},
            {"message", warning.message}});
    }
    const QJsonObject headerDto{{"snapshotId", header.snapshotId},
                                {"rootId", header.rootId},
                                {"rootTitle", header.displayTitle},
                                {"rootPath", header.rootPathAtCapture},
                                {"startedAtUtc", formatTimestamp(header.startedAtUtc)},
                                {"completedAtUtc", formatTimestamp(header.completedAtUtc)},
                                {"fileCount", QString::number(header.fileCount)},
                                {"directoryCount", QString::number(header.directoryCount)},
                                {"otherCount", QString::number(header.otherCount)},
                                {"totalFileBytes", QString::number(header.totalFileBytes)},
                                {"warningCount", header.scanWarnings.size()}};
    const QJsonObject folderSizes = snapshotFolderSizes(snapshot, cancelled);
    return {{"schemaVersion", kExportSchemaVersion},
            {"reportType", "snapshot"},
            {"header", headerDto},
            {"entries", entries},
            {"folderSizes", folderSizes},
            {"warnings", warnings}};
}

QJsonObject ExportBuilder::comparisonDto(const Snapshot &before, const Snapshot &after,
                                         const DiffResult &diff,
                                         const CancellationCallback &cancelled)
{
    validateSnapshot(before);
    validateSnapshot(after);
    QJsonArray entries;
    qsizetype entryIndex = 0;
    for (const DiffEntry &entry : diff.entries)
    {
        if ((entryIndex++ % 256) == 0)
        {
            checkCancelled(cancelled);
        }
        const SnapshotEntry *display = entry.after ? &*entry.after : &*entry.before;
        entries.append(QJsonObject{{"path", entry.path},
                                   {"displayPath", display->displayPath},
                                   {"change", changeName(entry.kind)},
                                   {"subtype", modificationName(entry.modification)},
                                   {"before", optionalEntryDto(entry.before)},
                                   {"after", optionalEntryDto(entry.after)}});
    }
    QJsonObject folderSizes;
    for (const ComparisonTreeRow &row : buildComparisonTree(before, after, diff, cancelled))
    {
        if (row.folder)
        {
            folderSizes[row.path] = QJsonObject{{"beforeBytes", QString::number(row.beforeBytes)},
                                                {"afterBytes", QString::number(row.afterBytes)},
                                                {"beforePresent", row.hasBeforeSize},
                                                {"afterPresent", row.hasAfterSize}};
        }
    }
    const QJsonObject header{
        {"rootId", before.header.rootId},
        {"rootTitle", after.header.displayTitle},
        {"rootPath", after.header.rootPathAtCapture},
        {"beforeId", before.header.snapshotId},
        {"afterId", after.header.snapshotId},
        {"beforeCompletedAtUtc", formatTimestamp(before.header.completedAtUtc)},
        {"afterCompletedAtUtc", formatTimestamp(after.header.completedAtUtc)},
        {"beforeWarningCount", diff.summary.beforeWarningCount},
        {"afterWarningCount", diff.summary.afterWarningCount},
        {"ignoreRulesDiffer", diff.summary.ignoreRulesDiffer}};
    return {{"schemaVersion", kExportSchemaVersion},
            {"reportType", "comparison"},
            {"header", header},
            {"entries", entries},
            {"folderSizes", folderSizes}};
}

QByteArray ExportBuilder::snapshotCsv(const Snapshot &snapshot,
                                      const CancellationCallback &cancelled)
{
    validateSnapshot(snapshot);
    checkCancelled(cancelled);
    QByteArray csv(kUtf8Bom, 3);
    appendCsvRow(csv, {"path", "displayPath", "type", "sizeBytes", "createdAtUtc", "modifiedAtUtc",
                       "attributes", "linkTarget"});
    qsizetype entryIndex = 0;
    for (const SnapshotEntry &entry : snapshot.entries)
    {
        if ((entryIndex++ % 256) == 0)
        {
            checkCancelled(cancelled);
        }
        appendCsvRow(csv, {entry.path, entry.displayPath, entryTypeName(entry.type),
                           QString::number(entry.size), timestampText(entry.createdNs),
                           timestampText(entry.modifiedNs), QString::number(entry.attributes),
                           entry.linkTarget});
    }
    return csv;
}

QByteArray ExportBuilder::comparisonCsv(const Snapshot &before, const Snapshot &after,
                                        const DiffResult &diff,
                                        const CancellationCallback &cancelled)
{
    validateSnapshot(before);
    validateSnapshot(after);
    checkCancelled(cancelled);
    QByteArray csv(kUtf8Bom, 3);
    appendCsvRow(csv,
                 {"path", "displayPath", "change", "subtype", "beforeType", "afterType",
                  "beforeSizeBytes", "afterSizeBytes", "beforeCreatedAtUtc", "afterCreatedAtUtc",
                  "beforeModifiedAtUtc", "afterModifiedAtUtc", "uncertain", "scopeDifference"});
    qsizetype entryIndex = 0;
    for (const DiffEntry &entry : diff.entries)
    {
        if ((entryIndex++ % 256) == 0)
        {
            checkCancelled(cancelled);
        }
        const SnapshotEntry *display = entry.after ? &*entry.after : &*entry.before;
        appendCsvRow(csv, {entry.path, display->displayPath, changeName(entry.kind),
                           modificationName(entry.modification), optionalType(entry.before),
                           optionalType(entry.after), optionalSize(entry.before),
                           optionalSize(entry.after), optionalCreated(entry.before),
                           optionalCreated(entry.after), optionalModified(entry.before),
                           optionalModified(entry.after),
                           entry.kind == ChangeKind::Uncertain ? "true" : "false",
                           entry.kind == ChangeKind::ScopeDifference ? "true" : "false"});
    }
    return csv;
}

QByteArray ExportBuilder::htmlReport(const QJsonObject &dto, const QByteArray &templateHtml)
{
    const QByteArray marker(kReportDataMarker);
    if (templateHtml.count(marker) != 1)
    {
        throw DomainError(ErrorCode::InvalidData,
                          "The export template must contain exactly one data marker.");
    }
    QString json = QString::fromUtf8(QJsonDocument(dto).toJson(QJsonDocument::Compact));
    json.replace('&', "\\u0026");
    json.replace('<', "\\u003C");
    json.replace('>', "\\u003E");
    json.replace(QChar(0x2028), "\\u2028");
    json.replace(QChar(0x2029), "\\u2029");
    QByteArray report = templateHtml;
    report.replace(marker, json.toUtf8());
    return report;
}
} // namespace foldersnap
