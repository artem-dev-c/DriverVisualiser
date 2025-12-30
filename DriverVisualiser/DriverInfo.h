#ifndef DRIVERINFO_H
#define DRIVERINFO_H

#include <string>

enum class DriverImportance {
    Critical,
    Important,
    Optional,
    Unknown
};

struct DriverInfo {
    std::wstring name;             ///< Human-readable name of the driver
    std::wstring manufacturer;     ///< Manufacturer of the driver
    std::wstring version;          ///< Version of the driver
    std::wstring provider;         ///< Provider of the driver    
    std::wstring status;           ///< Current status of the driver e.g "Active", "Disabled"
    std::wstring deviceClass;      ///< Class of the device the driver is associated with
    std::wstring installDate;      ///< Installation date of the driver
    std::wstring instanceId;       ///< Unique instance ID of the driver

    DriverImportance importanceLevel = DriverImportance::Unknown; ///< Importance level of the driver (e.g., Critical, Optional)
    
};

#endif // DRIVERINFO_H