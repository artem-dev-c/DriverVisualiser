#pragma once

#include <QString>
#include <optional>
#include <chrono>

class DriverInstallDateFormatter {
public:
    /**
     * @brief Converts install date to human-readable QString
     * @param date The installation date to convert.
     */
    static QString dateToString(const std::optional<std::chrono::sys_days>& date);
};
