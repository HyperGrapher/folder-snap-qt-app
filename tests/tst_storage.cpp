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
#include "paths/WindowsPaths.h"
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

    void cancelledAtomicReplacementPreservesExistingFile()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QString path = temporaryDirectory.filePath("report.csv");
        foldersnap::replaceFileAtomically(path, "existing");
        int checkpoints = 0;

        const bool replaced = foldersnap::replaceFileAtomically(
            path, QByteArray(128 * 1024, 'x'), [&checkpoints]() { return ++checkpoints == 2; });

        QVERIFY(!replaced);
        QCOMPARE(foldersnap::readFileLimited(path, 8), QByteArray("existing"));
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

    void snapshotSaveCancellationStopsCompressionAndLeavesNoPayload()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const auto paths = foldersnap::StoragePaths::fromDataDirectory(temporaryDirectory.path());
        auto source = fixtureSnapshot();
        for (int index = 0; index < 10000; ++index)
        {
            source.header.scanWarnings.append(
                {QString("generated/%1").arg(index, 6, 10, QChar('0')),
                 foldersnap::WarningOperation::Stat, foldersnap::WarningCategory::Io,
                 QString("generated warning %1").arg(index)});
        }

        int checkpoints = 0;
        try
        {
            (void)foldersnap::SnapshotStore(paths).saveSnapshot(source, [&checkpoints]
                                                                { return ++checkpoints >= 4; });
            QFAIL("A cancelled snapshot save was accepted.");
        }
        catch (const foldersnap::DomainError &error)
        {
            QCOMPARE(error.code(), foldersnap::ErrorCode::Cancelled);
        }
        QVERIFY(checkpoints >= 4);
        QVERIFY(!foldersnap::SnapshotStore(paths).hasPayload(source.header.snapshotId));
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

    void deleteSnapshotRemovesIndexAndPayload()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const auto paths = foldersnap::StoragePaths::fromDataDirectory(temporaryDirectory.path());
        const auto source = fixtureSnapshot();
        const foldersnap::HistoryStore historyStore(paths);
        (void)historyStore.commitSnapshot(source, 0);
        historyStore.deleteSnapshot(source.header.snapshotId);

        QVERIFY(historyStore.loadHistory().isEmpty());
        QVERIFY(!foldersnap::SnapshotStore(paths).hasPayload(source.header.snapshotId));
        try
        {
            historyStore.deleteSnapshot(source.header.snapshotId);
            QFAIL("Deleting a missing history record was accepted.");
        }
        catch (const foldersnap::DomainError &error)
        {
            QCOMPARE(error.code(), foldersnap::ErrorCode::InvalidData);
        }
    }

    void clearRootHistoryResetsRootStatusAndPreservesOtherRoots()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const auto paths = foldersnap::StoragePaths::fromDataDirectory(temporaryDirectory.path());
        const auto source = fixtureSnapshot();
        const QString otherRootId = "33333333-3333-4333-8333-333333333333";
        foldersnap::Configuration configuration;
        foldersnap::WatchedRoot root;
        root.rootId = source.header.rootId;
        root.displayName = source.header.displayTitle;
        root.path = source.header.rootPathAtCapture;
        root.normalizedPath = foldersnap::normalizeRootPath(root.path).identityPath;
        root.lastSnapshotUtc = source.header.completedAtUtc;
        root.lastScanError = "previous warning";
        configuration.roots.append(root);
        foldersnap::ConfigurationStore(paths).saveConfiguration(configuration);

        const foldersnap::HistoryStore historyStore(paths);
        (void)historyStore.commitSnapshot(source, 0);
        (void)historyStore.commitSnapshot(snapshotAt(2, otherRootId), 0);
        historyStore.clearRootHistory(source.header.rootId);

        QVERIFY(historyStore.loadHistoryForRoot(source.header.rootId).isEmpty());
        QCOMPARE(historyStore.loadHistoryForRoot(otherRootId).size(), 1);
        const auto loadedConfiguration =
            foldersnap::ConfigurationStore(paths).loadConfiguration().value;
        QVERIFY(!loadedConfiguration.roots.first().lastSnapshotUtc);
        QVERIFY(loadedConfiguration.roots.first().lastScanError.isEmpty());
    }

    void clearRootHistoryWithoutRecordsStillResetsRootStatus()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const auto paths = foldersnap::StoragePaths::fromDataDirectory(temporaryDirectory.path());
        const auto source = fixtureSnapshot();
        foldersnap::WatchedRoot root;
        root.rootId = source.header.rootId;
        root.displayName = source.header.displayTitle;
        root.path = source.header.rootPathAtCapture;
        root.normalizedPath = foldersnap::normalizeRootPath(root.path).identityPath;
        root.lastSnapshotUtc = source.header.completedAtUtc;
        root.lastScanError = "stale warning";
        foldersnap::Configuration configuration;
        configuration.roots.append(root);
        foldersnap::ConfigurationStore(paths).saveConfiguration(configuration);

        foldersnap::HistoryStore(paths).clearRootHistory(root.rootId);

        const auto persisted = foldersnap::ConfigurationStore(paths).loadConfiguration().value;
        QVERIFY(!persisted.roots.first().lastSnapshotUtc);
        QVERIFY(persisted.roots.first().lastScanError.isEmpty());
    }

    void removeWatchedRootDeletesOnlyItsConfigurationAndHistory()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const auto paths = foldersnap::StoragePaths::fromDataDirectory(temporaryDirectory.path());
        const auto source = fixtureSnapshot();
        const QString otherRootId = "33333333-3333-4333-8333-333333333333";
        const auto otherSnapshot = snapshotAt(2, otherRootId);

        foldersnap::WatchedRoot root;
        root.rootId = source.header.rootId;
        root.displayName = source.header.displayTitle;
        root.path = source.header.rootPathAtCapture;
        root.normalizedPath = foldersnap::normalizeRootPath(root.path).identityPath;
        foldersnap::WatchedRoot otherRoot;
        otherRoot.rootId = otherRootId;
        otherRoot.displayName = "Other";
        otherRoot.path = "C:/Other";
        otherRoot.normalizedPath = foldersnap::normalizeRootPath(otherRoot.path).identityPath;
        foldersnap::Configuration configuration;
        configuration.roots = {root, otherRoot};
        foldersnap::ConfigurationStore(paths).saveConfiguration(configuration);

        const foldersnap::HistoryStore historyStore(paths);
        (void)historyStore.commitSnapshot(source, 0);
        (void)historyStore.commitSnapshot(otherSnapshot, 0);

        historyStore.removeWatchedRoot(root.rootId);

        const foldersnap::Configuration persisted =
            foldersnap::ConfigurationStore(paths).loadConfiguration().value;
        QCOMPARE(persisted.roots, QList<foldersnap::WatchedRoot>{otherRoot});
        QVERIFY(historyStore.loadHistoryForRoot(root.rootId).isEmpty());
        QCOMPARE(historyStore.loadHistoryForRoot(otherRootId).size(), 1);
        const foldersnap::SnapshotStore snapshotStore(paths);
        QVERIFY(!snapshotStore.hasPayload(source.header.snapshotId));
        QVERIFY(snapshotStore.hasPayload(otherSnapshot.header.snapshotId));
    }

    void repairRestoresTombstonesAndRebuildsCorruptIndex()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const auto paths = foldersnap::StoragePaths::fromDataDirectory(temporaryDirectory.path());
        const auto source = snapshotAt(7);
        const foldersnap::SnapshotStore snapshotStore(paths);
        QVERIFY(snapshotStore.saveSnapshot(source) > 0);
        QVERIFY(snapshotStore.movePayloadToTombstone(source.header.snapshotId));
        foldersnap::replaceFileAtomically(paths.historyIndexFile, "corrupt index");

        const auto result = foldersnap::HistoryStore(paths).repair();
        QCOMPARE(result.restoredTombstones, 1);
        QCOMPARE(result.addedRecords, 1);
        const auto records = foldersnap::HistoryStore(paths).loadHistory();
        QCOMPARE(records.size(), 1);
        QCOMPARE(records.first().snapshotId, source.header.snapshotId);
        QVERIFY(records.first().payloadAvailable);
        QVERIFY(!QFile::exists(snapshotStore.tombstonePath(source.header.snapshotId)));
    }

    void repairRemovesUnreferencedTombstonesAndAddsOrphans()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const auto paths = foldersnap::StoragePaths::fromDataDirectory(temporaryDirectory.path());
        const auto orphan = snapshotAt(8);
        const foldersnap::SnapshotStore snapshotStore(paths);
        QVERIFY(snapshotStore.saveSnapshot(orphan) > 0);
        QVERIFY(snapshotStore.movePayloadToTombstone(orphan.header.snapshotId));
        foldersnap::ConfigurationStore(paths).saveHistoryIndex({});
        const auto result = foldersnap::HistoryStore(paths).repair();
        QCOMPARE(result.removedTombstones, 1);
        QCOMPARE(result.addedRecords, 0);
        QVERIFY(foldersnap::HistoryStore(paths).loadHistory().isEmpty());
    }
};

QTEST_MAIN(StorageTest)
#include "tst_storage.moc"
