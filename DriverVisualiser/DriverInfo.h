#ifndef DRIVERINFO_H
#define DRIVERINFO_H

#include <string>
#include <optional>
#include <chrono>
#include <windows.h>

enum class DriverImportance {
    Critical,
    Important,
    Optional,
    Unknown
};

enum class DriverStatus {
    Ok,
    Disabled,
    NotStarted,
    Error,
    Unknown
};

struct DriverVersion {
    uint16_t major{};
    uint16_t minor{};
    uint16_t build{};
    uint16_t revision{};
};


struct DriverInfo {
    std::wstring name;             ///< Human-readable name of the driver
    std::wstring manufacturer;     ///< Manufacturer of the driver
    std::wstring provider;         ///< Provider of the driver    
    std::wstring instanceId;       ///< Unique instance ID of the driver

    std::wstring deviceClassName;  ///< Name of the device class
    GUID deviceClassGuid{};        ///< GUID of the device class

    DriverVersion version{};       ///< Version of the driver
    bool hasVersion = false;       ///< Flag indicating if version information is available

    std::optional<std::chrono::sys_days> installDate;             ///< Installation date of the driver    

    DriverStatus status = DriverStatus::Unknown;                  ///< Current status of the driver

    DriverImportance importanceLevel = DriverImportance::Unknown; ///< Importance level of the driver (e.g., Critical, Optional)
    
};

#endif // DRIVERINFO_H