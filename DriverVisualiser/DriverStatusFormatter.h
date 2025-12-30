#ifndef DRIVER_STATUS_FORMATTER_H
#define DRIVER_STATUS_FORMATTER_H
#include <QString>
#include "DriverInfo.h"

class DriverStatusFormatter {
public:
    /**
     * @brief Converts a DriverStatus enum value to a human-readable QString.
     * @param status The DriverStatus value to convert.
     * @return A QString representing the status.
     */
    static QString statusToString(DriverStatus status);
};

#endif // DRIVER_STATUS_FORMATTER_H