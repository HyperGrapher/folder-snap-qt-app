#include <algorithm>

#include <QDir>
#include <QFile>
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
};

QTEST_MAIN(ScannerTest)
#include "tst_scanner.moc"
