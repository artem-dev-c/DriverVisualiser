#include "DriverInstallDateFormatter.h"

QString DriverInstallDateFormatter::dateToString(
    const std::optional<std::chrono::sys_days>& date)
{
    if (!date.has_value()) {
        return "Unknown";
    }

    auto ymd = std::chrono::year_month_day{ *date };

    return QString("%1-%2-%3")
        .arg(int(ymd.year()), 4, 10, QChar('0'))
        .arg(unsigned(ymd.month()), 2, 10, QChar('0'))
        .arg(unsigned(ymd.day()), 2, 10, QChar('0'));
}
