#include <algorithm>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "cleanup/CleanupPreflight.h"
#include "paths/WindowsPaths.h"
#include "scanner/MetadataScanner.h"

namespace
{
void writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(contents), contents.size());
}

foldersnap::ScanResult scan(const foldersnap::RootPath &root)
{
    foldersnap::ScanRequest request;
    request.rootId = foldersnap::createId();
    request.displayTitle = "Cleanup fixture";
    request.root = root;
    request.directoryWorkerCount = 1;
    return foldersnap::MetadataScanner::scan(request, {}, [] { return false; });
}

const foldersnap::CleanupPreflightItem *itemAt(const foldersnap::CleanupPreflightResult &result,
                                               const QString &path)
{
    const auto iterator = std::find_if(result.items.cbegin(), result.items.cend(),
                                       [&path](const foldersnap::CleanupPreflightItem &item)
                                       { return item.path == path; });
    return iterator == result.items.cend() ? nullptr : &*iterator;
}

QList<foldersnap::CleanupCandidate> candidatesFor(const foldersnap::Snapshot &snapshot,
                                                  const QStringList &paths)
{
    QList<foldersnap::CleanupCandidate> candidates;
    for (const QString &path : paths)
    {
        const auto iterator = std::find_if(snapshot.entries.cbegin(), snapshot.entries.cend(),
                                           [&path](const foldersnap::SnapshotEntry &entry)
                                           { return entry.path == path; });
        if (iterator != snapshot.entries.cend())
        {
            candidates.append({*iterator});
        }
    }
    return candidates;
}
} // namespace

class CleanupTest final : public QObject
{
    Q_OBJECT

  private slots:
    void selectedDirectoryExpandsAndReadyItemsPass()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        QVERIFY(QDir().mkpath(temporaryDirectory.filePath("folder")));
        writeFile(temporaryDirectory.filePath("folder/known.txt"), "known");
        const foldersnap::RootPath root = foldersnap::normalizeRootPath(temporaryDirectory.path());
        const foldersnap::ScanResult scanResult = scan(root);
        QVERIFY2(scanResult.error.isEmpty(), qPrintable(scanResult.error));

        const auto candidates = candidatesFor(scanResult.snapshot, {"folder", "folder/known.txt"});
        const foldersnap::CleanupPreflightResult result =
            foldersnap::CleanupPreflight::inspect(root, candidates, {"FOLDER"});

