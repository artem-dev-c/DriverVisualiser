#pragma once

#include <QString>
#include <optional>
#include <chrono>

struct DriverDateInfo {
    QString label;      // "Driver Date" or "Installed"
    QString dateText;   // "2023-11-02" or "Unknown"
};

class DriverDateFormatter {
public:
    /**
     * @brief Smart date formatting - uses driver date if valid, falls back to install date
     * @param driverDate The driver compile/release date from INF (may be garbage)
     * @param installDate The actual installation date on this system (reliable)
     * @return DriverDateInfo with appropriate label and formatted date
     */
    static DriverDateInfo formatSmartDate(
        const std::optional<std::chrono::sys_days>& driverDate,
        const std::optional<std::chrono::sys_days>& installDate);
    
    /**
     * @brief Checks if a driver date appears to be valid (not a placeholder)
     * @param date The date to validate
     * @return true if the date seems legitimate
     */
    static bool isDriverDateValid(const std::optional<std::chrono::sys_days>& date);
    
    /**
     * @brief Converts a date to YYYY-MM-DD string format
     * @param date The date to format
     * @return Formatted date string or "Unknown"
     */
    static QString dateToString(const std::optional<std::chrono::sys_days>& date);
};