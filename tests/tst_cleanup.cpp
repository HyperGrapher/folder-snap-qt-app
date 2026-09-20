#include <algorithm>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#ifdef Q_OS_WIN
#include <QProcess>
#endif
#include <QTemporaryDir>
#include <QtTest>

#include "cleanup/CleanupExecutor.h"
#include "cleanup/CleanupPreflight.h"
#include "domain/DomainError.h"
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

const foldersnap::CleanupExecutionItem *
executionItemAt(const foldersnap::CleanupExecutionResult &result, const QString &path)
{
    const auto iterator = std::find_if(result.items.cbegin(), result.items.cend(),
                                       [&path](const foldersnap::CleanupExecutionItem &item)
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

    void executorRejectsTraversalWithoutMove()
    {
        QTemporaryDir dataDirectory;
        QTemporaryDir temporaryDirectory;
        QVERIFY(dataDirectory.isValid());
        QVERIFY(temporaryDirectory.isValid());

        foldersnap::SnapshotEntry entry;
        entry.path = "../escape";
        entry.displayPath = entry.path;
        entry.type = foldersnap::EntryType::File;

        foldersnap::CleanupExecutionRequest request;
        request.paths = foldersnap::StoragePaths::fromDataDirectory(dataDirectory.path());
        request.root = foldersnap::normalizeRootPath(temporaryDirectory.path());
        request.rootId = foldersnap::createId();
        request.beforeId = foldersnap::createId();
        request.afterId = foldersnap::createId();
        request.candidates = {{entry}};
        request.selectedPaths = {entry.path};

        int attempts = 0;
        const auto result =
            foldersnap::CleanupExecutor::execute(request,
                                                 [&attempts](const QString &)
                                                 {
                                                     ++attempts;
                                                     return foldersnap::CleanupMoveResult{true};
                                                 });

        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(attempts, 0);
        QCOMPARE(executionItemAt(result, "../escape")->status,
                 foldersnap::CleanupStatus::OutsideRootOrInvalid);
    }

    void disappearingPathIsReportedWithoutFallback()
    {
        QTemporaryDir dataDirectory;
        QTemporaryDir temporaryDirectory;
        QVERIFY(dataDirectory.isValid());
        QVERIFY(temporaryDirectory.isValid());
        writeFile(temporaryDirectory.filePath("item.txt"), "item");

        const foldersnap::RootPath root = foldersnap::normalizeRootPath(temporaryDirectory.path());
        const foldersnap::ScanResult scanResult = scan(root);
        QVERIFY2(scanResult.error.isEmpty(), qPrintable(scanResult.error));

        foldersnap::CleanupExecutionRequest request;
        request.paths = foldersnap::StoragePaths::fromDataDirectory(dataDirectory.path());
        request.root = root;
        request.rootId = foldersnap::createId();
        request.beforeId = foldersnap::createId();
        request.afterId = foldersnap::createId();
        request.candidates = candidatesFor(scanResult.snapshot, {"item.txt"});
        request.selectedPaths = {"item.txt"};

        const auto result = foldersnap::CleanupExecutor::execute(
            request, [](const QString &)
            { return foldersnap::CleanupMoveResult{false, true, false, "Path disappeared."}; });

        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(executionItemAt(result, "item.txt")->status,
                 foldersnap::CleanupStatus::AlreadyMissing);
        QCOMPARE(result.summary.movedCount, 0);
        QCOMPARE(result.summary.alreadyMissingCount, 1);
        QVERIFY(QFileInfo::exists(temporaryDirectory.filePath("item.txt")));
    }

#ifdef Q_OS_WIN
    void reparseAncestorBlocksCleanup()
    {
        QTemporaryDir watchedDirectory;
        QTemporaryDir junctionTarget;
        QVERIFY(watchedDirectory.isValid());
        QVERIFY(junctionTarget.isValid());
        writeFile(junctionTarget.filePath("outside.txt"), "outside");

        const QString junctionPath = watchedDirectory.filePath("external");
        QProcess process;
        process.start("cmd.exe",
                      {"/D", "/C", "mklink", "/J", QDir::toNativeSeparators(junctionPath),
                       QDir::toNativeSeparators(junctionTarget.path())});
        QVERIFY(process.waitForFinished());
        if (process.exitCode() != 0)
        {
            QSKIP("The test environment does not allow creating directory junctions.");
        }

        const foldersnap::RootPath root = foldersnap::normalizeRootPath(watchedDirectory.path());
        foldersnap::SnapshotEntry entry;
        entry.path = "external/outside.txt";
        entry.displayPath = entry.path;
        entry.type = foldersnap::EntryType::File;
        entry.size = QFileInfo(junctionTarget.filePath("outside.txt")).size();

        const auto result =
            foldersnap::CleanupPreflight::inspect(root, {{entry}}, {"external/outside.txt"});
        QCOMPARE(itemAt(result, "external/outside.txt")->status,
                 foldersnap::CleanupStatus::OutsideRootOrInvalid);
    }

    void cleanupRevalidatesLongPathWithNativeMetadata()
    {
        QTemporaryDir dataDirectory;
        QTemporaryDir temporaryDirectory;
        QVERIFY(dataDirectory.isValid());
        QVERIFY(temporaryDirectory.isValid());

        QStringList components;
        for (int index = 0; index < 18; ++index)
        {
            components.append(QString("segment-%1").arg(index, 6, 10, QLatin1Char('0')));
        }
        const QString nestedPath = temporaryDirectory.filePath(components.join('/'));
        QVERIFY2(QDir().mkpath(nestedPath), qPrintable(nestedPath));
        const QString filePath = QDir(nestedPath).filePath(QString::fromUtf8("résumé-文件.txt"));
        writeFile(filePath, "cleanup-long-path");
        QVERIFY(filePath.size() > 260);

        const foldersnap::RootPath root = foldersnap::normalizeRootPath(temporaryDirectory.path());
        const foldersnap::ScanResult scanResult = scan(root);
        QVERIFY2(scanResult.error.isEmpty(), qPrintable(scanResult.error));
        const auto entry =
            std::find_if(scanResult.snapshot.entries.cbegin(), scanResult.snapshot.entries.cend(),
                         [](const foldersnap::SnapshotEntry &candidate)
                         {
                             return candidate.type == foldersnap::EntryType::File &&
                                    candidate.path.endsWith(QString::fromUtf8("résumé-文件.txt"));
                         });
        QVERIFY(entry != scanResult.snapshot.entries.cend());

        foldersnap::CleanupExecutionRequest request;
        request.paths = foldersnap::StoragePaths::fromDataDirectory(dataDirectory.path());
        request.root = root;
        request.rootId = foldersnap::createId();
        request.beforeId = foldersnap::createId();
        request.afterId = foldersnap::createId();
        request.candidates = {{*entry}};
        request.selectedPaths = {entry->path};

        QString movedPath;
        const auto result = foldersnap::CleanupExecutor::execute(
            request,
            [&movedPath](const QString &path)
            {
                movedPath = path;
                if (!QFile::remove(path))
                {
                    return foldersnap::CleanupMoveResult{
                        false, false, false, "The long-path test file could not be removed."};
                }
                return foldersnap::CleanupMoveResult{true};
            });

        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.summary.movedCount, 1);
        QCOMPARE(executionItemAt(result, entry->path)->status,
                 foldersnap::CleanupStatus::MovedToRecycleBin);
        QVERIFY(movedPath.size() > 260);
        QVERIFY(!QFileInfo::exists(filePath));
    }
#endif

    void executionRevalidatesAndMovesDeepestFirst()
    {
        QTemporaryDir dataDirectory;
        QTemporaryDir temporaryDirectory;
        QVERIFY(dataDirectory.isValid());
        QVERIFY(temporaryDirectory.isValid());
        QVERIFY(QDir().mkpath(temporaryDirectory.filePath("folder")));
        writeFile(temporaryDirectory.filePath("folder/known.txt"), "known");

        const foldersnap::RootPath root = foldersnap::normalizeRootPath(temporaryDirectory.path());
        const foldersnap::ScanResult scanResult = scan(root);
        QVERIFY2(scanResult.error.isEmpty(), qPrintable(scanResult.error));

        foldersnap::CleanupExecutionRequest request;
        request.paths = foldersnap::StoragePaths::fromDataDirectory(dataDirectory.path());
        request.root = root;
        request.rootId = foldersnap::createId();
        request.beforeId = foldersnap::createId();
        request.afterId = foldersnap::createId();
        request.candidates = candidatesFor(scanResult.snapshot, {"folder", "folder/known.txt"});
        request.selectedPaths = {"folder"};

        QStringList moved;
        const auto result = foldersnap::CleanupExecutor::execute(
            request,
            [&moved](const QString &path)
            {
                moved.append(QDir::fromNativeSeparators(path));
                const QFileInfo entry(path);
                if (entry.isDir())
                {
                    QDir parent(entry.absolutePath());
                    if (!parent.rmdir(entry.fileName()))
                    {
                        return foldersnap::CleanupMoveResult{
                            false, false, false, "The test directory could not be removed."};
                    }
                }
                else if (!QFile::remove(path))
                {
                    return foldersnap::CleanupMoveResult{false, false, false,
                                                         "The test file could not be removed."};
                }
                return foldersnap::CleanupMoveResult{true, false, false, {}};
            });

        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.preflight.summary.readyCount, 2);
        QCOMPARE(result.summary.movedCount, 2);
        QCOMPARE(result.summary.blockedCount, 0);
        QCOMPARE(result.summary.failedCount, 0);
        QCOMPARE(moved.size(), 2);
        QCOMPARE(QDir(root.displayPath).relativeFilePath(moved.at(0)), QString("folder/known.txt"));
        QCOMPARE(QDir(root.displayPath).relativeFilePath(moved.at(1)), QString("folder"));
        QCOMPARE(executionItemAt(result, "folder")->status,
                 foldersnap::CleanupStatus::MovedToRecycleBin);

        const auto second = foldersnap::CleanupExecutor::execute(
            request, [](const QString &) { return foldersnap::CleanupMoveResult{true}; });
        QVERIFY2(second.error.isEmpty(), qPrintable(second.error));
        QCOMPARE(second.summary.movedCount, 0);
        QCOMPARE(second.summary.alreadyMissingCount, 2);

        QFile audit(foldersnap::CleanupExecutor::auditPath(request.paths, request.rootId));
        QVERIFY(audit.open(QIODevice::ReadOnly));
        const QByteArray firstLine = audit.readLine();
        const QByteArray secondLine = audit.readLine();
        QVERIFY(!firstLine.isEmpty());
        QVERIFY(!secondLine.isEmpty());
        QVERIFY(audit.atEnd());
        const QJsonDocument document = QJsonDocument::fromJson(firstLine);
        QVERIFY(document.isObject());
        QCOMPARE(document.object().value("rootId").toString(), request.rootId);
        QCOMPARE(document.object().value("beforeId").toString(), request.beforeId);
        QCOMPARE(document.object().value("afterId").toString(), request.afterId);
        QCOMPARE(document.object().value("movedCount").toInt(), 2);
    }

    void executionRevalidationRejectsAFileReplacedByAnEarlierMove()
    {
        QTemporaryDir dataDirectory;
        QTemporaryDir temporaryDirectory;
        QVERIFY(dataDirectory.isValid());
        QVERIFY(temporaryDirectory.isValid());
        const QString firstPath = temporaryDirectory.filePath("a.txt");
        const QString secondPath = temporaryDirectory.filePath("b.txt");
        writeFile(firstPath, "aaaa");
        writeFile(secondPath, "bbbb");

        const foldersnap::RootPath root = foldersnap::normalizeRootPath(temporaryDirectory.path());
        const foldersnap::ScanResult scanResult = scan(root);
        QVERIFY2(scanResult.error.isEmpty(), qPrintable(scanResult.error));

        foldersnap::CleanupExecutionRequest request;
        request.paths = foldersnap::StoragePaths::fromDataDirectory(dataDirectory.path());
        request.root = root;
        request.rootId = foldersnap::createId();
        request.beforeId = foldersnap::createId();
        request.afterId = foldersnap::createId();
        request.candidates = candidatesFor(scanResult.snapshot, {"a.txt", "b.txt"});
        request.selectedPaths = {"a.txt", "b.txt"};

        const auto result = foldersnap::CleanupExecutor::execute(
            request,
            [&](const QString &path)
            {
                if (QFileInfo(path).fileName() == "a.txt")
                {
                    if (!QFile::remove(firstPath))
                    {
                        return foldersnap::CleanupMoveResult{
                            false, false, false, "The first test file could not be removed."};
                    }
                    writeFile(secondPath, "BBBBBBBB");
                }
                return foldersnap::CleanupMoveResult{true};
            });

        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.summary.movedCount, 1);
        QCOMPARE(result.summary.blockedCount, 1);
        QCOMPARE(executionItemAt(result, "a.txt")->status,
                 foldersnap::CleanupStatus::MovedToRecycleBin);
        QCOMPARE(executionItemAt(result, "b.txt")->status,
                 foldersnap::CleanupStatus::ChangedSinceSnapshot);
        QVERIFY(!QFileInfo::exists(firstPath));
        QVERIFY(QFileInfo::exists(secondPath));
    }

    void finalDirectoryRevalidationKeepsNonEmptyParent()
    {
        QTemporaryDir dataDirectory;
        QTemporaryDir temporaryDirectory;
        QVERIFY(dataDirectory.isValid());
        QVERIFY(temporaryDirectory.isValid());
        QVERIFY(QDir().mkpath(temporaryDirectory.filePath("folder")));
        writeFile(temporaryDirectory.filePath("folder/known.txt"), "known");

        const foldersnap::RootPath root = foldersnap::normalizeRootPath(temporaryDirectory.path());
        const foldersnap::ScanResult scanResult = scan(root);
        QVERIFY2(scanResult.error.isEmpty(), qPrintable(scanResult.error));

        foldersnap::CleanupExecutionRequest request;
        request.paths = foldersnap::StoragePaths::fromDataDirectory(dataDirectory.path());
        request.root = root;
        request.rootId = foldersnap::createId();
        request.beforeId = foldersnap::createId();
        request.afterId = foldersnap::createId();
        request.candidates = candidatesFor(scanResult.snapshot, {"folder", "folder/known.txt"});
        request.selectedPaths = {"folder"};

        const auto result = foldersnap::CleanupExecutor::execute(
            request,
            [&](const QString &path)
            {
                if (QFileInfo(path).fileName() == "known.txt")
                {
                    if (!QFile::remove(path))
                    {
                        return foldersnap::CleanupMoveResult{false, false, false,
                                                             "The test file could not be removed."};
                    }
                    writeFile(temporaryDirectory.filePath("folder/untracked.txt"), "untracked");
                }
                return foldersnap::CleanupMoveResult{true};
            });

        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.summary.movedCount, 1);
        QCOMPARE(result.summary.blockedCount, 1);
        QCOMPARE(executionItemAt(result, "folder/known.txt")->status,
                 foldersnap::CleanupStatus::MovedToRecycleBin);
        QCOMPARE(executionItemAt(result, "folder")->status,
                 foldersnap::CleanupStatus::ContainsUntrackedContent);
        QVERIFY(QFileInfo::exists(temporaryDirectory.filePath("folder/untracked.txt")));
        QVERIFY(QDir(temporaryDirectory.filePath("folder")).exists());
    }

    void failedChildKeepsParentOutOfRecycleBin()
    {
        QTemporaryDir dataDirectory;
        QTemporaryDir temporaryDirectory;
        QVERIFY(dataDirectory.isValid());
        QVERIFY(temporaryDirectory.isValid());
        QVERIFY(QDir().mkpath(temporaryDirectory.filePath("folder")));
        writeFile(temporaryDirectory.filePath("folder/known.txt"), "known");

        const foldersnap::RootPath root = foldersnap::normalizeRootPath(temporaryDirectory.path());
        const foldersnap::ScanResult scanResult = scan(root);
        QVERIFY2(scanResult.error.isEmpty(), qPrintable(scanResult.error));

        foldersnap::CleanupExecutionRequest request;
        request.paths = foldersnap::StoragePaths::fromDataDirectory(dataDirectory.path());
        request.root = root;
        request.rootId = foldersnap::createId();
        request.beforeId = foldersnap::createId();
        request.afterId = foldersnap::createId();
        request.candidates = candidatesFor(scanResult.snapshot, {"folder", "folder/known.txt"});
        request.selectedPaths = {"folder"};

        QStringList attempted;
        const auto result = foldersnap::CleanupExecutor::execute(
            request,
            [&attempted](const QString &path)
            {
                attempted.append(QDir::fromNativeSeparators(path));
                return foldersnap::CleanupMoveResult{false, false, true, "Shell aborted."};
            });

        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(attempted.size(), 1);
        QCOMPARE(QDir(root.displayPath).relativeFilePath(attempted.first()),
                 QString("folder/known.txt"));
        QCOMPARE(executionItemAt(result, "folder/known.txt")->status,
                 foldersnap::CleanupStatus::Failed);
        QCOMPARE(executionItemAt(result, "folder")->status, foldersnap::CleanupStatus::Failed);
        QCOMPARE(result.summary.movedCount, 0);
        QVERIFY(result.summary.failedCount >= 2);
    }

    void rotatesFullAuditBeforeRecordingTheNextCleanup()
    {
        QTemporaryDir dataDirectory;
        QTemporaryDir temporaryDirectory;
        QVERIFY(dataDirectory.isValid());
        QVERIFY(temporaryDirectory.isValid());
        writeFile(temporaryDirectory.filePath("item.txt"), "item");

        const foldersnap::RootPath root = foldersnap::normalizeRootPath(temporaryDirectory.path());
        const foldersnap::ScanResult scanResult = scan(root);
        QVERIFY2(scanResult.error.isEmpty(), qPrintable(scanResult.error));

        foldersnap::CleanupExecutionRequest request;
        request.paths = foldersnap::StoragePaths::fromDataDirectory(dataDirectory.path());
        request.root = root;
        request.rootId = foldersnap::createId();
        request.beforeId = foldersnap::createId();
        request.afterId = foldersnap::createId();
        request.candidates = candidatesFor(scanResult.snapshot, {"item.txt"});
        request.selectedPaths = {"item.txt"};

        const QString auditPath =
            foldersnap::CleanupExecutor::auditPath(request.paths, request.rootId);
        QVERIFY(QDir().mkpath(QFileInfo(auditPath).absolutePath()));
        QFile fullAudit(auditPath);
        QVERIFY(fullAudit.open(QIODevice::WriteOnly));
        constexpr qsizetype kAuditLimit = 64 * 1024 * 1024;
        QByteArray filler(kAuditLimit, 'x');
        QCOMPARE(fullAudit.write(filler), filler.size());
        QVERIFY(fullAudit.flush());
        fullAudit.close();

        const auto result = foldersnap::CleanupExecutor::execute(
            request,
            [](const QString &path)
            {
                return QFile::remove(path)
                           ? foldersnap::CleanupMoveResult{true}
                           : foldersnap::CleanupMoveResult{false, false, false, "remove failed"};
            });

        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.summary.movedCount, 1);
        const QFileInfo currentAudit(auditPath);
        QVERIFY(currentAudit.exists());
        QVERIFY(currentAudit.size() < kAuditLimit);
        const QStringList rotated =
            QDir(currentAudit.absolutePath()).entryList({"cleanup-log.*.jsonl"}, QDir::Files);
        QCOMPARE(rotated.size(), 1);
    }

    void auditRejectsUnsafeRootIds()
    {
        QTemporaryDir dataDirectory;
        QVERIFY(dataDirectory.isValid());
        const auto paths = foldersnap::StoragePaths::fromDataDirectory(dataDirectory.path());
        QVERIFY_EXCEPTION_THROWN(foldersnap::CleanupExecutor::auditPath(paths, "../escape"),
                                 foldersnap::DomainError);
    }
};

QTEST_GUILESS_MAIN(CleanupTest)
#include "tst_cleanup.moc"
