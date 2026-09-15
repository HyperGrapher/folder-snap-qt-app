#include <QtTest>

#include "domain/DomainError.h"
#include "ignore/IgnoreMatcher.h"

class IgnoreTest final : public QObject
{
    Q_OBJECT
  private slots:
    void matching_data()
    {
        QTest::addColumn<QString>("rule");
        QTest::addColumn<QString>("path");
        QTest::addColumn<bool>("directory");
        QTest::addColumn<bool>("included");
        QTest::newRow("basename") << "*.tmp" << "a/FILE.TMP" << false << false;
        QTest::newRow("question") << "file?.txt" << "file1.txt" << false << false;
        QTest::newRow("question-length") << "file?.txt" << "file12.txt" << false << true;
        QTest::newRow("anchored") << "/build/" << "build/file" << false << false;
        QTest::newRow("anchored-nested") << "/build/" << "a/build/file" << false << true;
        QTest::newRow("directory") << "build/" << "a/build" << true << false;
        QTest::newRow("same-name-file") << "build/" << "a/build" << false << true;
        QTest::newRow("descendant") << "build/" << "a/build/deep/file" << false << false;
        QTest::newRow("slash-rooted") << "docs/*.tmp" << "other/docs/file.tmp" << false << true;
        QTest::newRow("star-no-slash") << "docs/*.tmp" << "docs/sub/file.tmp" << false << true;
        QTest::newRow("globstar-zero") << "docs/**/*.tmp" << "docs/file.tmp" << false << false;
        QTest::newRow("globstar-deep") << "docs/**/*.tmp" << "docs/a/b/file.tmp" << false << false;
        QTest::newRow("globstar-all") << "cache/**" << "cache/a/b/file" << false << false;
        QTest::newRow("backslash") << "cache\\" << "cache/file" << false << false;
        QTest::newRow("literal-brackets") << "[cache]/" << "[cache]/file" << false << false;
        QTest::newRow("unicode") << "資料/" << "資料/file" << false << false;
    }

    void matching()
    {
        QFETCH(QString, rule);
        QFETCH(QString, path);
        QFETCH(bool, directory);
        QFETCH(bool, included);
        QCOMPARE(foldersnap::IgnoreMatcher({rule}).testPath(path, directory).included, included);
    }

    void orderingAndDiagnostics()
    {
        const foldersnap::IgnoreMatcher matcher(
            {"# comment", "", " build/ ", "!build/keep.txt", "*.tmp"});
        const auto excluded = matcher.testPath("build/file", false);
        QVERIFY(!excluded.included);
        QCOMPARE(excluded.ruleLine, 3);
        QCOMPARE(excluded.originalRule, QString(" build/ "));
        QVERIFY(matcher.testPath("build/keep.txt", false).included);
        QVERIFY(!matcher.testPath("build/keep.tmp", false).included);
        QVERIFY(!matcher.canPruneDirectory("build"));
        QVERIFY(foldersnap::IgnoreMatcher({"build/"}).canPruneDirectory("build"));
        QVERIFY(!foldersnap::IgnoreMatcher({"build/"}).canPruneDirectory("src"));
        for (const QString &rule : QStringList{"!", "/", "../secret", "a/./b", "a//b"})
        {
            try
            {
                const foldersnap::IgnoreMatcher invalid({"# first", rule});
                QFAIL("Invalid rule accepted");
            }
            catch (const foldersnap::DomainError &error)
            {
                QCOMPARE(error.code(), foldersnap::ErrorCode::InvalidRule);
                QCOMPARE(error.ruleLine(), 2);
            }
        }
    }

    void protectionAndHash()
    {
        const foldersnap::IgnoreMatcher matcher({"!**"}, QString("app/[foldersnap]"));
        const auto protectedMatch = matcher.testPath("app/[foldersnap]/snapshots/a.json", false);
        QVERIFY(!protectedMatch.included);
        QVERIFY(protectedMatch.protectedByApplication);
        QVERIFY(matcher.canPruneDirectory("app/[foldersnap]"));
        QVERIFY(matcher.testPath("app/[foldersnap]-other/file", false).included);
        QCOMPARE(foldersnap::IgnoreMatcher::rulesHash({}),
                 QString("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
        QVERIFY(foldersnap::IgnoreMatcher::rulesHash({"build/"}) !=
                foldersnap::IgnoreMatcher::rulesHash({" build/ "}));
        QVERIFY(foldersnap::IgnoreMatcher::rulesHash({"a", "b"}) !=
                foldersnap::IgnoreMatcher::rulesHash({"b", "a"}));
    }
};

QTEST_GUILESS_MAIN(IgnoreTest)
#include "tst_ignore.moc"
