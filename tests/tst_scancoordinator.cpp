#include <algorithm>

#include <QTemporaryDir>
#include <QtTest>

#include "application/ScanCoordinator.h"
#include "domain/Snapshot.h"
#include "paths/WindowsPaths.h"

namespace
{
foldersnap::ScanJobRequest requestFor(const QString &rootId, const QString &path,
                                      const foldersnap::StoragePaths &storage,
                                      foldersnap::SnapshotTrigger trigger)
{
    foldersnap::ScanJobRequest request;
    request.scan.rootId = rootId;
    request.scan.displayTitle = rootId;
    request.scan.root = foldersnap::normalizeRootPath(path);
    request.scan.trigger = trigger;
    request.paths = storage;
    return request;
}
} // namespace

class ScanCoordinatorTest final : public QObject
{
    Q_OBJECT

  private slots:
    void limitsAndCoalescesRequests()
    {
        QTemporaryDir dataDirectory;
        QTemporaryDir firstDirectory;
        QTemporaryDir secondDirectory;
        QTemporaryDir thirdDirectory;
        QVERIFY(dataDirectory.isValid());
        QVERIFY(firstDirectory.isValid());
        QVERIFY(secondDirectory.isValid());
        QVERIFY(thirdDirectory.isValid());

        const foldersnap::StoragePaths storage =
            foldersnap::StoragePaths::fromDataDirectory(dataDirectory.path());
        const QString firstId = foldersnap::createId();
        const QString secondId = foldersnap::createId();
        const QString thirdId = foldersnap::createId();
        foldersnap::ScanCoordinator coordinator;
        int maximumActive = 0;
        QList<foldersnap::SnapshotTrigger> firstTriggers;
        int completed = 0;
        connect(&coordinator, &foldersnap::ScanCoordinator::activeChanged, this,
                [&coordinator, &maximumActive](const QString &, bool)
                { maximumActive = std::max(maximumActive, coordinator.activeCount()); });
        connect(&coordinator, &foldersnap::ScanCoordinator::succeeded, this,
                [&completed, &firstTriggers, &firstId](const foldersnap::ScanJobResult &result)
                {
                    ++completed;
                    if (result.rootId == firstId)
                    {
                        firstTriggers.append(result.trigger);
                    }
                });

        coordinator.request(requestFor(firstId, firstDirectory.path(), storage,
                                       foldersnap::SnapshotTrigger::Manual));
        coordinator.request(requestFor(secondId, secondDirectory.path(), storage,
                                       foldersnap::SnapshotTrigger::Manual));
        coordinator.request(requestFor(thirdId, thirdDirectory.path(), storage,
                                       foldersnap::SnapshotTrigger::Manual));
        QCOMPARE(coordinator.activeCount(), 2);
        QVERIFY(coordinator.isActive(firstId));
        QVERIFY(coordinator.isActive(secondId));
        QVERIFY(!coordinator.isActive(thirdId));

        coordinator.request(requestFor(firstId, firstDirectory.path(), storage,
                                       foldersnap::SnapshotTrigger::Scheduled));
        coordinator.request(requestFor(firstId, firstDirectory.path(), storage,
                                       foldersnap::SnapshotTrigger::Scheduled));
        coordinator.request(requestFor(firstId, firstDirectory.path(), storage,
                                       foldersnap::SnapshotTrigger::Manual));
        coordinator.request(requestFor(firstId, firstDirectory.path(), storage,
                                       foldersnap::SnapshotTrigger::Manual));

        QTRY_COMPARE_WITH_TIMEOUT(completed, 5, 10000);
        QCOMPARE(maximumActive, 2);
        QCOMPARE(firstTriggers,
                 QList<foldersnap::SnapshotTrigger>({foldersnap::SnapshotTrigger::Manual,
                                                     foldersnap::SnapshotTrigger::Manual,
                                                     foldersnap::SnapshotTrigger::Scheduled}));
    }

    void cancellingQueuedRootPreventsItsScan()
    {
        QTemporaryDir dataDirectory;
        QTemporaryDir firstDirectory;
        QTemporaryDir secondDirectory;
        QTemporaryDir queuedDirectory;
        QVERIFY(dataDirectory.isValid());
        QVERIFY(firstDirectory.isValid());
        QVERIFY(secondDirectory.isValid());
        QVERIFY(queuedDirectory.isValid());

        const foldersnap::StoragePaths storage =
            foldersnap::StoragePaths::fromDataDirectory(dataDirectory.path());
        const QString firstId = foldersnap::createId();
        const QString secondId = foldersnap::createId();
        const QString queuedId = foldersnap::createId();
        foldersnap::ScanCoordinator coordinator;
        QStringList completedRootIds;
        connect(&coordinator, &foldersnap::ScanCoordinator::succeeded, this,
                [&completedRootIds](const foldersnap::ScanJobResult &result)
                { completedRootIds.append(result.rootId); });

        coordinator.request(requestFor(firstId, firstDirectory.path(), storage,
                                       foldersnap::SnapshotTrigger::Manual));
        coordinator.request(requestFor(secondId, secondDirectory.path(), storage,
                                       foldersnap::SnapshotTrigger::Manual));
        coordinator.request(requestFor(queuedId, queuedDirectory.path(), storage,
                                       foldersnap::SnapshotTrigger::Manual));
        QCOMPARE(coordinator.activeCount(), 2);

        coordinator.cancelRoot(queuedId);

        QTRY_COMPARE_WITH_TIMEOUT(completedRootIds.size(), 2, 5000);
        QVERIFY(completedRootIds.contains(firstId));
        QVERIFY(completedRootIds.contains(secondId));
        QVERIFY(!completedRootIds.contains(queuedId));
        QCOMPARE(coordinator.activeCount(), 0);
    }
};

QTEST_GUILESS_MAIN(ScanCoordinatorTest)
#include "tst_scancoordinator.moc"
