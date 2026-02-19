#pragma once

#include <QString>
#include "DriverInfo.h"

class DriverImportanceFormatter {
    public:
        /**
         * @brief Converts a DriverImportance enum value to a human-readable string.
         * @param level The DriverImportance level to convert.
         * @return A QString representing the importance level.
         */
        static QString importanceToString(DriverImportance level);

};