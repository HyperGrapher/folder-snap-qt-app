#include <atomic>
#include <latch>
#include <mutex>
#include <thread>
#include <vector>

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "domain/DomainError.h"
#include "domain/JsonCodec.h"
#include "storage/AtomicFile.h"
#include "storage/ConfigurationStore.h"
#include "storage/HistoryStore.h"
#include "storage/SnapshotStore.h"

namespace
{
foldersnap::HistoryRecord historyRecord()
{
    return {
        "11111111-1111-4111-8111-111111111111",
        "22222222-2222-4222-8222-222222222222",
        "C:/Projects",
        "Projects",
        {Q_INT64_C(1789466400123456789)},
        foldersnap::SnapshotTrigger::Manual,
        "First snapshot",
        12,
        4,
        0,
        9007199254740993,
        0,
        1234,
    };
}

foldersnap::Snapshot fixtureSnapshot()
{
    QFile fixtureFile(QString(FOLDERSNAP_FIXTURE_DIR) + "/snapshot-v2.json");
    if (!fixtureFile.open(QIODevice::ReadOnly))
    {
        qFatal("Could not open the snapshot fixture.");
    }
    return foldersnap::decodeSnapshot(fixtureFile.readAll());
}

QString snapshotId(int sequence)
{
    return QString("00000000-0000-4000-8000-%1").arg(sequence, 12, 10, QChar('0'));
}

foldersnap::Snapshot snapshotAt(int sequence, const QString &rootId = {})
{
    auto snapshot = fixtureSnapshot();
    snapshot.header.snapshotId = snapshotId(sequence);
    if (!rootId.isEmpty())
    {
        snapshot.header.rootId = rootId;
    }
    snapshot.header.startedAtUtc.nanoseconds += sequence;
    snapshot.header.completedAtUtc.nanoseconds += sequence;
    return snapshot;
}
} // namespace

class StorageTest final : public QObject
{
    Q_OBJECT

  private slots:
    void pathsRequireAbsoluteDirectory()
    {
        QVERIFY_EXCEPTION_THROWN(foldersnap::StoragePaths::fromDataDirectory("relative"),
                                 foldersnap::DomainError);
        const auto paths = foldersnap::StoragePaths::fromDataDirectory("C:/FolderSnap Test");
        QCOMPARE(paths.configurationFile, QString("C:/FolderSnap Test/configuration.json"));
        QCOMPARE(paths.historyIndexFile, QString("C:/FolderSnap Test/history-index.json"));
    }

