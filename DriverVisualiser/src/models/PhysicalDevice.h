#pragma once
#include <string>
#include <vector>
#include <guiddef.h>
#include "DriverInfo.h"

struct PhysicalDevice {
    GUID containerId;                       ///< Container ID for grouping related drivers
    std::wstring displayName;               ///< Display name of the physical device
    std::wstring deviceClassName;           ///< Name of the device class
    std::wstring manufacturer;              ///< Manufacturer of the physical device
    
    std::vector<DriverInfo> drivers;
    
    DriverImportance maxImportance = DriverImportance::Unknown;
    int maxFaultScore = 0;
    DriverStatus worstStatus = DriverStatus::Unknown;
    
    const DriverInfo* getPrimaryDriver() const {
        if (drivers.empty()) return nullptr;
        
        const DriverInfo* primary = &drivers[0];
        int highestPriority = -1;
        
        for (const auto& driver : drivers) {
            int priority = 0;
            
            // Priority by device class
            if (driver.deviceClassName == L"Display") {
                priority = 100;
            } else if (driver.deviceClassName == L"MEDIA") {
                priority = 90;
            } else if (driver.deviceClassName == L"Net") {
                priority = 85;
            } else if (driver.deviceClassName == L"HIDClass") {
                priority = 80;
            } else {
                // Priority by importance level
                switch (driver.importanceLevel) {
                    case DriverImportance::Critical:  priority = 70; break;
                    case DriverImportance::Important: priority = 60; break;
                    case DriverImportance::Optional:  priority = 40; break;
                    case DriverImportance::Unknown:   priority = 30; break;
                }
            }
            
            if (priority > highestPriority) {
                highestPriority = priority;
                primary = &driver;
            }
        }
        
        return primary;
    }
};
