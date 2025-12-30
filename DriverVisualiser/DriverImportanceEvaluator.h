#pragma once
#include "DriverInfo.h"
#include <QString>

class DriverImportanceEvaluator {
public:
    /**
     * @brief Converts a DriverImportance enum value to a human-readable string.
     * @param level The DriverImportance level to convert.
     * @return A QString representing the importance level.
     */
    static QString importanceToString(DriverImportance level);

    /**
     * @brief Evaluates the importance level of a driver based on its attributes.
     * @param driver The DriverInfo object containing driver details.
     * @return The evaluated DriverImportance level.
     */
    static DriverImportance evaluate(const DriverInfo& driver);
};
