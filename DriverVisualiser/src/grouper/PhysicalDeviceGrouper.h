#pragma once

#include <vector>
#include <map>
#include <guiddef.h>
#include "DriverInfo.h"
#include "PhysicalDevice.h"

/// Groups driver instances into physical devices using Container IDs
class PhysicalDeviceGrouper
{
public:
    /// Groups drivers by Container ID into physical devices
    /// Drivers with the same Container ID are grouped together
    /// Drivers without Container IDs become standalone entries
    static std::map<std::wstring, PhysicalDevice> groupByContainerId(
        const std::vector<DriverInfo>& drivers);

private:
    static std::wstring guidToString(const GUID& guid);
    static bool isNullGuid(const GUID& guid);
    static std::wstring determineDisplayName(const PhysicalDevice& device);
    static std::wstring determineDeviceClass(const PhysicalDevice& device);
    static std::wstring determineManufacturer(const PhysicalDevice& device);
};
