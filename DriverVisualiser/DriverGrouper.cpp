#include "DriverGrouper.h"

// ================== public groupByClass() ==================
// Groups drivers by their device class
DriverGrouper::DriverGroupMap
DriverGrouper::groupByClass(const std::vector<DriverInfo>& drivers)
{
    DriverGroupMap grouped;

    for (const auto& driver : drivers) {
        std::wstring key = driver.deviceClass.empty()
            ? L"Unknown"
            : driver.deviceClass;

        grouped[key].push_back(driver);
    }

    return grouped;
}
