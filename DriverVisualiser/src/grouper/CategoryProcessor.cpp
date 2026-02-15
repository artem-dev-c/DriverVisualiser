#include "CategoryProcessor.h"

std::vector<ProcessedCategory> CategoryProcessor::process(
    const std::map<std::wstring, DeviceCategory>& categories)
{
    std::vector<ProcessedCategory> result;
    
    // Convert each category to ProcessedCategory with sorted drivers and health info
    for (const auto& [className, category] : categories) {
        ProcessedCategory processed;
        processed.displayName = category.displayName;
        processed.sortedDrivers = sortDrivers(category.drivers);
        processed.healthInfo = analyzeHealth(processed.sortedDrivers);
        result.push_back(processed);
    }
    
    // Sort categories by worst health (problematic first)
    std::sort(result.begin(), result.end(),
        [](const ProcessedCategory& a, const ProcessedCategory& b) {
            // Primary: Worst health (ascending)
            if (a.healthInfo.worstHealth != b.healthInfo.worstHealth) {
                return a.healthInfo.worstHealth < b.healthInfo.worstHealth;
            }
            // Secondary: Name (alphabetical)
            return a.displayName < b.displayName;
        }
    );
    
    return result;
}

std::vector<DriverInfo> CategoryProcessor::sortDrivers(const std::vector<DriverInfo>& drivers)
{
    std::vector<DriverInfo> sorted = drivers;
    
    std::sort(sorted.begin(), sorted.end(),
        [](const DriverInfo& a, const DriverInfo& b) {
            // Primary: Health score (worst first)
            if (a.healthScore != b.healthScore) {
                return a.healthScore < b.healthScore;
            }
            // Secondary: Importance (critical first)
            if (a.importanceLevel != b.importanceLevel) {
                return a.importanceLevel < b.importanceLevel;
            }
            // Tertiary: Name (alphabetical)
            return a.name < b.name;
        }
    );
    
    return sorted;
}

CategoryHealthInfo CategoryProcessor::analyzeHealth(const std::vector<DriverInfo>& drivers)
{
    CategoryHealthInfo info;
    info.totalDrivers = static_cast<int>(drivers.size());
    
    for (const auto& driver : drivers) {
        // SKIP user-disabled drivers (problem code 22 = CM_PROB_DISABLED)
        // These are intentional and shouldn't mark a category as problematic
        bool isUserDisabled = (driver.problemCode == 22);  // CM_PROB_DISABLED
        
        // Track worst health (but ignore user-disabled)
        if (!isUserDisabled && driver.healthScore < info.worstHealth) {
            info.worstHealth = driver.healthScore;
        }
        
        // Categorize by health tier (skip user-disabled)
        if (isUserDisabled) {
            // Count as healthy - it's intentional
            info.healthyCount++;
        } else if (driver.healthScore < 70) {
            info.criticalCount++;
            info.hasIssues = true;
        } else if (driver.healthScore < 90) {
            info.warningCount++;
            info.hasIssues = true;
        } else {
            info.healthyCount++;
        }
    }
    
    return info;
}