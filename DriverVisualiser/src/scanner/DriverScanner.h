#pragma once

#include <vector>
#include <string>
#include <windows.h>   // for ULONG
#include <cfgmgr32.h>  // for DEVINST
#include "DriverInfo.h"

class DriverScanner {
public:
    DriverScanner();

    /**
     * @brief Enumerates all present devices and collects basic driver information.
     * @return A vector of DriverInfo structures containing information about each driver.
     */
    std::vector<DriverInfo> fetchDrivers();

private:

    std::wstring getProperty(void* hDevInfo, void* devInfoData, unsigned long property);
    
    std::wstring getDeviceProperty(void* hDevInfo, void* devInfoData, const void* propertyKey);

    GUID getDevicePropertyGuid(void* hDevInfo, void* devInfoData, const void* propertyKey);

    std::vector<std::wstring> getDevicePropertyMultiString(void* hDevInfo, void* devInfoData, const void* propertyKey);
    
    DriverStatus getDeviceStatus(DEVINST devInst);

    std::wstring problemCodeToString(ULONG problem);

};
