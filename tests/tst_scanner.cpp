#include <algorithm>

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

#include "domain/Snapshot.h"
#include "paths/WindowsPaths.h"
#include "scanner/MetadataScanner.h"

namespace
{
void writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(contents), contents.size());
}
} // namespace

class ScannerTest final : public QObject
{
    Q_OBJECT

  private slots:
    void capturesMetadataAndAppliesIgnores()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        QVERIFY(QDir().mkpath(temporaryDirectory.filePath("src/nested")));
        QVERIFY(QDir().mkpath(temporaryDirectory.filePath("build")));
        writeFile(temporaryDirectory.filePath("README.md"), "hello");
        writeFile(temporaryDirectory.filePath("src/nested/data.bin"), "123456");
        writeFile(temporaryDirectory.filePath("build/ignored.obj"), "ignored");

        foldersnap::ScanRequest request;
        request.rootId = foldersnap::createId();
        request.displayTitle = "Fixture";
        request.root = foldersnap::normalizeRootPath(temporaryDirectory.path());
        request.ignoreRules = {"build/"};

        int lastProgress = 0;
        const foldersnap::ScanResult result = foldersnap::MetadataScanner::scan(
            request, [&lastProgress](int progress) { lastProgress = progress; },
            [] { return false; });

        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QVERIFY(!result.cancelled);
        QCOMPARE(result.snapshot.header.rootId, request.rootId);
        QCOMPARE(result.snapshot.header.displayTitle, QString("Fixture"));
        QCOMPARE(result.snapshot.header.fileCount, qint64(2));
        QCOMPARE(result.snapshot.header.totalFileBytes, qint64(11));
        QCOMPARE(result.snapshot.header.directoryCount, qint64(2));
        QCOMPARE(lastProgress, 100);
        QVERIFY(std::none_of(result.snapshot.entries.cbegin(), result.snapshot.entries.cend(),
                             [](const foldersnap::SnapshotEntry &entry)
                             { return entry.path.startsWith("build/"); }));
        QVERIFY(std::is_sorted(
            result.snapshot.entries.cbegin(), result.snapshot.entries.cend(),
            [](const foldersnap::SnapshotEntry &left, const foldersnap::SnapshotEntry &right)
            { return left.path < right.path; }));
        QVERIFY(std::all_of(result.snapshot.entries.cbegin(), result.snapshot.entries.cend(),
                            [](const foldersnap::SnapshotEntry &entry)
                            { return entry.modifiedNs >= 0 && entry.createdNs >= 0; }));
    }

    void cancellationStopsTraversal()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        writeFile(temporaryDirectory.filePath("file.txt"), "data");

        foldersnap::ScanRequest request;
        request.rootId = foldersnap::createId();
        request.displayTitle = "Cancelled";
        request.root = foldersnap::normalizeRootPath(temporaryDirectory.path());
        const foldersnap::ScanResult result =
            foldersnap::MetadataScanner::scan(request, {}, [] { return true; });

        QVERIFY(result.cancelled);
        QVERIFY(result.error.isEmpty());
    }

#ifdef Q_OS_WIN
    void doesNotTraverseDirectoryJunctions()
    {
        QTemporaryDir watchedDirectory;
        QTemporaryDir junctionTarget;
        QVERIFY(watchedDirectory.isValid());
        QVERIFY(junctionTarget.isValid());
        writeFile(junctionTarget.filePath("outside.txt"), "must not be counted");

        const QString junctionPath = watchedDirectory.filePath("external");
        QProcess process;
        process.start("cmd.exe",
                      {"/D", "/C", "mklink", "/J", QDir::toNativeSeparators(junctionPath),
                       QDir::toNativeSeparators(junctionTarget.path())});
        QVERIFY(process.waitForFinished());
        QCOMPARE(process.exitCode(), 0);

        foldersnap::ScanRequest request;
        request.rootId = foldersnap::createId();
        request.displayTitle = "Junction";
        request.root = foldersnap::normalizeRootPath(watchedDirectory.path());
        const foldersnap::ScanResult result =
            foldersnap::MetadataScanner::scan(request, {}, [] { return false; });

        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.snapshot.header.fileCount, qint64(0));
        QCOMPARE(result.snapshot.header.totalFileBytes, qint64(0));
        QCOMPARE(result.snapshot.entries.size(), 1);
        QCOMPARE(result.snapshot.entries.first().path, QString("external"));
        QCOMPARE(result.snapshot.entries.first().type, foldersnap::EntryType::Reparse);
    }
#endif

    void workerCountsProduceTheSameSnapshot()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        QVERIFY(QDir().mkpath(temporaryDirectory.filePath("one/two")));
        writeFile(temporaryDirectory.filePath("top.txt"), "top");
        writeFile(temporaryDirectory.filePath("one/child.txt"), "child");
        writeFile(temporaryDirectory.filePath("one/two/leaf.txt"), "leaf");

        foldersnap::ScanRequest request;
        request.rootId = foldersnap::createId();
        request.displayTitle = "Workers";
        request.root = foldersnap::normalizeRootPath(temporaryDirectory.path());
        request.directoryWorkerCount = 1;
        const foldersnap::ScanResult singleWorker =
            foldersnap::MetadataScanner::scan(request, {}, [] { return false; });

        request.directoryWorkerCount = 4;
        const foldersnap::ScanResult fourWorkers =
            foldersnap::MetadataScanner::scan(request, {}, [] { return false; });

        QVERIFY2(singleWorker.error.isEmpty(), qPrintable(singleWorker.error));
        QVERIFY2(fourWorkers.error.isEmpty(), qPrintable(fourWorkers.error));
        const auto entryDescriptions = [](const QList<foldersnap::SnapshotEntry> &entries)
        {
            QStringList descriptions;
            for (const foldersnap::SnapshotEntry &entry : entries)
            {
                descriptions.append(QString("%1|%2|%3|%4|%5")
                                        .arg(entry.path)
                                        .arg(static_cast<int>(entry.type))
                                        .arg(entry.size)
                                        .arg(entry.attributes)
                                        .arg(entry.linkTarget));
            }
            return descriptions;
        };
        QCOMPARE(entryDescriptions(singleWorker.snapshot.entries),
                 entryDescriptions(fourWorkers.snapshot.entries));
        QCOMPARE(singleWorker.snapshot.header.scanWarnings,
                 fourWorkers.snapshot.header.scanWarnings);
    }

    void rejectsOutOfRangeWorkerCount()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        foldersnap::ScanRequest request;
        request.rootId = foldersnap::createId();
        request.root = foldersnap::normalizeRootPath(temporaryDirectory.path());
        request.directoryWorkerCount = 33;
        const foldersnap::ScanResult result =
            foldersnap::MetadataScanner::scan(request, {}, [] { return false; });

        QCOMPARE(result.error, QString("The directory worker count must be between 1 and 32."));
    }
};

QTEST_MAIN(ScannerTest)
#include "tst_scanner.moc"
