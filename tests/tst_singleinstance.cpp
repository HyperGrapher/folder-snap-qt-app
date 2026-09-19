#include <atomic>
#include <thread>

#include <QSignalSpy>
#include <QUuid>
#include <QtTest>

#include "platform/windows/SingleInstance.h"

class SingleInstanceTest final : public QObject
{
    Q_OBJECT

  private slots:
    void forwardsActivationToPrimary()
    {
        const QString instanceName =
            QStringLiteral("FolderSnap.Test.") + QUuid::createUuid().toString(QUuid::Id128);
        SingleInstance primary(instanceName);
        QCOMPARE(primary.acquire(), SingleInstance::AcquireResult::Primary);
        QTest::qWait(50);
        QSignalSpy activationSpy(&primary, &SingleInstance::activationRequested);

        std::atomic_int secondaryResult{-1};
        std::jthread worker(
            [&]
            {
                SingleInstance secondary(instanceName);
                secondaryResult.store(static_cast<int>(secondary.acquire()),
                                      std::memory_order_release);
            });
        QTRY_VERIFY_WITH_TIMEOUT(secondaryResult.load(std::memory_order_acquire) != -1, 2000);
        QCOMPARE(static_cast<SingleInstance::AcquireResult>(
                     secondaryResult.load(std::memory_order_acquire)),
                 SingleInstance::AcquireResult::Forwarded);
        QTRY_COMPARE_WITH_TIMEOUT(activationSpy.count(), 1, 2000);
        QVERIFY(primary.takePendingActivation());
        QVERIFY(!primary.takePendingActivation());
    }
};

QTEST_GUILESS_MAIN(SingleInstanceTest)
#include "tst_singleinstance.moc"
