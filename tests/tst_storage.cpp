#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "domain/DomainError.h"
#include "storage/AtomicFile.h"
#include "storage/ConfigurationStore.h"

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
};

QTEST_MAIN(StorageTest)
#include "tst_storage.moc"
