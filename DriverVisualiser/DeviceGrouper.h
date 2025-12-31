#pragma once

#include <map>
#include <vector>
#include "DriverInfo.h"
#include "DeviceInfo.h"

class DeviceGrouper {
public:
    /**
     * @brief Groups drivers by their associated devices.
     * @param drivers A vector of DriverInfo objects to be grouped.
     * @return A map where the key is the device instance ID and the value is the corresponding DeviceInfo.
     */
    static std::map<std::wstring, DeviceInfo>
    groupByDevice(const std::vector<DriverInfo>& drivers);
};