    void atomicReplacementCreatesParentAndReplacesContents()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QString path = temporaryDirectory.filePath("nested/state.json");
        foldersnap::replaceFileAtomically(path, "one");
        QCOMPARE(foldersnap::readFileLimited(path, 3), QByteArray("one"));
        foldersnap::replaceFileAtomically(path, "two");
        QCOMPARE(foldersnap::readFileLimited(path, 3), QByteArray("two"));
        QVERIFY_EXCEPTION_THROWN(foldersnap::readFileLimited(path, 2), foldersnap::DomainError);
    }

    void configurationRoundTripsAndMissingConfigurationUsesDefaults()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const foldersnap::ConfigurationStore store(
            foldersnap::StoragePaths::fromDataDirectory(temporaryDirectory.path()));
        const auto missing = store.loadConfiguration();
        QVERIFY(!missing.restoredDefaults());
        QVERIFY(missing.value == foldersnap::Configuration{});

        auto configuration = foldersnap::Configuration{};
        configuration.launchAtStartup = true;
        store.saveConfiguration(configuration);
        const auto loaded = store.loadConfiguration();
        QVERIFY(!loaded.restoredDefaults());
        QVERIFY(loaded.value == configuration);
    }

    void malformedConfigurationIsPreservedAndDefaultsReturned()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const foldersnap::ConfigurationStore store(
            foldersnap::StoragePaths::fromDataDirectory(temporaryDirectory.path()));
        foldersnap::replaceFileAtomically(store.paths().configurationFile, "not json");

        const auto result = store.loadConfiguration();
        QVERIFY(result.restoredDefaults());
        QVERIFY(result.value == foldersnap::Configuration{});
        QVERIFY(!QFile::exists(store.paths().configurationFile));
        QVERIFY(QFile::exists(result.preservedCorruptFile));
        QCOMPARE(foldersnap::readFileLimited(result.preservedCorruptFile, 16),
                 QByteArray("not json"));
    }

    void historyRoundTripsAndCorruptionIsPreserved()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const foldersnap::ConfigurationStore store(
            foldersnap::StoragePaths::fromDataDirectory(temporaryDirectory.path()));
        const QList<foldersnap::HistoryRecord> records{historyRecord()};
        store.saveHistoryIndex(records);
        QVERIFY(store.loadHistoryIndex().value == records);

        foldersnap::replaceFileAtomically(store.paths().historyIndexFile, "[]");
        const auto result = store.loadHistoryIndex();
        QVERIFY(result.restoredDefaults());
        QVERIFY(result.value.isEmpty());
        QVERIFY(QFile::exists(result.preservedCorruptFile));
    }

    void snapshotPayloadRoundTripsAsGzip()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const auto paths = foldersnap::StoragePaths::fromDataDirectory(temporaryDirectory.path());
        QFile fixtureFile(QString(FOLDERSNAP_FIXTURE_DIR) + "/snapshot-v2.json");
        QVERIFY(fixtureFile.open(QIODevice::ReadOnly));
        const auto source = foldersnap::decodeSnapshot(fixtureFile.readAll());
        const foldersnap::SnapshotStore store(paths);

        const qint64 compressedBytes = store.saveSnapshot(source);
        QVERIFY(compressedBytes > 0);
        const QString path = store.payloadPath(source.header.snapshotId);
        QVERIFY(QFile::exists(path));
        const QByteArray payload = foldersnap::readFileLimited(path, 1024);
        QCOMPARE(payload.left(2), QByteArray::fromHex("1f8b"));
        QVERIFY(store.hasPayload(source.header.snapshotId));
        QVERIFY(store.loadSnapshot(source.header.snapshotId).header == source.header);
        QVERIFY(store.loadSnapshot(source.header.snapshotId).entries == source.entries);
        QCOMPARE(
            [&]()
            {
                try
                {
                    (void)store.loadSnapshot("33333333-3333-4333-8333-333333333333");
                }
                catch (const foldersnap::DomainError &error)
                {
                    return error.code();
                }
                return foldersnap::ErrorCode::InvalidData;
            }(),
            foldersnap::ErrorCode::MissingPayload);
    }

    void snapshotDecodeLimitAndTruncationAreRejected()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const auto paths = foldersnap::StoragePaths::fromDataDirectory(temporaryDirectory.path());
        QFile fixtureFile(QString(FOLDERSNAP_FIXTURE_DIR) + "/snapshot-v2.json");
        QVERIFY(fixtureFile.open(QIODevice::ReadOnly));
        const auto source = foldersnap::decodeSnapshot(fixtureFile.readAll());
        const foldersnap::SnapshotStore limitedStore(paths, 32);
        QVERIFY(limitedStore.saveSnapshot(source) > 0);
        try
        {
            (void)limitedStore.loadSnapshot(source.header.snapshotId);
            QFAIL("An oversized decoded payload was accepted.");
        }
        catch (const foldersnap::DomainError &error)
        {
            QCOMPARE(error.code(), foldersnap::ErrorCode::SizeLimit);
        }

        const QString path = limitedStore.payloadPath(source.header.snapshotId);
        QByteArray truncated = foldersnap::readFileLimited(path, 1024 * 1024);
        QVERIFY(truncated.size() > 8);
        truncated.chop(4);
        foldersnap::replaceFileAtomically(path, truncated);
        const foldersnap::SnapshotStore normalStore(paths);
        try
        {
            (void)normalStore.loadSnapshot(source.header.snapshotId);
            QFAIL("A truncated gzip payload was accepted.");
        }
        catch (const foldersnap::DomainError &error)
        {
            QCOMPARE(error.code(), foldersnap::ErrorCode::InvalidData);
        }
    }

    void historyReportsPayloadAvailability()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const auto paths = foldersnap::StoragePaths::fromDataDirectory(temporaryDirectory.path());
        QFile fixtureFile(QString(FOLDERSNAP_FIXTURE_DIR) + "/snapshot-v2.json");
        QVERIFY(fixtureFile.open(QIODevice::ReadOnly));
        const auto source = foldersnap::decodeSnapshot(fixtureFile.readAll());
        const foldersnap::ConfigurationStore configurationStore(paths);
        configurationStore.saveHistoryIndex({historyRecord()});
        QVERIFY(!configurationStore.loadHistoryIndex().value.first().payloadAvailable);

        const foldersnap::SnapshotStore snapshotStore(paths);
        QVERIFY(snapshotStore.saveSnapshot(source) > 0);
        const auto loaded = configurationStore.loadHistoryIndex();
        QVERIFY(loaded.value.first().payloadAvailable);
    }

    void descriptionEditsDoNotMutateSnapshotPayload()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const auto paths = foldersnap::StoragePaths::fromDataDirectory(temporaryDirectory.path());
        const foldersnap::Snapshot source = fixtureSnapshot();
        const foldersnap::HistoryStore historyStore(paths);
        QCOMPARE(historyStore.commitSnapshot(source, 0).record.snapshotId,
                 source.header.snapshotId);
        const QString payloadPath =
            foldersnap::SnapshotStore(paths).payloadPath(source.header.snapshotId);
        const QByteArray payloadBefore = foldersnap::readFileLimited(payloadPath, 1024 * 1024);

        historyStore.updateDescription(source.header.snapshotId, "A later note ✨");

        const auto records = historyStore.loadHistory();
        QCOMPARE(records.size(), 1);
        QCOMPARE(records.first().description, QString("A later note ✨"));
        QCOMPARE(foldersnap::readFileLimited(payloadPath, 1024 * 1024), payloadBefore);
        QCOMPARE(foldersnap::SnapshotStore(paths)
                     .loadSnapshot(source.header.snapshotId)
                     .header.description,
                 source.header.description);
    }

    void retentionPrunesOnlyTheSavedRootsOldestPayload()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const auto paths = foldersnap::StoragePaths::fromDataDirectory(temporaryDirectory.path());
        const foldersnap::HistoryStore historyStore(paths);
        for (int sequence = 1; sequence <= 11; ++sequence)
        {
            QCOMPARE(historyStore.commitSnapshot(snapshotAt(sequence), 10).record.snapshotId,
                     snapshotId(sequence));
        }
        const QString otherRootId = "33333333-3333-4333-8333-333333333333";
        const auto other = snapshotAt(100, otherRootId);
        QCOMPARE(historyStore.commitSnapshot(other, 10).record.snapshotId, other.header.snapshotId);

        const auto primary = historyStore.loadHistoryForRoot(fixtureSnapshot().header.rootId);
        QCOMPARE(primary.size(), 10);
        QVERIFY(!foldersnap::SnapshotStore(paths).hasPayload(snapshotId(1)));
        QVERIFY(!QFile::exists(foldersnap::SnapshotStore(paths).tombstonePath(snapshotId(1))));
        QCOMPARE(historyStore.loadHistoryForRoot(otherRootId).size(), 1);
        QVERIFY(foldersnap::SnapshotStore(paths).hasPayload(other.header.snapshotId));
    }

    void concurrentSnapshotCommitsDoNotLoseHistory()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const auto paths = foldersnap::StoragePaths::fromDataDirectory(temporaryDirectory.path());
        constexpr int writerCount = 16;
        std::latch startGate(1);
        std::mutex errorMutex;
        QStringList errors;
        std::vector<std::thread> writers;
        writers.reserve(writerCount);
        for (int sequence = 1; sequence <= writerCount; ++sequence)
        {
            writers.emplace_back(
                [&, sequence]()
                {
                    startGate.wait();
                    try
                    {
                        (void)foldersnap::HistoryStore(paths).commitSnapshot(snapshotAt(sequence),
                                                                             0);
                    }
                    catch (const std::exception &error)
                    {
                        const std::scoped_lock lock(errorMutex);
                        errors.append(QString::fromUtf8(error.what()));
                    }
                });
        }
        startGate.count_down();
        for (std::thread &writer : writers)
        {
            writer.join();
        }

        QVERIFY2(errors.isEmpty(), qPrintable(errors.join('\n')));
        const auto records = foldersnap::HistoryStore(paths).loadHistory();
        QCOMPARE(records.size(), writerCount);
        for (int sequence = 1; sequence <= writerCount; ++sequence)
        {
            QVERIFY(foldersnap::SnapshotStore(paths).hasPayload(snapshotId(sequence)));
        }
    }
};

QTEST_MAIN(StorageTest)
#include "tst_storage.moc"
