#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "application/ExportCoordinator.h"
#include "domain/JsonCodec.h"
#include "storage/SnapshotStore.h"

namespace
{
foldersnap::Snapshot fixtureSnapshot()
{
    QFile fixture(QString(FOLDERSNAP_FIXTURE_DIR) + "/snapshot-v2.json");
    if (!fixture.open(QIODevice::ReadOnly))
    {
        qFatal("Could not open the snapshot fixture.");
    }
    return foldersnap::decodeSnapshot(fixture.readAll());
}
} // namespace

class ExportCoordinatorTest final : public QObject
{
    Q_OBJECT

  private slots:
    void runsExportOutsideCallingThread()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const foldersnap::StoragePaths paths =
            foldersnap::StoragePaths::fromDataDirectory(temporaryDirectory.filePath("data"));
        const foldersnap::Snapshot snapshot = fixtureSnapshot();
        const foldersnap::SnapshotStore store(paths);
        QVERIFY(store.saveSnapshot(snapshot) > 0);

        foldersnap::ExportCoordinator coordinator;
        QSignalSpy activeSpy(&coordinator, &foldersnap::ExportCoordinator::activeChanged);
        QSignalSpy successSpy(&coordinator, &foldersnap::ExportCoordinator::succeeded);
        QSignalSpy failureSpy(&coordinator, &foldersnap::ExportCoordinator::failed);
        const QString destination = temporaryDirectory.filePath("report.csv");

        coordinator.start({paths,
                           snapshot.header.snapshotId,
                           {},
                           destination,
                           foldersnap::ExportFormat::Csv,
                           {}});

        QVERIFY(coordinator.isActive());
        QCOMPARE(activeSpy.count(), 1);
        QVERIFY(successSpy.wait());
        QCOMPARE(failureSpy.count(), 0);
        QCOMPARE(activeSpy.count(), 2);
        QVERIFY(!coordinator.isActive());
        QVERIFY(QFile::exists(destination));
    }

    void cancelStopsQueuedWorkWithoutOutput()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const foldersnap::StoragePaths paths =
            foldersnap::StoragePaths::fromDataDirectory(temporaryDirectory.filePath("data"));
        const foldersnap::Snapshot snapshot = fixtureSnapshot();
        const foldersnap::SnapshotStore store(paths);
        QVERIFY(store.saveSnapshot(snapshot) > 0);

        foldersnap::ExportCoordinator coordinator;
        QSignalSpy cancelledSpy(&coordinator, &foldersnap::ExportCoordinator::cancelled);
        const QString destination = temporaryDirectory.filePath("cancelled.csv");
        coordinator.start({paths,
                           snapshot.header.snapshotId,
                           {},
                           destination,
                           foldersnap::ExportFormat::Csv,
                           {}});
        coordinator.cancel();

        QVERIFY(cancelledSpy.wait());
        QVERIFY(!QFile::exists(destination));
    }
};

QTEST_MAIN(ExportCoordinatorTest)
#include "tst_exportcoordinator.moc"
