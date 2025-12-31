#pragma once

#include <string>
#include <vector>
#include <guiddef.h>
#include "DriverInfo.h"

struct DeviceInfo {
    std::wstring name;                  ///< Human-readable name of the device
    std::wstring instanceId;            ///< Unique instance ID of the device
    std::wstring manufacturer;          ///< Manufacturer of the device
    
    std::wstring deviceClassName;       ///< Name of the device class
    GUID deviceClassGuid;

    std::vector<DriverInfo> drivers;    ///< List of drivers associated with the device
};
