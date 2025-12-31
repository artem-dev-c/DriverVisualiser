#pragma once

#include <QString>
#include "DriverInfo.h"

class DriverVersionFormatter {
public:
    /**
     * @brief Converts DriverVersion to human-readable QString
     * @param version The DriverVersion to convert.
     */
    static QString versionToString(const DriverVersion& version);
};


