#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

#include "application/ExportJob.h"
#include "diff/DiffEngine.h"
#include "domain/DomainError.h"
#include "domain/JsonCodec.h"
#include "export/ExportBuilder.h"
#include "storage/SnapshotStore.h"

namespace
{
foldersnap::Snapshot fixtureSnapshot()
{
    QFile fixture(QString(FOLDERSNAP_FIXTURE_DIR) + "/snapshot-v2.json");
    if (!fixture.open(QIODevice::ReadOnly))
    {
        qFatal("Could not open the snapshot fixture.");
    }
    return foldersnap::decodeSnapshot(fixture.readAll());
}

QByteArray exportTemplate()
{
    QFile file(QString(FOLDERSNAP_EXPORT_TEMPLATE));
    if (!file.open(QIODevice::ReadOnly))
    {
        qFatal("Could not open the export template.");
    }
    return file.readAll();
}
} // namespace

class ExportTest final : public QObject
{
    Q_OBJECT

  private slots:
    void snapshotDtoPreservesExactValuesAndUnknownDates()
    {
        foldersnap::Snapshot snapshot = fixtureSnapshot();
        snapshot.header.displayTitle = "</script><script>alert('blocked')</script>";
        const QByteArray before = foldersnap::encodeSnapshot(snapshot);

        const QJsonObject dto = foldersnap::ExportBuilder::snapshotDto(snapshot);

        QCOMPARE(dto.value("schemaVersion").toInt(), 1);
        QCOMPARE(dto.value("reportType").toString(), QString("snapshot"));
        const QJsonObject header = dto.value("header").toObject();
        QCOMPARE(header.value("rootTitle").toString(), snapshot.header.displayTitle);
        QCOMPARE(header.value("totalFileBytes").toString(), QString("9007199254740993"));
        const QJsonArray entries = dto.value("entries").toArray();
        QCOMPARE(entries.size(), snapshot.entries.size());
        QVERIFY(entries.first().toObject().value("createdAtUtc").isNull());
        QCOMPARE(entries.last().toObject().value("sizeBytes").toString(),
                 QString("9007199254740993"));
        QCOMPARE(foldersnap::encodeSnapshot(snapshot), before);
    }

    void snapshotCsvUsesBomAndRfc4180Quoting()
    {
        foldersnap::Snapshot snapshot = fixtureSnapshot();
        snapshot.entries[1].linkTarget = "target,\"quoted\"\nline";

        const QByteArray csv = foldersnap::ExportBuilder::snapshotCsv(snapshot);

        QCOMPARE(csv.left(3), QByteArray::fromHex("efbbbf"));
        QVERIFY(
            csv.contains("path,displayPath,type,sizeBytes,createdAtUtc,modifiedAtUtc,attributes,"
                         "linkTarget\r\n"));
        QVERIFY(csv.contains("\"target,\"\"quoted\"\"\nline\""));
        QVERIFY(csv.contains("9007199254740993"));
        QVERIFY(csv.endsWith("\r\n"));
    }

    void htmlInjectionEscapesScriptMarkupAndRemainsOffline()
    {
        foldersnap::Snapshot snapshot = fixtureSnapshot();
        snapshot.header.displayTitle =
            QString::fromUtf8("</script><script>alert('blocked')</script>&\xE2\x80\xA8line");
        const QJsonObject dto = foldersnap::ExportBuilder::snapshotDto(snapshot);

        const QByteArray html = foldersnap::ExportBuilder::htmlReport(dto, exportTemplate());

        QVERIFY(!html.contains("/* FOLDERSNAP_REPORT_DATA */"));
        QVERIFY(!html.contains("</script><script>alert"));
        QVERIFY(html.contains("\\u003C/script\\u003E\\u003Cscript\\u003E"));
        QVERIFY(html.contains("\\u0026\\u2028line"));
        QVERIFY(!html.contains("http://"));
        QVERIFY(!html.contains("https://"));
        QVERIFY(!html.contains(".innerHTML"));
        QVERIFY(html.contains("createDocumentFragment"));
        QVERIFY(html.contains("textContent"));

        const QByteArray opening = "<script id=\"foldersnap-data\" type=\"application/json\">";
        const qsizetype jsonStart = html.indexOf(opening) + opening.size();
        const qsizetype jsonEnd = html.indexOf("</script>", jsonStart);
        QVERIFY(jsonStart >= opening.size());
        QVERIFY(jsonEnd > jsonStart);
        QJsonParseError parseError;
        const QJsonDocument embedded =
            QJsonDocument::fromJson(html.mid(jsonStart, jsonEnd - jsonStart), &parseError);
        QCOMPARE(parseError.error, QJsonParseError::NoError);
        QCOMPARE(embedded.object(), dto);
    }

