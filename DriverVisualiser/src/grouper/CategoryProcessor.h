#pragma once

#include <vector>
#include <map>
#include <algorithm>
#include "DriverInfo.h"
#include "DeviceCategory.h"

/// Health statistics for a category (pure data)
struct CategoryHealthInfo {
    int totalDrivers = 0;
    int criticalCount = 0;    // Health < 70
    int warningCount = 0;     // Health 70-89  
    int healthyCount = 0;     // Health >= 90
    int worstHealth = 100;    // Lowest health score
    bool hasIssues = false;   // True if any driver < 90
};

/// Processed category ready for UI display
struct ProcessedCategory {
    std::wstring displayName;
    std::vector<DriverInfo> sortedDrivers;  // Already sorted
    CategoryHealthInfo healthInfo;
};

class CategoryProcessor
{
public:
    /// Process categories: sort drivers, analyze health, sort categories
    static std::vector<ProcessedCategory> process(
        const std::map<std::wstring, DeviceCategory>& categories);

private:
    static std::vector<DriverInfo> sortDrivers(const std::vector<DriverInfo>& drivers);
    static CategoryHealthInfo analyzeHealth(const std::vector<DriverInfo>& drivers);
};