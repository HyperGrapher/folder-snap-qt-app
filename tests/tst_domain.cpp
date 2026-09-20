#include <algorithm>
#include <limits>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QtTest>

#include "domain/DomainError.h"
#include "domain/JsonCodec.h"
#include "ignore/IgnoreMatcher.h"

namespace
{
QByteArray fixture(const QString &name)
{
    QFile file(QString(FOLDERSNAP_FIXTURE_DIR) + '/' + name + "-v2.json");
    if (!file.open(QIODevice::ReadOnly))
    {
        qFatal("Cannot read fixture: %s", qPrintable(file.fileName()));
    }
    return file.readAll();
}

QByteArray json(const QJsonObject &root)
{
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}
} // namespace

class DomainTest final : public QObject
{
    Q_OBJECT
  private slots:
    void snapshotRoundTrip()
    {
        const auto snapshot = foldersnap::decodeSnapshot(fixture("snapshot"));
        const auto encoded = foldersnap::encodeSnapshot(snapshot);
        const auto decoded = foldersnap::decodeSnapshot(encoded);
        QVERIFY(decoded.header == snapshot.header);
        QVERIFY(decoded.entries == snapshot.entries);
        QVERIFY(decoded.entriesSorted);
        QCOMPARE(decoded.header.totalFileBytes, Q_INT64_C(9007199254740993));
        QCOMPARE(decoded.entries.last().modifiedNs, Q_INT64_C(1789466400123456789));
        QCOMPARE(decoded.entries.last().createdNs, Q_INT64_C(1789460000987654321));
        QCOMPARE(decoded.entries.last().attributes, std::numeric_limits<quint32>::max());
        QVERIFY(encoded.contains("9007199254740993"));
        QVERIFY(!encoded.contains('\n'));
        QVERIFY(!encoded.contains("entriesSorted"));
        auto unordered = snapshot;
        std::reverse(unordered.entries.begin(), unordered.entries.end());
        std::reverse(unordered.header.scanWarnings.begin(), unordered.header.scanWarnings.end());
        QCOMPARE(foldersnap::encodeSnapshot(unordered), encoded);
        auto root = QJsonDocument::fromJson(encoded).object();
        auto entries = root["entries"].toArray();
        entries.prepend(entries.takeAt(entries.size() - 1));
        root["entries"] = entries;
        QVERIFY(!foldersnap::decodeSnapshot(json(root)).entriesSorted);
    }

    void configurationRoundTrip()
    {
        const auto config = foldersnap::decodeConfiguration(fixture("config"));
        QCOMPARE(config.roots.size(), 5);
        const auto encoded = foldersnap::encodeConfiguration(config);
        QVERIFY(foldersnap::decodeConfiguration(encoded) == config);
        QVERIFY(encoded.contains("\n  \"roots\""));
        for (qsizetype i = 0; i < config.roots.size(); ++i)
        {
            QCOMPARE(static_cast<int>(config.roots[i].schedule.kind), static_cast<int>(i));
        }
        const foldersnap::Configuration defaults;
        QVERIFY(foldersnap::decodeConfiguration(foldersnap::encodeConfiguration(defaults)) ==
                defaults);
        auto legacyConfiguration = QJsonDocument::fromJson(fixture("config")).object();
        legacyConfiguration.remove("notifyScheduledBefore");
        legacyConfiguration["notifyScheduledSuccess"] = true;
        QVERIFY_EXCEPTION_THROWN(
            (void)foldersnap::decodeConfiguration(
                QJsonDocument(legacyConfiguration).toJson(QJsonDocument::Compact)),
            foldersnap::DomainError);
        QVERIFY(!config.roots.first().schedule.nextDueAtUtc);
        QCOMPARE(config.roots.last().schedule.dayOfMonth, 31);
    }

