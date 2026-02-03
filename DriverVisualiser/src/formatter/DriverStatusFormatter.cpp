#include "DriverStatusFormatter.h"

QString DriverStatusFormatter::statusToString(DriverStatus status) {
    switch (status) {
        case DriverStatus::Ok:
            return "OK";

        case DriverStatus::NotStarted:
            return "Not started";

        case DriverStatus::Error:
            return "Error";

        case DriverStatus::Unknown:
        default:
            return "Unknown";
    }
}
