#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <guiddef.h>

enum class DriverImportance {
    Critical,
    Important,
    Optional,
    Virtual,
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

    GUID containerId;                           ///< Container ID for grouping related drivers

    std::wstring parentInstanceId;              ///< Parent device instance ID

    std::wstring hardwareId;                    ///< Primary hardware ID
    std::vector<std::wstring> compatibleIds;    ///< Compatible hardware IDs
    std::wstring locationPath;                  ///< Physical location path
    std::wstring driverInfPath;                 ///< Path to the .inf file for this driver
    std::vector<std::wstring> driverFiles;      ///< List of driver files installed by this driver package
    uint32_t problemCode = 0;                   ///< Problem code indicating driver issues (0 if no issues)
    bool isPresent = true;                      ///< Indicates if the driver is currently present in the system
    bool isSigned = true;                       ///< Indicates if the driver is digitally signed
    int faultScore = 0;                         ///< Calculated fault/risk score
};