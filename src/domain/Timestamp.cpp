#include "Timestamp.h"

#include <limits>

#include <QDateTime>
#include <QRegularExpression>
#include <QTimeZone>

#include "domain/DomainError.h"

namespace foldersnap
{
namespace
{
constexpr qint64 kNanosecondsPerSecond = 1000000000;
[[noreturn]] void invalidTimestamp()
{
    throw DomainError(ErrorCode::InvalidData, "Invalid or out-of-range timestamp.");
}
} // namespace

UtcTimestamp parseTimestamp(const QString &text)
{
    const QRegularExpression pattern("^(\\d{4})-(\\d{2})-(\\d{2})T(\\d{2}):(\\d{2}):(\\d{2})"
                                     "(?:\\.(\\d{1,9}))?(Z|[+-]\\d{2}:\\d{2})\\z");
    const auto match = pattern.match(text);
    if (!match.hasMatch())
    {
        invalidTimestamp();
    }
    const QDate date(match.captured(1).toInt(), match.captured(2).toInt(),
                     match.captured(3).toInt());
    const QTime time(match.captured(4).toInt(), match.captured(5).toInt(),
                     match.captured(6).toInt());
    if (!date.isValid() || !time.isValid())
    {
        invalidTimestamp();
    }
    qint64 seconds = QDateTime(date, time, QTimeZone::UTC).toSecsSinceEpoch();
    const QString zone = match.captured(8);
    if (zone != "Z")
    {
        const int hours = zone.mid(1, 2).toInt();
        const int minutes = zone.mid(4, 2).toInt();
        if (hours > 23 || minutes > 59)
        {
            invalidTimestamp();
        }
        seconds -= (zone[0] == '+' ? 1 : -1) * (hours * 3600 + minutes * 60);
    }
    const qint64 fraction = match.captured(7).leftJustified(9, '0').toLongLong();
    constexpr qint64 maximum = std::numeric_limits<qint64>::max();
    constexpr qint64 minimum = std::numeric_limits<qint64>::min();
    if (seconds > maximum / kNanosecondsPerSecond ||
        (seconds == maximum / kNanosecondsPerSecond &&
         fraction > maximum % kNanosecondsPerSecond) ||
        seconds < minimum / kNanosecondsPerSecond - 1 ||
        (seconds == minimum / kNanosecondsPerSecond - 1 &&
         fraction < kNanosecondsPerSecond + minimum % kNanosecondsPerSecond))
    {
        invalidTimestamp();
    }
    if (seconds < 0)
    {
        // Multiplying the lowest representable second directly would underflow.
        return {(seconds + 1) * kNanosecondsPerSecond - (kNanosecondsPerSecond - fraction)};
    }
    return {seconds * kNanosecondsPerSecond + fraction};
}

QString formatTimestamp(UtcTimestamp timestamp)
{
    qint64 seconds = timestamp.nanoseconds / kNanosecondsPerSecond;
    qint64 fraction = timestamp.nanoseconds % kNanosecondsPerSecond;
    if (fraction < 0)
    {
        --seconds;
        fraction += kNanosecondsPerSecond;
    }
    return QDateTime::fromSecsSinceEpoch(seconds, QTimeZone::UTC)
               .toString("yyyy-MM-dd'T'HH:mm:ss") +
           '.' + QString::number(fraction).rightJustified(9, '0') + 'Z';
}

qint64 unixNanosecondsFromFileTime(quint64 ticks)
{
    constexpr quint64 kEpochOffset = 116444736000000000ULL;
    constexpr quint64 kLargestTicks =
        static_cast<quint64>(std::numeric_limits<qint64>::max()) / 100;
    const quint64 difference = ticks >= kEpochOffset ? ticks - kEpochOffset : kEpochOffset - ticks;
    if (difference > kLargestTicks)
    {
        invalidTimestamp();
    }
    const qint64 nanoseconds = static_cast<qint64>(difference) * 100;
    return ticks >= kEpochOffset ? nanoseconds : -nanoseconds;
}
} // namespace foldersnap
