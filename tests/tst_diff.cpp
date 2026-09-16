#include <QTest>

#include "diff/DiffEngine.h"

namespace
{
foldersnap::Snapshot makeSnapshot(const QString &id, qint64 completedNs,
                                  const QList<foldersnap::SnapshotEntry> &entries,
                                  const QStringList &ignoreRules = {})
{
    foldersnap::Snapshot snapshot;
    snapshot.header.snapshotId = id;
    snapshot.header.rootId = "11111111-1111-4111-8111-111111111111";
    snapshot.header.completedAtUtc.nanoseconds = completedNs;
    snapshot.header.ignoreConfig.rules = ignoreRules;
    snapshot.header.scanWarnings = {};
    snapshot.header.totalFileBytes = 0;
    for (const foldersnap::SnapshotEntry &entry : entries)
    {
        snapshot.header.totalFileBytes += entry.type == foldersnap::EntryType::File ? entry.size : 0;
    }
    snapshot.entries = entries;
    return snapshot;
}

foldersnap::SnapshotEntry file(const QString &path, qint64 size, qint64 modified)
{
    foldersnap::SnapshotEntry entry;
    entry.path = path;
    entry.displayPath = path;
    entry.type = foldersnap::EntryType::File;
    entry.size = size;
    entry.modifiedNs = modified;
    return entry;
}
} // namespace

class DiffTest final : public QObject
{
    Q_OBJECT

  private slots:
    void classifiesEntriesAndIgnoresCreationOnlyChanges()
    {
        const foldersnap::Snapshot before = makeSnapshot(
            "11111111-1111-4111-8111-111111111111", 10,
            {file("file2.txt", 2, 1), file("same.txt", 3, 1), file("removed.txt", 4, 1)});
        auto modified = file("file2.txt", 5, 2);
        auto same = file("same.txt", 3, 1);
        same.createdNs = 99;
        foldersnap::Snapshot after = makeSnapshot(
            "22222222-2222-4222-8222-222222222222", 20,
            {modified, same, file("file10.txt", 10, 1)});

        const foldersnap::DiffResult result = foldersnap::DiffEngine::compare(before, after);
        QCOMPARE(result.summary.addedCount, qint64(1));
        QCOMPARE(result.summary.removedCount, qint64(1));
        QCOMPARE(result.summary.modifiedCount, qint64(1));
        QCOMPARE(result.summary.unchangedCount, qint64(1));
        QCOMPARE(result.summary.addedFileBytes, qint64(10));
        QCOMPARE(result.summary.removedFileBytes, qint64(4));
        QCOMPARE(result.summary.modifiedBeforeFileBytes, qint64(2));
        QCOMPARE(result.summary.modifiedAfterFileBytes, qint64(5));
        QCOMPARE(result.entries.at(0).path, QString("file10.txt"));
        QCOMPARE(result.entries.at(1).path, QString("removed.txt"));
        QCOMPARE(result.entries.at(2).path, QString("file2.txt"));
    }

    void swapsReverseChronologicalInputs()
    {
        const foldersnap::Snapshot older = makeSnapshot(
            "11111111-1111-4111-8111-111111111111", 10, {file("old.txt", 1, 1)});
        const foldersnap::Snapshot newer = makeSnapshot(
            "22222222-2222-4222-8222-222222222222", 20, {file("new.txt", 2, 1)});
        const foldersnap::DiffResult result = foldersnap::DiffEngine::compare(newer, older);

        QCOMPARE(result.summary.addedCount, qint64(1));
        QCOMPARE(result.summary.removedCount, qint64(1));
        QCOMPARE(result.summary.netFileBytes, qint64(1));
    }

    void warningMakesMissingEntryUncertain()
    {
        const foldersnap::Snapshot before = makeSnapshot(
            "11111111-1111-4111-8111-111111111111", 10, {file("folder/file.txt", 2, 1)});
        foldersnap::Snapshot after = makeSnapshot(
            "22222222-2222-4222-8222-222222222222", 20, {});
        after.header.scanWarnings.append({"folder", foldersnap::WarningOperation::Enumerate,
                                          foldersnap::WarningCategory::AccessDenied, "denied"});

        const foldersnap::DiffResult result = foldersnap::DiffEngine::compare(before, after);
        QCOMPARE(result.summary.uncertainCount, qint64(1));
        QCOMPARE(result.summary.removedCount, qint64(0));
    }

    void cancellationReturnsAnIncompleteResult()
    {
        const foldersnap::Snapshot before = makeSnapshot(
            "11111111-1111-4111-8111-111111111111", 10, {file("a.txt", 1, 1)});
        const foldersnap::Snapshot after = makeSnapshot(
            "22222222-2222-4222-8222-222222222222", 20, {file("b.txt", 1, 1)});
        int checks = 0;
        const foldersnap::DiffResult result = foldersnap::DiffEngine::compare(
            before, after, [&checks]() { return ++checks > 1; });

        QVERIFY(result.cancelled);
        QVERIFY(result.entries.isEmpty());
    }
};

QTEST_MAIN(DiffTest)
#include "tst_diff.moc"
