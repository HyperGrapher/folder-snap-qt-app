#include <QtTest>

#include "domain/DomainError.h"
#include "paths/WindowsPaths.h"

class PathsTest final : public QObject
{
    Q_OBJECT
  private slots:
    void roots()
    {
        const auto root = foldersnap::normalizeRootPath("c:\\Projects\\Demo\\..\\資料\\");
        QCOMPARE(root.displayPath, QString("C:/Projects/資料"));
        QCOMPARE(root.identityPath, QString("c:/projects/資料"));
        QCOMPARE(foldersnap::normalizeRootPath("C:/").displayPath, QString("C:/"));
        QCOMPARE(foldersnap::normalizeRootPath("\\\\?\\C:\\Projects").displayPath,
                 QString("C:/Projects"));
        QCOMPARE(foldersnap::normalizeRootPath("\\\\?\\UNC\\Server\\Share\\Folder").identityPath,
                 QString("//server/share/folder"));
        QCOMPARE(foldersnap::normalizeRootPath("//Server/Share/").displayPath,
                 QString("//Server/Share"));
        const QString longPath = "C:/" + QString("long-folder/").repeated(30) + "leaf";
        QCOMPARE(foldersnap::normalizeRootPath(longPath).displayPath, longPath);
    }

    void invalidRoots_data()
    {
        QTest::addColumn<QString>("path");
        const QStringList paths{"",
                                "C:relative",
                                "/root",
                                "relative",
                                "//server",
                                "//server/share/../escape",
                                "C:/../escape",
                                "C:/folder./file",
                                "C:/NUL.txt",
                                "//./pipe/test",
                                "C:/file:stream",
                                "C:/folder /file",
                                "///server/share"};
        for (const QString &path : paths)
        {
            QTest::newRow(qPrintable(path.isEmpty() ? "empty" : path)) << path;
        }
    }

    void invalidRoots()
    {
        QFETCH(QString, path);
        QVERIFY_EXCEPTION_THROWN((void)foldersnap::normalizeRootPath(path),
                                 foldersnap::DomainError);
    }

    void invalidRelativePaths_data()
    {
        QTest::addColumn<QString>("path");
        const QStringList paths{"",          ".",           "..",
                                "../escape", "a/../b",      "a/./b",
                                "/absolute", "C:/absolute", "a//b",
                                "a/",        "a\\..\\b",    "file:stream",
                                "con.txt",   "COM1",        "LPT².log",
                                "folder ",   "folder.",     "a?b",
                                "a*b",       "a\nb",        QString("a") + QChar::Null};
        for (const QString &path : paths)
        {
            QTest::newRow(qPrintable(QString::number(path.size()) + ':' + path)) << path;
        }
    }

    void invalidRelativePaths()
    {
        QFETCH(QString, path);
        QVERIFY_EXCEPTION_THROWN((void)foldersnap::normalizeRelativePath(path),
                                 foldersnap::DomainError);
    }

    void identitiesAndContainment()
    {
        QVERIFY_EXCEPTION_THROWN((void)foldersnap::normalizeRelativePath("CON .txt"),
                                 foldersnap::DomainError);
        QCOMPARE(foldersnap::normalizeRelativePath("資料\\Résumé.TXT"), QString("資料/résumé.txt"));
        foldersnap::validateIdentityPath("資料/résumé.txt");
        QVERIFY_EXCEPTION_THROWN(foldersnap::validateIdentityPath("File.txt"),
                                 foldersnap::DomainError);
        QVERIFY(foldersnap::isAtOrBelow("C:/FOO/bar", "c:/foo"));
        QVERIFY(foldersnap::isAtOrBelow("C:/foo", "c:/foo"));
        QVERIFY(!foldersnap::isAtOrBelow("C:/foobar", "C:/foo"));
        const auto root = foldersnap::normalizeRootPath("C:/Foo");
        QCOMPARE(foldersnap::joinUnderRoot(root, "dir/file"), QString("C:/Foo/dir/file"));
        QVERIFY_EXCEPTION_THROWN((void)foldersnap::joinUnderRoot(root, "../escape"),
                                 foldersnap::DomainError);
        auto forged = root;
        forged.identityPath = "c:/elsewhere";
        QVERIFY_EXCEPTION_THROWN((void)foldersnap::joinUnderRoot(forged, "file"),
                                 foldersnap::DomainError);
        foldersnap::validateStorageId("snapshot-123");
        for (const QString &id : QStringList{"", "..", "../file", "con", "a:b", "a/b", "a\\b"})
        {
            try
            {
                foldersnap::validateStorageId(id);
                QFAIL("Unsafe ID accepted");
            }
            catch (const foldersnap::DomainError &error)
            {
                QCOMPARE(error.code(), foldersnap::ErrorCode::InvalidIdentifier);
            }
        }
    }

    void dataProtection()
    {
        const auto root = foldersnap::normalizeRootPath("C:/Users/Test");
        const auto dataDirectory =
            foldersnap::normalizeRootPath("C:/Users/Test/AppData/FolderSnap");
        QCOMPARE(*foldersnap::protectedDataSubtree(root, dataDirectory),
                 QString("appdata/foldersnap"));
        QVERIFY(!foldersnap::protectedDataSubtree(foldersnap::normalizeRootPath("D:/Work"),
                                                  dataDirectory));
        QVERIFY_EXCEPTION_THROWN(
            (void)foldersnap::protectedDataSubtree(dataDirectory, dataDirectory),
            foldersnap::DomainError);
        QVERIFY_EXCEPTION_THROWN(
            (void)foldersnap::protectedDataSubtree(
                foldersnap::normalizeRootPath(dataDirectory.displayPath + "/snapshots"),
                dataDirectory),
            foldersnap::DomainError);
    }
};

QTEST_GUILESS_MAIN(PathsTest)
#include "tst_paths.moc"
