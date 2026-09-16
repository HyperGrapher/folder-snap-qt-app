#include <QDateTime>
#include <QTest>

#include "schedule/ScheduleCalculator.h"

namespace
{
foldersnap::UtcTimestamp utc(const QString &text)
{
    return {QDateTime::fromString(text, Qt::ISODate).toMSecsSinceEpoch() * 1000000};
}

QString iso(foldersnap::UtcTimestamp timestamp)
{
    return QDateTime::fromMSecsSinceEpoch(timestamp.nanoseconds / 1000000, QTimeZone::UTC)
        .toString(Qt::ISODate);
}
} // namespace

class ScheduleTest final : public QObject
{
    Q_OBJECT

  private slots:
    void manualScheduleHasNoDueTime()
    {
        const foldersnap::ScheduleDecision decision = foldersnap::ScheduleCalculator::evaluate(
            {}, utc("2026-01-01T10:00:00Z"), QTimeZone::UTC);
        QVERIFY(!decision.shouldRun);
        QVERIFY(!decision.nextDueAtUtc);
    }

    void initializesAndAdvancesIntervalWithoutBursting()
    {
        foldersnap::Schedule schedule;
        schedule.kind = foldersnap::ScheduleKind::Interval;
        schedule.intervalHours = 3;
        foldersnap::ScheduleDecision decision = foldersnap::ScheduleCalculator::evaluate(
            schedule, utc("2026-01-01T10:00:00Z"), QTimeZone::UTC);
        QVERIFY(!decision.shouldRun);
        QCOMPARE(iso(*decision.nextDueAtUtc), QString("2026-01-01T13:00:00Z"));

        schedule.nextDueAtUtc = utc("2026-01-01T01:00:00Z");
        decision = foldersnap::ScheduleCalculator::evaluate(schedule, utc("2026-01-01T10:00:00Z"),
                                                            QTimeZone::UTC);
        QVERIFY(decision.shouldRun);
        QCOMPARE(iso(*decision.nextDueAtUtc), QString("2026-01-01T13:00:00Z"));
    }

    void calculatesDailyAndWeeklyOccurrences()
    {
        foldersnap::Schedule daily;
        daily.kind = foldersnap::ScheduleKind::Daily;
        daily.hour = 9;
        daily.minute = 30;
        auto decision = foldersnap::ScheduleCalculator::evaluate(daily, utc("2026-01-01T10:00:00Z"),
                                                                 QTimeZone::UTC);
        QCOMPARE(iso(*decision.nextDueAtUtc), QString("2026-01-02T09:30:00Z"));

        foldersnap::Schedule weekly;
        weekly.kind = foldersnap::ScheduleKind::Weekly;
        weekly.weekday = 0;
        weekly.hour = 8;
        decision = foldersnap::ScheduleCalculator::evaluate(weekly, utc("2026-01-01T10:00:00Z"),
                                                            QTimeZone::UTC);
        QCOMPARE(iso(*decision.nextDueAtUtc), QString("2026-01-04T08:00:00Z"));
    }

    void clampsMonthlyDatesAndPreservesCalendarAnchor()
    {
        foldersnap::Schedule schedule;
        schedule.kind = foldersnap::ScheduleKind::Monthly;
        schedule.dayOfMonth = 31;
        schedule.hour = 9;
        auto decision = foldersnap::ScheduleCalculator::evaluate(
            schedule, utc("2024-02-01T00:00:00Z"), QTimeZone::UTC);
        QCOMPARE(iso(*decision.nextDueAtUtc), QString("2024-02-29T09:00:00Z"));

        schedule.nextDueAtUtc = utc("2024-01-31T09:00:00Z");
        decision = foldersnap::ScheduleCalculator::evaluate(schedule, utc("2024-03-01T00:00:00Z"),
                                                            QTimeZone::UTC);
        QVERIFY(decision.shouldRun);
        QCOMPARE(iso(*decision.nextDueAtUtc), QString("2024-03-31T09:00:00Z"));
    }

    void resolvesSkippedDstLocalTime()
    {
        const QTimeZone berlin("Europe/Berlin");
        QVERIFY(berlin.isValid());
        foldersnap::Schedule schedule;
        schedule.kind = foldersnap::ScheduleKind::Daily;
        schedule.hour = 2;
        schedule.minute = 30;
        const foldersnap::ScheduleDecision decision =
            foldersnap::ScheduleCalculator::evaluate(schedule, utc("2026-03-28T12:00:00Z"), berlin);
        const QDateTime local = QDateTime::fromMSecsSinceEpoch(
                                    decision.nextDueAtUtc->nanoseconds / 1000000, QTimeZone::UTC)
                                    .toTimeZone(berlin);

        QCOMPARE(local.date(), QDate(2026, 3, 29));
        QCOMPARE(local.time(), QTime(3, 30));
    }
};

QTEST_MAIN(ScheduleTest)
#include "tst_schedule.moc"