    void optionalFieldsAndRuleContext()
    {
        auto snapshot = foldersnap::decodeSnapshot(fixture("snapshot"));
        snapshot.header.trigger = foldersnap::SnapshotTrigger::Manual;
        snapshot.header.description.clear();
        snapshot.header.ignoreConfig.rules = {"# exact raw rules", " build/ ", "!build/keep.txt"};
        snapshot.header.ignoreConfig.hash =
            foldersnap::IgnoreMatcher::rulesHash(snapshot.header.ignoreConfig.rules);
        const auto encoded = foldersnap::encodeSnapshot(snapshot);
        QVERIFY(foldersnap::decodeSnapshot(encoded).header == snapshot.header);
        QVERIFY(!encoded.contains("description"));
        auto root = QJsonDocument::fromJson(encoded).object();
        auto entries = root["entries"].toArray();
        auto entry = entries.last().toObject();
        entry.remove("createdNs");
        entries[entries.size() - 1] = entry;
        root["entries"] = entries;
        QCOMPARE(foldersnap::decodeSnapshot(json(root)).entries.last().createdNs, Q_INT64_C(0));
        QVERIFY_EXCEPTION_THROWN((void)foldersnap::parseTimestamp("2026-09-15T00:00:00Z\n"),
                                 foldersnap::DomainError);
        QVERIFY_EXCEPTION_THROWN(foldersnap::validateUuid("11111111-1111-4111-7111-111111111111"),
                                 foldersnap::DomainError);
    }

    void historyRoundTrip()
    {
        auto records = foldersnap::decodeHistoryIndex(fixture("index"));
        QCOMPARE(records.size(), 1);
        QVERIFY(foldersnap::decodeHistoryIndex(foldersnap::encodeHistoryIndex(records)) == records);
        records[0].payloadAvailable = true;
        const auto encoded = foldersnap::encodeHistoryIndex(records);
        QVERIFY(!encoded.contains("payloadAvailable"));
        QVERIFY(!foldersnap::decodeHistoryIndex(encoded)[0].payloadAvailable);
        QCOMPARE(foldersnap::decodeHistoryIndex(encoded)[0].description,
                 QString("Updated note 🚀"));
        auto newer = records[0];
        newer.snapshotId = "33333333-3333-4333-8333-333333333333";
        newer.completedAtUtc.nanoseconds += 1;
        QCOMPARE(
            foldersnap::decodeHistoryIndex(foldersnap::encodeHistoryIndex({records[0], newer}))[0]
                .snapshotId,
            newer.snapshotId);
        QVERIFY_EXCEPTION_THROWN((void)foldersnap::encodeHistoryIndex({newer, newer}),
                                 foldersnap::DomainError);
        auto root = QJsonDocument::fromJson(encoded).object();
        auto list = root["records"].toArray();
        list.append(list.first());
        root["records"] = list;
        QVERIFY_EXCEPTION_THROWN((void)foldersnap::decodeHistoryIndex(json(root)),
                                 foldersnap::DomainError);
    }

    void timestamps()
    {
        const QList<qint64> values{std::numeric_limits<qint64>::min(),
                                   -1000000001,
                                   -1,
                                   0,
                                   1,
                                   Q_INT64_C(1789466400123456789),
                                   std::numeric_limits<qint64>::max()};
        for (const qint64 value : values)
        {
            QCOMPARE(foldersnap::parseTimestamp(foldersnap::formatTimestamp({value})).nanoseconds,
                     value);
        }
        QCOMPARE(foldersnap::parseTimestamp("2026-09-15T13:00:00.123456789+03:00").nanoseconds,
                 foldersnap::parseTimestamp("2026-09-15T10:00:00.123456789Z").nanoseconds);
        QCOMPARE(foldersnap::parseTimestamp("1970-01-01T00:00:00.1Z").nanoseconds,
                 Q_INT64_C(100000000));
        QCOMPARE(foldersnap::unixNanosecondsFromFileTime(116444736000000000ULL), Q_INT64_C(0));
        QCOMPARE(foldersnap::unixNanosecondsFromFileTime(116444736000000001ULL), Q_INT64_C(100));
        QCOMPARE(foldersnap::unixNanosecondsFromFileTime(116444735999999999ULL), Q_INT64_C(-100));
        QVERIFY_EXCEPTION_THROWN((void)foldersnap::unixNanosecondsFromFileTime(0),
                                 foldersnap::DomainError);
        for (const QString &text :
             QStringList{"", "2026-02-30T00:00:00Z", "2026-09-15T00:00:60Z", "2026-09-15T00:00:00",
                         "2026-09-15T00:00:00.1234567890Z", "2026-09-15T00:00:00+24:00",
                         "1600-01-01T00:00:00Z", "2300-01-01T00:00:00Z"})
        {
            QVERIFY_EXCEPTION_THROWN((void)foldersnap::parseTimestamp(text),
                                     foldersnap::DomainError);
        }
    }

