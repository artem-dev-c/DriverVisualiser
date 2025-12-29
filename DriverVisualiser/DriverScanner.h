#ifndef DRIVER_SCANNER_H
#define DRIVER_SCANNER_H

#include <vector>
#include <string>
#include <windows.h>   // for ULONG
#include <cfgmgr32.h>  // for DEVINST

struct DriverInfo {
    std::wstring name;             ///< Human-readable name of the driver
    std::wstring manufacturer;     ///< Manufacturer of the driver
    std::wstring version;          ///< Version of the driver
    std::wstring provider;         ///< Provider of the driver    
    std::wstring status;           ///< Current status of the driver e.g "Active", "Disabled"
    std::wstring deviceClass;      ///< Class of the device the driver is associated with
    std::wstring installDate;      ///< Installation date of the driver

    // TODO: Add deviceInstanceId (DEVINST) for more detailed identification
    // TODO: Add driverCategory (e.g., System, Network, Display)
    // TODO: Add Importance level (e.g., Critical, Optional)

};

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

    // Fetches a list of installed drivers with their information
    std::vector<DriverInfo> fetchDrivers();

private:

    // Helper to get a specific property of a device
    std::wstring getProperty(void* hDevInfo, void* devInfoData, unsigned long property);

    // Retrieves the current status of the device driver
    std::wstring getDeviceStatus(DEVINST devInst);

    // Converts a problem code into a human-readable string
    std::wstring problemCodeToString(ULONG problem);
};

#endif // DRIVER_SCANNER_H



