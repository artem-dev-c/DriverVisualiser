#pragma once

#include <vector>
#include <map>
#include <string>
#include "DriverInfo.h"
#include "DeviceCategory.h"

class CategoryGrouper
{
public:
    /// Groups drivers by their device class (like Device Manager)
    static std::map<std::wstring, DeviceCategory> groupByCategory(
        const std::vector<DriverInfo>& drivers);
};