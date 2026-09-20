#include "JsonCodec.h"

#include <algorithm>
#include <limits>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSet>

#include "domain/DomainError.h"

namespace foldersnap
{
namespace
{
constexpr qsizetype kMiB = 1024 * 1024;

[[noreturn]] void invalidField(const QString &name)
{
    throw DomainError(ErrorCode::InvalidData, "Missing or invalid JSON field: " + name);
}

QJsonObject object(const QJsonValue &value, const QString &name)
{
    if (!value.isObject())
    {
        invalidField(name);
    }
    return value.toObject();
}

QJsonArray array(const QJsonValue &value, const QString &name)
{
    if (!value.isArray())
    {
        invalidField(name);
    }
    return value.toArray();
}

QString string(const QJsonObject &root, const QString &name, bool optional = false)
{
    if (optional && !root.contains(name))
    {
        return {};
    }
    if (!root[name].isString())
    {
        invalidField(name);
    }
    return root[name].toString();
}

bool boolean(const QJsonObject &root, const QString &name)
{
    if (!root[name].isBool())
    {
        invalidField(name);
    }
    return root[name].toBool();
}

qint64 integer(const QJsonObject &root, const QString &name)
{
    const QJsonValue value = root[name];
    const qint64 result = value.toInteger();
    if (!value.isDouble() || QJsonValue(result) != value)
    {
        invalidField(name);
    }
    return result;
}

int smallInteger(const QJsonObject &root, const QString &name)
{
    const qint64 value = integer(root, name);
    if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
    {
        invalidField(name);
    }
    return static_cast<int>(value);
}

void checkSchema(const QJsonObject &root)
{
    if (integer(root, "schemaVersion") != kSchemaVersion)
    {
        throw DomainError(ErrorCode::UnsupportedSchema, "Unsupported FolderSnap schema version.");
    }
}

QJsonObject parseDocument(const QByteArray &json, qsizetype maximumBytes)
{
    if (json.size() > maximumBytes)
    {
        throw DomainError(ErrorCode::SizeLimit, "JSON document exceeds the allowed size.");
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError)
    {
        throw DomainError(
            ErrorCode::InvalidData,
            QString("Invalid JSON at byte %1: %2").arg(error.offset).arg(error.errorString()));
    }
    if (!document.isObject())
    {
        invalidField("document");
    }
    checkSchema(document.object());
    return document.object();
}

QByteArray indentedJson(const QJsonObject &root)
{
    const QByteArray fourSpace = QJsonDocument(root).toJson(QJsonDocument::Indented);
    QByteArray result;
    for (const QByteArray &line : fourSpace.split('\n'))
    {
        qsizetype spaces = 0;
        while (spaces < line.size() && line[spaces] == ' ')
        {
            ++spaces;
        }
        result += line.mid(spaces / 2);
        result += '\n';
    }
    result.chop(1);
    return result;
}

QStringList strings(const QJsonObject &root, const QString &name)
{
    QStringList result;
    for (const QJsonValue &value : array(root[name], name))
    {
        if (!value.isString())
        {
            invalidField(name);
        }
        result.push_back(value.toString());
    }
    return result;
}

std::optional<UtcTimestamp> optionalTimestamp(const QJsonObject &root, const QString &name)
{
    if (!root.contains(name))
    {
        return std::nullopt;
    }
    return parseTimestamp(string(root, name));
}

QString enumName(int value, const QStringList &names)
{
    if (value < 0 || value >= names.size())
    {
        invalidField("enum");
    }
    return names[value];
}

int enumIndex(const QString &value, const QStringList &names)
{
    const qsizetype index = names.indexOf(value);
    if (index < 0)
    {
        invalidField("enum: " + value);
    }
    return static_cast<int>(index);
}

QString triggerName(SnapshotTrigger trigger)
{
    return enumName(static_cast<int>(trigger), {"manual", "scheduled"});
}

SnapshotTrigger parseTrigger(const QJsonObject &root)
{
    return static_cast<SnapshotTrigger>(
        enumIndex(string(root, "trigger"), {"manual", "scheduled"}));
}

QJsonObject encodeSchedule(const Schedule &schedule)
{
    validateSchedule(schedule);
    QJsonObject result{{"kind", enumName(static_cast<int>(schedule.kind),
                                         {"manual", "interval", "daily", "weekly", "monthly"})}};
    if (schedule.nextDueAtUtc)
    {
        result["nextDueAtUtc"] = formatTimestamp(*schedule.nextDueAtUtc);
    }
    if (schedule.kind == ScheduleKind::Interval)
    {
        result["intervalHours"] = schedule.intervalHours;
    }
    if (schedule.kind == ScheduleKind::Daily || schedule.kind == ScheduleKind::Weekly ||
        schedule.kind == ScheduleKind::Monthly)
    {
        result["hour"] = schedule.hour;
        result["minute"] = schedule.minute;
    }
    if (schedule.kind == ScheduleKind::Weekly)
    {
        result["weekday"] = schedule.weekday;
    }
    if (schedule.kind == ScheduleKind::Monthly)
    {
        result["dayOfMonth"] = schedule.dayOfMonth;
    }
    return result;
}

Schedule decodeSchedule(const QJsonObject &root)
{
    Schedule schedule;
    schedule.kind = static_cast<ScheduleKind>(
        enumIndex(string(root, "kind"), {"manual", "interval", "daily", "weekly", "monthly"}));
    schedule.nextDueAtUtc = optionalTimestamp(root, "nextDueAtUtc");
    QStringList fields{"kind", "nextDueAtUtc"};
    if (schedule.kind == ScheduleKind::Interval)
    {
        schedule.intervalHours = smallInteger(root, "intervalHours");
        fields += "intervalHours";
    }
    if (schedule.kind == ScheduleKind::Daily || schedule.kind == ScheduleKind::Weekly ||
        schedule.kind == ScheduleKind::Monthly)
    {
        schedule.hour = smallInteger(root, "hour");
        schedule.minute = smallInteger(root, "minute");
        fields += QStringList{"hour", "minute"};
    }
    if (schedule.kind == ScheduleKind::Weekly)
    {
        schedule.weekday = smallInteger(root, "weekday");
        fields += "weekday";
    }
    if (schedule.kind == ScheduleKind::Monthly)
    {
        schedule.dayOfMonth = smallInteger(root, "dayOfMonth");
        fields += "dayOfMonth";
    }
    for (const QString &key : root.keys())
    {
        if (!fields.contains(key))
        {
            invalidField("schedule." + key);
        }
    }
    validateSchedule(schedule);
    return schedule;
}

QJsonObject encodeEntry(const SnapshotEntry &entry)
{
    QJsonObject result{
        {"path", entry.path},
        {"displayPath", entry.displayPath},
        {"type", enumName(static_cast<int>(entry.type), {"file", "directory", "reparse", "other"})},
        {"size", entry.size},
        {"modifiedNs", entry.modifiedNs},
        {"createdNs", entry.createdNs},
        {"attributes", static_cast<qint64>(entry.attributes)}};
    if (!entry.linkTarget.isEmpty())
    {
        result["linkTarget"] = entry.linkTarget;
    }
    return result;
}

SnapshotEntry decodeEntry(const QJsonObject &root)
{
    SnapshotEntry entry;
    entry.path = string(root, "path");
    entry.displayPath = string(root, "displayPath");
    entry.type = static_cast<EntryType>(
        enumIndex(string(root, "type"), {"file", "directory", "reparse", "other"}));
    entry.size = integer(root, "size");
    entry.modifiedNs = integer(root, "modifiedNs");
    entry.createdNs = root.contains("createdNs") ? integer(root, "createdNs") : 0;
    const qint64 attributes = integer(root, "attributes");
    if (attributes < 0 || attributes > std::numeric_limits<quint32>::max())
    {
        invalidField("attributes");
    }
    entry.attributes = static_cast<quint32>(attributes);
    entry.linkTarget = string(root, "linkTarget", true);
    return entry;
}

QJsonObject encodeWarning(const ScanWarning &warning)
{
    return {{"path", warning.path},
            {"operation", enumName(static_cast<int>(warning.operation), {"enumerate", "stat"})},
            {"category",
             enumName(static_cast<int>(warning.category), {"access_denied", "not_found", "io"})},
            {"message", warning.message}};
}

ScanWarning decodeWarning(const QJsonObject &root)
{
    return {
        string(root, "path"),
        static_cast<WarningOperation>(enumIndex(string(root, "operation"), {"enumerate", "stat"})),
        static_cast<WarningCategory>(
            enumIndex(string(root, "category"), {"access_denied", "not_found", "io"})),
        string(root, "message")};
}

QJsonObject encodeRecord(const HistoryRecord &record)
{
    validateHistoryRecord(record);
    QJsonObject result{{"snapshotId", record.snapshotId},
                       {"rootId", record.rootId},
                       {"rootPath", record.rootPath},
                       {"displayTitle", record.displayTitle},
                       {"completedAtUtc", formatTimestamp(record.completedAtUtc)},
                       {"trigger", triggerName(record.trigger)},
                       {"fileCount", record.fileCount},
                       {"directoryCount", record.directoryCount},
                       {"otherCount", record.otherCount},
                       {"totalFileBytes", record.totalFileBytes},
                       {"warningCount", record.warningCount},
                       {"compressedBytes", record.compressedBytes}};
    if (!record.description.isEmpty())
    {
        result["description"] = record.description;
    }
    return result;
}

HistoryRecord decodeRecord(const QJsonObject &root)
{
    HistoryRecord record;
    record.snapshotId = string(root, "snapshotId");
    record.rootId = string(root, "rootId");
    record.rootPath = string(root, "rootPath");
    record.displayTitle = string(root, "displayTitle");
    record.completedAtUtc = parseTimestamp(string(root, "completedAtUtc"));
    record.trigger = parseTrigger(root);
    record.description = string(root, "description", true);
    record.fileCount = integer(root, "fileCount");
    record.directoryCount = integer(root, "directoryCount");
    record.otherCount = integer(root, "otherCount");
    record.totalFileBytes = integer(root, "totalFileBytes");
    record.warningCount = integer(root, "warningCount");
    record.compressedBytes = integer(root, "compressedBytes");
    validateHistoryRecord(record);
    return record;
}
} // namespace

QByteArray encodeSnapshot(const Snapshot &snapshot)
{
    validateSnapshot(snapshot);
    const auto &header = snapshot.header;
    QJsonArray warnings;
    QList<ScanWarning> sortedWarnings = header.scanWarnings;
    std::sort(sortedWarnings.begin(), sortedWarnings.end(),
              [](const auto &left, const auto &right) {
                  return left.path != right.path ? left.path < right.path
                                                 : left.operation < right.operation;
              });
    for (const ScanWarning &warning : sortedWarnings)
    {
        warnings.push_back(encodeWarning(warning));
    }
    QJsonObject headerJson{
        {"schemaVersion", kSchemaVersion},
        {"snapshotId", header.snapshotId},
        {"rootId", header.rootId},
        {"rootPathAtCapture", header.rootPathAtCapture},
        {"displayTitle", header.displayTitle},
        {"startedAtUtc", formatTimestamp(header.startedAtUtc)},
        {"completedAtUtc", formatTimestamp(header.completedAtUtc)},
        {"trigger", triggerName(header.trigger)},
        {"fileCount", header.fileCount},
        {"directoryCount", header.directoryCount},
        {"otherCount", header.otherCount},
        {"totalFileBytes", header.totalFileBytes},
        {"ignoreConfig",
         QJsonObject{{"rules", QJsonArray::fromStringList(header.ignoreConfig.rules)},
                     {"hash", header.ignoreConfig.hash}}},
        {"scanWarnings", warnings}};
    if (!header.description.isEmpty())
    {
        headerJson["description"] = header.description;
    }
    QList<const SnapshotEntry *> ordered;
    ordered.reserve(snapshot.entries.size());
    for (const auto &entry : snapshot.entries)
    {
        ordered.push_back(&entry);
    }
    std::sort(ordered.begin(), ordered.end(),
              [](const auto *left, const auto *right) { return left->path < right->path; });
    QJsonArray entries;
    for (const auto *entry : ordered)
    {
        entries.push_back(encodeEntry(*entry));
    }
    const QByteArray json = QJsonDocument(QJsonObject{{"schemaVersion", kSchemaVersion},
                                                      {"header", headerJson},
                                                      {"entries", entries}})
                                .toJson(QJsonDocument::Compact);
    if (json.size() > 1024 * kMiB)
    {
        throw DomainError(ErrorCode::SizeLimit, "Snapshot exceeds the decoded size limit.");
    }
    return json;
}

Snapshot decodeSnapshot(const QByteArray &json)
{
    const QJsonObject root = parseDocument(json, 1024 * kMiB);
    const QJsonObject header = object(root["header"], "header");
    checkSchema(header);
    Snapshot snapshot;
    auto &result = snapshot.header;
    result.snapshotId = string(header, "snapshotId");
    result.rootId = string(header, "rootId");
    result.rootPathAtCapture = string(header, "rootPathAtCapture");
    result.displayTitle = string(header, "displayTitle");
    result.startedAtUtc = parseTimestamp(string(header, "startedAtUtc"));
    result.completedAtUtc = parseTimestamp(string(header, "completedAtUtc"));
    result.trigger = parseTrigger(header);
    result.description = string(header, "description", true);
    result.fileCount = integer(header, "fileCount");
    result.directoryCount = integer(header, "directoryCount");
    result.otherCount = integer(header, "otherCount");
    result.totalFileBytes = integer(header, "totalFileBytes");
    const QJsonObject ignore = object(header["ignoreConfig"], "ignoreConfig");
    result.ignoreConfig = {strings(ignore, "rules"), string(ignore, "hash")};
    for (const auto &value : array(header["scanWarnings"], "scanWarnings"))
    {
        result.scanWarnings.push_back(decodeWarning(object(value, "scanWarning")));
    }
    for (const auto &value : array(root["entries"], "entries"))
    {
        snapshot.entries.push_back(decodeEntry(object(value, "entry")));
    }
    snapshot.entriesSorted =
        std::is_sorted(snapshot.entries.begin(), snapshot.entries.end(),
                       [](const auto &left, const auto &right) { return left.path < right.path; });
    validateSnapshot(snapshot);
    return snapshot;
}

QByteArray encodeConfiguration(const Configuration &configuration)
{
    validateConfiguration(configuration);
    QJsonArray roots;
    for (const WatchedRoot &root : configuration.roots)
    {
        QJsonObject item{{"rootId", root.rootId},
                         {"displayName", root.displayName},
                         {"path", root.path},
                         {"normalizedPath", root.normalizedPath},
                         {"archived", root.archived},
                         {"schedule", encodeSchedule(root.schedule)},
                         {"ignoreRules", QJsonArray::fromStringList(root.ignoreRules)},
                         {"retention", root.retention},
                         {"lastScanError", root.lastScanError}};
        if (root.lastSnapshotUtc)
        {
            item["lastSnapshotUtc"] = formatTimestamp(*root.lastSnapshotUtc);
        }
        roots.push_back(item);
    }
    return indentedJson(
        {{"schemaVersion", kSchemaVersion},
         {"roots", roots},
         {"defaultRetention", configuration.defaultRetention},
         {"defaultIgnoreRules", QJsonArray::fromStringList(configuration.defaultIgnoreRules)},
         {"launchAtStartup", configuration.launchAtStartup},
         {"notifyScheduledBefore", configuration.notifyScheduledBefore},
         {"closeToTray", configuration.closeToTray}});
}

Configuration decodeConfiguration(const QByteArray &json)
{
    const QJsonObject root = parseDocument(json, 16 * kMiB);
    Configuration configuration;
    configuration.defaultRetention = smallInteger(root, "defaultRetention");
    configuration.defaultIgnoreRules = strings(root, "defaultIgnoreRules");
    configuration.launchAtStartup = boolean(root, "launchAtStartup");
    configuration.notifyScheduledBefore = boolean(root, "notifyScheduledBefore");
    configuration.closeToTray = boolean(root, "closeToTray");
    for (const auto &value : array(root["roots"], "roots"))
    {
        const QJsonObject item = object(value, "root");
        WatchedRoot watched;
        watched.rootId = string(item, "rootId");
        watched.displayName = string(item, "displayName");
        watched.path = string(item, "path");
        watched.normalizedPath = string(item, "normalizedPath");
        watched.archived = boolean(item, "archived");
        watched.schedule = decodeSchedule(object(item["schedule"], "schedule"));
        watched.ignoreRules = strings(item, "ignoreRules");
        watched.retention = smallInteger(item, "retention");
        watched.lastSnapshotUtc = optionalTimestamp(item, "lastSnapshotUtc");
        watched.lastScanError = string(item, "lastScanError", true);
        configuration.roots.push_back(watched);
    }
    validateConfiguration(configuration);
    return configuration;
}

QByteArray encodeHistoryIndex(const QList<HistoryRecord> &records)
{
    QList<HistoryRecord> sorted = records;
    std::sort(sorted.begin(), sorted.end(),
              [](const auto &left, const auto &right)
              {
                  return left.completedAtUtc != right.completedAtUtc
                             ? left.completedAtUtc > right.completedAtUtc
                             : left.snapshotId < right.snapshotId;
              });
    QJsonArray array;
    QSet<QString> identifiers;
    for (const auto &record : sorted)
    {
        if (identifiers.contains(record.snapshotId))
        {
            throw DomainError(ErrorCode::InvalidData, "Duplicate snapshot ID in history index.");
        }
        identifiers.insert(record.snapshotId);
        array.push_back(encodeRecord(record));
    }
    return indentedJson({{"schemaVersion", kSchemaVersion}, {"records", array}});
}

QList<HistoryRecord> decodeHistoryIndex(const QByteArray &json)
{
    const QJsonObject root = parseDocument(json, 32 * kMiB);
    QList<HistoryRecord> records;
    QSet<QString> identifiers;
    for (const auto &value : array(root["records"], "records"))
    {
        auto record = decodeRecord(object(value, "record"));
        if (identifiers.contains(record.snapshotId))
        {
            throw DomainError(ErrorCode::InvalidData, "Duplicate snapshot ID in history index.");
        }
        identifiers.insert(record.snapshotId);
        records.push_back(record);
    }
    return records;
}
} // namespace foldersnap
