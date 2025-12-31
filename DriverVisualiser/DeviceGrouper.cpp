#include "DeviceGrouper.h"

std::map<std::wstring, DeviceInfo>
DeviceGrouper::groupByDevice(const std::vector<DriverInfo>& drivers)
{
    std::map<std::wstring, DeviceInfo> devices;

    for (const auto& driver : drivers) {
        auto& device = devices[driver.instanceId];

        // Initialize device only once
        if (device.instanceId.empty()) {
            device.instanceId      = driver.instanceId;
            device.deviceClassName = driver.deviceClassName;
            device.deviceClassGuid = driver.deviceClassGuid;
            device.manufacturer    = driver.manufacturer;

            // TEMP: device name fallback
            device.name   = driver.name;
        }

        device.drivers.push_back(driver);
    }

    return devices;
}
