#include "DriverDateFormatter.h"

DriverDateInfo DriverDateFormatter::formatSmartDate(
    const std::optional<std::chrono::sys_days>& driverDate,
    const std::optional<std::chrono::sys_days>& installDate)
{
    DriverDateInfo info;
    
    // Try driver date first (if valid)
    if (isDriverDateValid(driverDate)) {
        info.label = "Driver Date";
        info.dateText = dateToString(driverDate);
        return info;
    }
    
    // Fall back to install date
    if (installDate.has_value()) {
        info.label = "Installed";
        info.dateText = dateToString(installDate);
        return info;
    }
    
    // Neither available
    info.label = "Date";
    info.dateText = "Unknown";
    return info;
}

bool DriverDateFormatter::isDriverDateValid(const std::optional<std::chrono::sys_days>& date)
{
    if (!date.has_value()) {
        return false;
    }
    
    auto ymd = std::chrono::year_month_day{*date};
    int year = static_cast<int>(ymd.year());
    unsigned month = static_cast<unsigned>(ymd.month());
    unsigned day = static_cast<unsigned>(ymd.day());
    
    // Get current year for upper bound check
    auto now = std::chrono::system_clock::now();
    auto nowDays = std::chrono::floor<std::chrono::days>(now);
    auto nowYmd = std::chrono::year_month_day{nowDays};
    int currentYear = static_cast<int>(nowYmd.year());
    
    // Rule 1: Year too old (before Windows XP era)
    if (year < 2001) {
        return false;
    }
    
    // Rule 2: Year in the future
    if (year > currentYear) {
        return false;
    }
    
    // Rule 3: Common Microsoft placeholder date (2006-06-21)
    if (year == 2006 && month == 6 && day == 21) {
        return false;
    }
    
    // Rule 4: Another common placeholder (2006-06-01)
    if (year == 2006 && month == 6 && day == 1) {
        return false;
    }
    
    // Rule 5: Very old dates that are likely placeholders
    // Many system drivers use dates from 2006
    // But vendor drivers from 2006+ could be legitimate
    // We'll be conservative and only reject the specific placeholders above
    
    return true;
}

QString DriverDateFormatter::dateToString(const std::optional<std::chrono::sys_days>& date)
{
    if (!date.has_value()) {
        return "Unknown";
    }

    auto ymd = std::chrono::year_month_day{*date};

    return QString("%1-%2-%3")
        .arg(static_cast<int>(ymd.year()), 4, 10, QChar('0'))
        .arg(static_cast<unsigned>(ymd.month()), 2, 10, QChar('0'))
        .arg(static_cast<unsigned>(ymd.day()), 2, 10, QChar('0'));
}