        QCOMPARE(result.items.size(), 2);
        QCOMPARE(result.summary.readyCount, 2);
        QCOMPARE(result.summary.blockedCount, 0);
        QCOMPARE(result.summary.alreadyMissingCount, 0);
        QVERIFY(itemAt(result, "folder"));
        QCOMPARE(itemAt(result, "folder")->status, foldersnap::CleanupStatus::Ready);
        QCOMPARE(itemAt(result, "folder/known.txt")->status, foldersnap::CleanupStatus::Ready);
    }

    void changedMetadataAndTypeAreBlocked()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        writeFile(temporaryDirectory.filePath("item.txt"), "before");
        const foldersnap::RootPath root = foldersnap::normalizeRootPath(temporaryDirectory.path());
        const foldersnap::ScanResult scanResult = scan(root);
        QVERIFY2(scanResult.error.isEmpty(), qPrintable(scanResult.error));
        const auto candidates = candidatesFor(scanResult.snapshot, {"item.txt"});

        writeFile(temporaryDirectory.filePath("item.txt"), "after with a different size");
        auto result = foldersnap::CleanupPreflight::inspect(root, candidates, {"item.txt"});
        QCOMPARE(itemAt(result, "item.txt")->status,
                 foldersnap::CleanupStatus::ChangedSinceSnapshot);

        QVERIFY(QFile::remove(temporaryDirectory.filePath("item.txt")));
        QVERIFY(QDir().mkpath(temporaryDirectory.filePath("item.txt")));
        result = foldersnap::CleanupPreflight::inspect(root, candidates, {"item.txt"});
        QCOMPARE(itemAt(result, "item.txt")->status, foldersnap::CleanupStatus::TypeChanged);
    }

    void missingEntriesAreNonBlocking()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        writeFile(temporaryDirectory.filePath("item.txt"), "before");
        const foldersnap::RootPath root = foldersnap::normalizeRootPath(temporaryDirectory.path());
        const foldersnap::ScanResult scanResult = scan(root);
        QVERIFY2(scanResult.error.isEmpty(), qPrintable(scanResult.error));
        const auto candidates = candidatesFor(scanResult.snapshot, {"item.txt"});
        QVERIFY(QFile::remove(temporaryDirectory.filePath("item.txt")));

        const auto result = foldersnap::CleanupPreflight::inspect(root, candidates, {"item.txt"});
        QCOMPARE(itemAt(result, "item.txt")->status, foldersnap::CleanupStatus::AlreadyMissing);
        QCOMPARE(result.summary.readyCount, 0);
        QCOMPARE(result.summary.blockedCount, 0);
        QCOMPARE(result.summary.alreadyMissingCount, 1);
    }

    void untrackedContentBlocksDirectory()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        QVERIFY(QDir().mkpath(temporaryDirectory.filePath("folder")));
        writeFile(temporaryDirectory.filePath("folder/known.txt"), "known");
        const foldersnap::RootPath root = foldersnap::normalizeRootPath(temporaryDirectory.path());
        const foldersnap::ScanResult scanResult = scan(root);
        QVERIFY2(scanResult.error.isEmpty(), qPrintable(scanResult.error));
        const auto candidates = candidatesFor(scanResult.snapshot, {"folder", "folder/known.txt"});
        writeFile(temporaryDirectory.filePath("folder/extra.txt"), "extra");

        const auto result = foldersnap::CleanupPreflight::inspect(root, candidates, {"folder"});
        QCOMPARE(itemAt(result, "folder")->status,
                 foldersnap::CleanupStatus::ContainsUntrackedContent);
        QCOMPARE(itemAt(result, "folder/known.txt")->status, foldersnap::CleanupStatus::Ready);
        QCOMPARE(result.summary.readyCount, 1);
        QCOMPARE(result.summary.blockedCount, 1);
    }

    void blockedDescendantBlocksSelectedDirectory()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        QVERIFY(QDir().mkpath(temporaryDirectory.filePath("folder")));
        writeFile(temporaryDirectory.filePath("folder/known.txt"), "before");
        const foldersnap::RootPath root = foldersnap::normalizeRootPath(temporaryDirectory.path());
        const foldersnap::ScanResult scanResult = scan(root);
        QVERIFY2(scanResult.error.isEmpty(), qPrintable(scanResult.error));
        const auto candidates = candidatesFor(scanResult.snapshot, {"folder", "folder/known.txt"});
        writeFile(temporaryDirectory.filePath("folder/known.txt"), "changed");

        const auto result = foldersnap::CleanupPreflight::inspect(root, candidates, {"folder"});
        QCOMPARE(itemAt(result, "folder/known.txt")->status,
                 foldersnap::CleanupStatus::ChangedSinceSnapshot);
        QCOMPARE(itemAt(result, "folder")->status, foldersnap::CleanupStatus::ChangedSinceSnapshot);
        QVERIFY(itemAt(result, "folder")->detail.contains("known.txt"));
        QCOMPARE(result.summary.readyCount, 0);
        QCOMPARE(result.summary.blockedCount, 2);
    }

    void invalidSelectionsAreBlocked()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const foldersnap::RootPath root = foldersnap::normalizeRootPath(temporaryDirectory.path());
        const auto result =
            foldersnap::CleanupPreflight::inspect(root, {}, {"../escape", "not-a-candidate"});

        QCOMPARE(result.items.size(), 2);
        QCOMPARE(result.summary.readyCount, 0);
        QCOMPARE(result.summary.blockedCount, 2);
        QCOMPARE(itemAt(result, "../escape")->status,
                 foldersnap::CleanupStatus::OutsideRootOrInvalid);
        QCOMPARE(itemAt(result, "not-a-candidate")->status,
                 foldersnap::CleanupStatus::OutsideRootOrInvalid);
    }

    void cancellationStopsInspection()
    {
        const auto result = foldersnap::CleanupPreflight::inspect({}, {}, {}, [] { return true; });
        QVERIFY(result.cancelled);
        QVERIFY(result.items.isEmpty());
    }
};

QTEST_GUILESS_MAIN(CleanupTest)
#include "tst_cleanup.moc"
