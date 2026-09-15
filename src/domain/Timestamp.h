#pragma once

#include <compare>

#include <QString>
#include <QtTypes>

namespace foldersnap
{
struct UtcTimestamp
{
    qint64 nanoseconds{0};
    auto operator<=>(const UtcTimestamp &) const = default;
};

[[nodiscard]] UtcTimestamp parseTimestamp(const QString &text);
[[nodiscard]] QString formatTimestamp(UtcTimestamp timestamp);
[[nodiscard]] qint64 unixNanosecondsFromFileTime(quint64 ticks);
} // namespace foldersnap