    void idsAndDescriptions()
    {
        const QString first = foldersnap::createId();
        foldersnap::validateUuid(first);
        QVERIFY(first != foldersnap::createId());
        QCOMPARE(QUuid(first).version(), QUuid::Random);
        for (const QString &id : QStringList{"bad", "../id", "00000000-0000-0000-0000-000000000000",
                                             "{11111111-1111-4111-8111-111111111111}"})
        {
            QVERIFY_EXCEPTION_THROWN(foldersnap::validateUuid(id), foldersnap::DomainError);
        }
        foldersnap::validateDescription(QString("🚀").repeated(500));
        QVERIFY_EXCEPTION_THROWN(foldersnap::validateDescription(QString("🚀").repeated(501)),
                                 foldersnap::DomainError);
    }

    void malformedSnapshots_data()
    {
        QTest::addColumn<QString>("mutation");
        for (const QString &mutation : QStringList{
                 "schema", "header-schema", "count", "hash", "date", "trigger", "negative",
                 "fractional", "overflow", "duplicate", "traversal", "case", "display",
                 "directory-size", "link-target", "attributes", "missing", "boolean", "warning"})
        {
            QTest::newRow(qPrintable(mutation)) << mutation;
        }
    }

    void malformedSnapshots()
    {
        QFETCH(QString, mutation);
        auto root = QJsonDocument::fromJson(fixture("snapshot")).object();
        auto header = root["header"].toObject();
        auto entries = root["entries"].toArray();
        auto entry = entries[0].toObject();
        if (mutation == "schema")
        {
            root["schemaVersion"] = 1;
        }
        if (mutation == "header-schema")
        {
            header["schemaVersion"] = 3;
        }
        if (mutation == "count")
        {
            header["fileCount"] = 5;
        }
        if (mutation == "hash")
        {
            header["ignoreConfig"] = QJsonObject{{"rules", QJsonArray{}}, {"hash", "bad"}};
        }
        if (mutation == "date")
        {
            header["completedAtUtc"] = "2025-01-01T00:00:00Z";
        }
        if (mutation == "trigger")
        {
            header["trigger"] = "automatic";
        }
        if (mutation == "negative")
        {
            entry["size"] = -1;
        }
        if (mutation == "fractional")
        {
            entry["modifiedNs"] = 1.5;
        }
        if (mutation == "overflow")
        {
            entry["size"] = std::numeric_limits<qint64>::max();
        }
        if (mutation == "duplicate")
        {
            entries.append(entry);
        }
        if (mutation == "traversal")
        {
            entry["path"] = "../escape";
        }
        if (mutation == "case")
        {
            entry["path"] = "Empty.txt";
        }
        if (mutation == "display")
        {
            entry["displayPath"] = "Other.txt";
        }
        if (mutation == "directory-size")
        {
            entry["type"] = "directory";
            entry["size"] = 1;
        }
        if (mutation == "link-target")
        {
            entry["linkTarget"] = "C:/elsewhere";
        }
        if (mutation == "attributes")
        {
            entry["attributes"] = Q_INT64_C(4294967296);
        }
        if (mutation == "missing")
        {
            entry.remove("size");
        }
        if (mutation == "boolean")
        {
            entry["size"] = false;
        }
        if (mutation == "warning")
        {
            header["scanWarnings"] = QJsonArray{QJsonObject{{"path", "../escape"}}};
        }
        entries[0] = entry;
        root["entries"] = entries;
        root["header"] = header;
        QVERIFY_EXCEPTION_THROWN((void)foldersnap::decodeSnapshot(json(root)),
                                 foldersnap::DomainError);
    }

