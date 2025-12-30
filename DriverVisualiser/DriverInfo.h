#pragma once

#include <string>
#include <chrono>
#include <optional>
#include <guiddef.h>

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
    uint16_t major = 0;
    uint16_t minor = 0;
    uint16_t build = 0;
    uint16_t revision = 0;
    bool hasVersion = false;       ///< Flag indicating if version information is available
};


struct DriverInfo {
    std::wstring name;             ///< Human-readable name of the driver
    std::wstring manufacturer;     ///< Manufacturer of the driver
    std::wstring provider;         ///< Provider of the driver    
    std::wstring instanceId;       ///< Unique instance ID of the driver

    std::wstring deviceClassName;  ///< Name of the device class
    GUID deviceClassGuid;          ///< GUID of the device class

    DriverVersion version;         ///< Version of the driver
   
    std::optional<std::chrono::sys_days> installDate;             ///< Installation date of the driver    

    DriverStatus status = DriverStatus::Unknown;                  ///< Current status of the driver

    DriverImportance importanceLevel = DriverImportance::Unknown; ///< Importance level of the driver (e.g., Critical, Optional)
    
};