#pragma once
#include "DriverInfo.h"

class DriverImportanceEvaluator {
public:
    /**
     * @brief Evaluates the importance level of a driver based on its attributes.
     * @param driver The DriverInfo object containing driver details.
     * @return The evaluated DriverImportance level.
     */
    static DriverImportance evaluate(const DriverInfo& driver);
};