    void invalidConfiguration()
    {
        const auto original = foldersnap::decodeConfiguration(fixture("config"));
        auto config = original;
        config.roots.append(config.roots.first());
        QVERIFY_EXCEPTION_THROWN((void)foldersnap::encodeConfiguration(config),
                                 foldersnap::DomainError);
        config = original;
        config.roots[1].path = config.roots[0].path;
        config.roots[1].normalizedPath = config.roots[0].normalizedPath;
        QVERIFY_EXCEPTION_THROWN((void)foldersnap::encodeConfiguration(config),
                                 foldersnap::DomainError);
        config = original;
        config.roots[1].schedule.intervalHours = 2;
        QVERIFY_EXCEPTION_THROWN((void)foldersnap::encodeConfiguration(config),
                                 foldersnap::DomainError);
        config = original;
        config.roots[3].schedule.weekday = 7;
        QVERIFY_EXCEPTION_THROWN((void)foldersnap::encodeConfiguration(config),
                                 foldersnap::DomainError);
        config = original;
        config.roots[4].schedule.dayOfMonth = 0;
        QVERIFY_EXCEPTION_THROWN((void)foldersnap::encodeConfiguration(config),
                                 foldersnap::DomainError);
        config = original;
        config.defaultRetention = -1;
        QVERIFY_EXCEPTION_THROWN((void)foldersnap::encodeConfiguration(config),
                                 foldersnap::DomainError);
        auto root = QJsonDocument::fromJson(fixture("config")).object();
        auto roots = root["roots"].toArray();
        auto watched = roots[0].toObject();
        watched["schedule"] = QJsonObject{{"kind", "manual"}, {"intervalHours", 0}};
        roots[0] = watched;
        root["roots"] = roots;
        QVERIFY_EXCEPTION_THROWN((void)foldersnap::decodeConfiguration(json(root)),
                                 foldersnap::DomainError);
    }

    void malformedDocuments()
    {
        for (const QByteArray &invalid :
             QList<QByteArray>{"", "[]", "{", "{}", "{\"schemaVersion\":2.5}"})
        {
            QVERIFY_EXCEPTION_THROWN((void)foldersnap::decodeConfiguration(invalid),
                                     foldersnap::DomainError);
            QVERIFY_EXCEPTION_THROWN((void)foldersnap::decodeSnapshot(invalid),
                                     foldersnap::DomainError);
            QVERIFY_EXCEPTION_THROWN((void)foldersnap::decodeHistoryIndex(invalid),
                                     foldersnap::DomainError);
        }
        try
        {
            (void)foldersnap::decodeConfiguration(QByteArray(16 * 1024 * 1024 + 1, ' '));
            QFAIL("Oversized config accepted");
        }
        catch (const foldersnap::DomainError &error)
        {
            QCOMPARE(error.code(), foldersnap::ErrorCode::SizeLimit);
        }
        try
        {
            (void)foldersnap::decodeHistoryIndex("{\"schemaVersion\":1}");
            QFAIL("Old schema accepted");
        }
        catch (const foldersnap::DomainError &error)
        {
            QCOMPARE(error.code(), foldersnap::ErrorCode::UnsupportedSchema);
        }
    }
};

QTEST_GUILESS_MAIN(DomainTest)
#include "tst_domain.moc"