    void htmlTemplateRequiresOneMarker()
    {
        const QJsonObject dto{{"schemaVersion", 1}, {"reportType", "snapshot"}};
        QVERIFY_EXCEPTION_THROWN(foldersnap::ExportBuilder::htmlReport(dto, "<html></html>"),
                                 foldersnap::DomainError);
        const QByteArray duplicate = "/* FOLDERSNAP_REPORT_DATA *//* FOLDERSNAP_REPORT_DATA */";
        QVERIFY_EXCEPTION_THROWN(foldersnap::ExportBuilder::htmlReport(dto, duplicate),
                                 foldersnap::DomainError);
    }

    void comparisonDtoAndCsvContainBothSides()
    {
        const foldersnap::Snapshot before = fixtureSnapshot();
        foldersnap::Snapshot after = before;
        after.header.snapshotId = "33333333-3333-4333-8333-333333333333";
        after.header.startedAtUtc.nanoseconds += 10000000000LL;
        after.header.completedAtUtc.nanoseconds += 10000000000LL;
        after.entries.last().size += 7;
        after.entries.last().modifiedNs += 1;
        after.header.totalFileBytes += 7;
        const foldersnap::DiffResult diff = foldersnap::DiffEngine::compare(before, after);
        QCOMPARE(diff.summary.modifiedCount, qint64(1));

        const QJsonObject dto = foldersnap::ExportBuilder::comparisonDto(before, after, diff);
        QCOMPARE(dto.value("reportType").toString(), QString("comparison"));
        const QJsonObject entry = dto.value("entries").toArray().first().toObject();
        QCOMPARE(entry.value("change").toString(), QString("modified"));
        QCOMPARE(entry.value("before").toObject().value("sizeBytes").toString(),
                 QString("9007199254740993"));
        QCOMPARE(entry.value("after").toObject().value("sizeBytes").toString(),
                 QString("9007199254741000"));

        const QByteArray csv = foldersnap::ExportBuilder::comparisonCsv(before, after, diff);
        QCOMPARE(csv.left(3), QByteArray::fromHex("efbbbf"));
        QVERIFY(csv.contains("modified,metadata,file,file,9007199254740993,9007199254741000"));
    }

    void exportJobLoadsSnapshotAndPublishesCsvAtomically()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const foldersnap::StoragePaths paths =
            foldersnap::StoragePaths::fromDataDirectory(temporaryDirectory.filePath("data"));
        const foldersnap::Snapshot snapshot = fixtureSnapshot();
        const foldersnap::SnapshotStore store(paths);
        QVERIFY(store.saveSnapshot(snapshot) > 0);

        const QString destination = temporaryDirectory.filePath("reports/snapshot.csv");
        const foldersnap::ExportJobResult result =
            foldersnap::ExportJob::run({paths,
                                        snapshot.header.snapshotId,
                                        {},
                                        destination,
                                        foldersnap::ExportFormat::Csv,
                                        {}});

        QVERIFY(result.succeeded);
        QVERIFY(!result.cancelled);
        QVERIFY(result.error.isEmpty());
        QVERIFY(result.bytesWritten > 3);
        QFile output(destination);
        QVERIFY(output.open(QIODevice::ReadOnly));
        QCOMPARE(output.read(3), QByteArray::fromHex("efbbbf"));
    }

    void exportJobCancellationDoesNotReplaceExistingFile()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const foldersnap::StoragePaths paths =
            foldersnap::StoragePaths::fromDataDirectory(temporaryDirectory.filePath("data"));
        const foldersnap::Snapshot snapshot = fixtureSnapshot();
        const foldersnap::SnapshotStore store(paths);
        QVERIFY(store.saveSnapshot(snapshot) > 0);

        const QString destination = temporaryDirectory.filePath("snapshot.csv");
        QFile existing(destination);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        QCOMPARE(existing.write("keep"), qint64(4));
        existing.close();

        int checkpoints = 0;
        const foldersnap::ExportJobResult result = foldersnap::ExportJob::run(
            {paths, snapshot.header.snapshotId, {}, destination, foldersnap::ExportFormat::Csv, {}},
            [&checkpoints]() { return ++checkpoints == 3; });

        QVERIFY(!result.succeeded);
        QVERIFY(result.cancelled);
        QVERIFY(result.error.isEmpty());
        QVERIFY(existing.open(QIODevice::ReadOnly));
        QCOMPARE(existing.readAll(), QByteArray("keep"));
    }

    void exportJobReportsMissingPayloadWithoutPartialOutput()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const foldersnap::StoragePaths paths =
            foldersnap::StoragePaths::fromDataDirectory(temporaryDirectory.filePath("data"));
        const QString destination = temporaryDirectory.filePath("missing.html");

        const foldersnap::ExportJobResult result =
            foldersnap::ExportJob::run({paths,
                                        "99999999-9999-4999-8999-999999999999",
                                        {},
                                        destination,
                                        foldersnap::ExportFormat::Html,
                                        exportTemplate()});

        QVERIFY(!result.succeeded);
        QVERIFY(!result.cancelled);
        QVERIFY(result.error.contains("missing", Qt::CaseInsensitive));
        QVERIFY(!QFile::exists(destination));
    }
};

QTEST_MAIN(ExportTest)
#include "tst_export.moc"
