#pragma once

#include <string>
#include <vector>
#include "DriverInfo.h"

struct DeviceCategory {
    std::wstring className;               ///< Device class name (e.g., "Display", "Net")
    std::wstring displayName;             ///< User-friendly name (e.g., "Display adapters")
    std::vector<DriverInfo> drivers;      ///< All drivers in this category
};