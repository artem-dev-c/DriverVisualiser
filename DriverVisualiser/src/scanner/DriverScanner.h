#pragma once

#include <vector>
#include <string>
#include <optional>
#include <chrono>
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include "DriverInfo.h"

/**
 * @class DriverScanner
 * @brief Enumerates Windows devices and collects driver information.
 *
 * Uses SetupAPI and CfgMgr32 APIs to query device properties,
 * status, and driver details for all present devices.
 */
class DriverScanner {
public:
    DriverScanner() = default;

    /**
     * @brief Scans all present devices and collects driver information.
     * @return Vector of DriverInfo structures for each device.
     */
    std::vector<DriverInfo> fetchDrivers();

private:
    /// Reads a registry property using legacy SetupDiGetDeviceRegistryProperty API
    std::wstring getProperty(HDEVINFO hDevInfo, PSP_DEVINFO_DATA devInfoData, DWORD property);

    /// Reads a string property using modern SetupDiGetDeviceProperty API
    std::wstring getDeviceProperty(HDEVINFO hDevInfo, PSP_DEVINFO_DATA devInfoData, const DEVPROPKEY* key);

    /// Reads a GUID property
    GUID getDevicePropertyGuid(HDEVINFO hDevInfo, PSP_DEVINFO_DATA devInfoData, const DEVPROPKEY* key);

    /// Reads a multi-string property (REG_MULTI_SZ equivalent)
    std::vector<std::wstring> getDevicePropertyMultiString(HDEVINFO hDevInfo, PSP_DEVINFO_DATA devInfoData, const DEVPROPKEY* key);

    /// Reads a FILETIME property and converts to sys_days
    std::optional<std::chrono::sys_days> getDevicePropertyFileTime(HDEVINFO hDevInfo, PSP_DEVINFO_DATA devInfoData, const DEVPROPKEY* key);

    /// Determines device status from CM_Get_DevNode_Status flags
    DriverStatus getDeviceStatus(DEVINST devInst);
};