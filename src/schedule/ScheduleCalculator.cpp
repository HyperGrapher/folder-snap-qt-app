#include "schedule/ScheduleCalculator.h"

#include <algorithm>
#include <limits>

#include <QDate>
#include <QDateTime>
#include <QTime>

#include "domain/DomainError.h"

namespace foldersnap
{
namespace
{
constexpr qint64 kNanosecondsPerMillisecond = 1000000;
constexpr qint64 kNanosecondsPerHour = 60LL * 60LL * 1000LL * kNanosecondsPerMillisecond;

QDateTime dateTimeFromUtc(UtcTimestamp timestamp)
{
    return QDateTime::fromMSecsSinceEpoch(timestamp.nanoseconds / kNanosecondsPerMillisecond,
                                          QTimeZone::UTC);
}

UtcTimestamp utcFromDateTime(const QDateTime &dateTime)
{
    const qint64 milliseconds = dateTime.toMSecsSinceEpoch();
    if (milliseconds > std::numeric_limits<qint64>::max() / kNanosecondsPerMillisecond ||
        milliseconds < std::numeric_limits<qint64>::min() / kNanosecondsPerMillisecond)
    {
        throw DomainError(ErrorCode::SizeLimit, "Scheduled time is outside the supported range.");
    }
    return {milliseconds * kNanosecondsPerMillisecond};
}

QDateTime localOccurrence(const QDate &date, const Schedule &schedule, const QTimeZone &timeZone)
{
    return QDateTime(date, QTime(schedule.hour, schedule.minute), timeZone,
                     QDateTime::TransitionResolution::RelativeToBefore);
}

QDateTime nextDaily(const Schedule &schedule, const QDateTime &nowUtc, const QTimeZone &timeZone)
{
    const QDate localDate = nowUtc.toTimeZone(timeZone).date();
    QDateTime candidate = localOccurrence(localDate, schedule, timeZone);
    if (candidate <= nowUtc)
    {
        candidate = localOccurrence(localDate.addDays(1), schedule, timeZone);
    }
    return candidate;
}

QDateTime nextWeekly(const Schedule &schedule, const QDateTime &nowUtc, const QTimeZone &timeZone)
{
    const QDate localDate = nowUtc.toTimeZone(timeZone).date();
    const int currentWeekday = localDate.dayOfWeek() % 7;
    int daysAhead = (schedule.weekday - currentWeekday + 7) % 7;
    QDateTime candidate = localOccurrence(localDate.addDays(daysAhead), schedule, timeZone);
    if (candidate <= nowUtc)
    {
        daysAhead += 7;
        candidate = localOccurrence(localDate.addDays(daysAhead), schedule, timeZone);
    }
    return candidate;
}

QDate clampedMonthlyDate(int year, int month, int requestedDay)
{
    const QDate firstOfMonth(year, month, 1);
    return QDate(year, month, std::min(requestedDay, firstOfMonth.daysInMonth()));
}

QDateTime nextMonthly(const Schedule &schedule, const QDateTime &nowUtc, const QTimeZone &timeZone)
{
    const QDate localDate = nowUtc.toTimeZone(timeZone).date();
    QDate candidateDate =
        clampedMonthlyDate(localDate.year(), localDate.month(), schedule.dayOfMonth);
    QDateTime candidate = localOccurrence(candidateDate, schedule, timeZone);
    if (candidate <= nowUtc)
    {
        const QDate nextMonth = QDate(localDate.year(), localDate.month(), 1).addMonths(1);
        candidateDate =
            clampedMonthlyDate(nextMonth.year(), nextMonth.month(), schedule.dayOfMonth);
        candidate = localOccurrence(candidateDate, schedule, timeZone);
    }
    return candidate;
}

UtcTimestamp nextCalendarOccurrence(const Schedule &schedule, UtcTimestamp nowUtc,
                                    const QTimeZone &timeZone)
{
    const QDateTime now = dateTimeFromUtc(nowUtc);
    switch (schedule.kind)
    {
    case ScheduleKind::Daily:
        return utcFromDateTime(nextDaily(schedule, now, timeZone));
    case ScheduleKind::Weekly:
        return utcFromDateTime(nextWeekly(schedule, now, timeZone));
    case ScheduleKind::Monthly:
        return utcFromDateTime(nextMonthly(schedule, now, timeZone));
    case ScheduleKind::Manual:
    case ScheduleKind::Interval:
        break;
    }
    throw DomainError(ErrorCode::InvalidData, "Expected a calendar schedule.");
}
} // namespace

ScheduleDecision ScheduleCalculator::evaluate(const Schedule &schedule, UtcTimestamp nowUtc,
                                              const QTimeZone &timeZone)
{
    validateSchedule(schedule);
    if (!timeZone.isValid())
    {
        throw DomainError(ErrorCode::InvalidData, "A valid time zone is required for scheduling.");
    }
    if (schedule.kind == ScheduleKind::Manual)
    {
        return {};
    }
    if (!schedule.nextDueAtUtc)
    {
        if (schedule.kind == ScheduleKind::Interval)
        {
            const qint64 interval = schedule.intervalHours * kNanosecondsPerHour;
            if (nowUtc.nanoseconds > std::numeric_limits<qint64>::max() - interval)
            {
                throw DomainError(ErrorCode::SizeLimit,
                                  "Scheduled time is outside the supported range.");
            }
            return {false, UtcTimestamp{nowUtc.nanoseconds + interval}};
        }
        return {false, nextCalendarOccurrence(schedule, nowUtc, timeZone)};
    }
    if (*schedule.nextDueAtUtc > nowUtc)
    {
        return {false, schedule.nextDueAtUtc};
    }
    if (schedule.kind == ScheduleKind::Interval)
    {
        const qint64 interval = schedule.intervalHours * kNanosecondsPerHour;
        const quint64 elapsed = static_cast<quint64>(nowUtc.nanoseconds) -
                                static_cast<quint64>(schedule.nextDueAtUtc->nanoseconds);
        const quint64 periods = elapsed / static_cast<quint64>(interval) + 1;
        if (periods > static_cast<quint64>(std::numeric_limits<qint64>::max()) /
                          static_cast<quint64>(interval))
        {
            throw DomainError(ErrorCode::SizeLimit,
                              "Scheduled time is outside the supported range.");
        }
        const qint64 advance = static_cast<qint64>(periods) * interval;
        if (schedule.nextDueAtUtc->nanoseconds > std::numeric_limits<qint64>::max() - advance)
        {
            throw DomainError(ErrorCode::SizeLimit,
                              "Scheduled time is outside the supported range.");
        }
        return {true, UtcTimestamp{schedule.nextDueAtUtc->nanoseconds + advance}};
    }
    return {true, nextCalendarOccurrence(schedule, nowUtc, timeZone)};
}
} // namespace foldersnap
