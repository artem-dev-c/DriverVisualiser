#ifndef DRIVER_SCANNER_H
#define DRIVER_SCANNER_H

#include <vector>
#include <string>
#include <windows.h>   // for ULONG
#include <cfgmgr32.h>  // for DEVINST
#include "DriverInfo.h"


/**
 * @class DriverScanner
 * @brief Scans and retrieves information about installed device drivers on the system.
 * 
 * 
 * TODO:
 *  - Implement driver categorization (Windows / OEM / Virtual )
 *  - Implement filtering oprions
 */

class DriverScanner {
public:
    DriverScanner();

    /**
     * @brief Enumerates all present devices and collects basic driver information.
     * @return A vector of DriverInfo structures containing information about each driver.
     */
    std::vector<DriverInfo> fetchDrivers();

private:

    /**
     * @brief Helper to get the device properties
     * @return The property value as a wide string.
     */
    std::wstring getProperty(void* hDevInfo, void* devInfoData, unsigned long property);

    /**
    * @brief Retrieves the current status.
    * @note Uses Configuration Manager (CfgMgr32) to evaluate live device node 
    * status bits and problem codes.
    */
    std::wstring getDeviceStatus(DEVINST devInst);

    /**
     * @brief Converts a problem code into a human-readable string.
     * @param problem The problem code to convert.
     * @return A human-readable string representation of the problem.   
     */
    std::wstring problemCodeToString(ULONG problem);
};

#endif // DRIVER_SCANNER_H



