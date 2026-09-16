#pragma once

#include <optional>

#include <QTimeZone>

#include "domain/Configuration.h"

namespace foldersnap
{
struct ScheduleDecision
{
    bool shouldRun{false};
    std::optional<UtcTimestamp> nextDueAtUtc;
};

class ScheduleCalculator final
{
  public:
    [[nodiscard]] static ScheduleDecision evaluate(const Schedule &schedule, UtcTimestamp nowUtc,
                                                   const QTimeZone &timeZone);
};
} // namespace foldersnap
