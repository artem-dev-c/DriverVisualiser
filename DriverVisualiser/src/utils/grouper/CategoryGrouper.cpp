#include "CategoryGrouper.h"

std::map<std::wstring, DeviceCategory> 
CategoryGrouper::groupByCategory(const std::vector<DriverInfo>& drivers)
{
    std::map<std::wstring, DeviceCategory> categories;
    
    for (const auto& driver : drivers) {
        std::wstring className = driver.deviceClassName;
        
        if (className.empty() || className == L"Unknown") {
            continue;
        }
        
        auto& category = categories[className];
        
        if (category.drivers.empty()) {
            category.className = className;
            category.displayName = className;
        }
        
        category.drivers.push_back(driver);
    }
    
    return categories;
}