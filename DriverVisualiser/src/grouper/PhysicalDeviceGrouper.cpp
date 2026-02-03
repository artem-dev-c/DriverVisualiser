#include "PhysicalDeviceGrouper.h"
#include <debugapi.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

std::map<std::wstring, PhysicalDevice>
PhysicalDeviceGrouper::groupByContainerId(const std::vector<DriverInfo>& drivers)
{
    std::map<std::wstring, PhysicalDevice> physicalDevices;
    std::map<std::wstring, int> containerCounts;
    
    for (const auto& driver : drivers) {
        std::wstring groupKey;
        
        if (!isNullGuid(driver.containerId)) {
        // Strategy 1: Container ID (best for external devices)
            groupKey = guidToString(driver.containerId);
        } else if (!driver.parentInstanceId.empty()) {
            // Strategy 2: Parent device + class (for internal chipset devices)
            groupKey = L"PARENT_" + driver.parentInstanceId + L"_CLASS_" + driver.deviceClassName;
        } else {
            // Strategy 3: Standalone (truly unique devices)
            groupKey = L"STANDALONE_" + driver.instanceId;
        }
        
        containerCounts[groupKey]++;
        
        auto& device = physicalDevices[groupKey];
        
        if (device.drivers.empty()) {
            device.containerId = driver.containerId;
        }
        
        device.drivers.push_back(driver);
    }
    
    // Debug: Show top 10 largest groups
    std::vector<std::pair<int, std::wstring>> sizes;
    for (const auto& [key, count] : containerCounts) {
        sizes.push_back({count, key});
    }
    std::sort(sizes.rbegin(), sizes.rend());
    
    OutputDebugStringW(L"=== Top 10 Largest Groups ===\n");
    for (int i = 0; i < std::min(10, (int)sizes.size()); i++) {
        std::wstring msg = L"Group " + std::to_wstring(i+1) + L": " + 
                          std::to_wstring(sizes[i].first) + L" drivers in " + 
                          sizes[i].second + L"\n";
        OutputDebugStringW(msg.c_str());
    }
    
    OutputDebugStringW(L"\n=== Examining Top Groups ===\n");
    for (int i = 0; i < std::min(3, (int)sizes.size()); i++) {
        const auto& groupKey = sizes[i].second;
        const auto& device = physicalDevices[groupKey];
        
        std::wstring msg = L"\nGroup " + std::to_wstring(i+1) + L" (" + 
                          std::to_wstring(sizes[i].first) + L" drivers):\n";
        msg += L"  Primary: " + (device.drivers.empty() ? L"none" : device.drivers[0].name) + L"\n";
        
        for (int j = 0; j < std::min(3, (int)device.drivers.size()); j++) {
            msg += L"  - " + device.drivers[j].name + L" [" + device.drivers[j].deviceClassName + L"]\n";
        }
        
        OutputDebugStringW(msg.c_str());
    }
    
    for (auto& [key, device] : physicalDevices) {
        device.displayName = determineDisplayName(device);
        device.deviceClassName = determineDeviceClass(device);
        device.manufacturer = determineManufacturer(device);
        
        device.worstStatus = DriverStatus::Ok;
        for (const auto& driver : device.drivers) {
            if (driver.status == DriverStatus::Error) {
                device.worstStatus = DriverStatus::Error;
                break;
            }
        }
    }
    
    return physicalDevices;
}

std::wstring PhysicalDeviceGrouper::guidToString(const GUID& guid)
{
    std::wostringstream oss;
    oss << std::hex << std::uppercase << std::setfill(L'0')
        << L"{"
        << std::setw(8) << guid.Data1 << L"-"
        << std::setw(4) << guid.Data2 << L"-"
        << std::setw(4) << guid.Data3 << L"-";
    
    for (int i = 0; i < 2; i++) {
        oss << std::setw(2) << static_cast<int>(guid.Data4[i]);
    }
    oss << L"-";
    for (int i = 2; i < 8; i++) {
        oss << std::setw(2) << static_cast<int>(guid.Data4[i]);
    }
    oss << L"}";
    
    return oss.str();
}

bool PhysicalDeviceGrouper::isNullGuid(const GUID& guid)
{
    static const GUID NULL_GUID = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};
    static const GUID INVALID_GUID = {0, 0, 0, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    
    return (memcmp(&guid, &NULL_GUID, sizeof(GUID)) == 0) ||
           (memcmp(&guid, &INVALID_GUID, sizeof(GUID)) == 0);
}

std::wstring PhysicalDeviceGrouper::determineDisplayName(const PhysicalDevice& device)
{
    const DriverInfo* primary = device.getPrimaryDriver();
    
    if (primary != nullptr && !primary->name.empty()) {
        return primary->name;
    }
    
    if (!device.deviceClassName.empty()) {
        return device.deviceClassName + L" Device";
    }
    
    return L"Unknown Device";
}

std::wstring PhysicalDeviceGrouper::determineDeviceClass(const PhysicalDevice& device)
{
    const DriverInfo* primary = device.getPrimaryDriver();
    return (primary && !primary->deviceClassName.empty()) 
        ? primary->deviceClassName 
        : L"Unknown";
}

std::wstring PhysicalDeviceGrouper::determineManufacturer(const PhysicalDevice& device)
{
    const DriverInfo* primary = device.getPrimaryDriver();
    return (primary && !primary->manufacturer.empty()) 
        ? primary->manufacturer 
        : L"Unknown";
}