#ifndef DRIVER_SCANNER_H
#define DRIVER_SCANNER_H

#include <vector>
#include <string>

struct DriverInfo {
    std::wstring name;             ///< Human-readable name of the driver
    std::wstring manufacturer;     ///< Manufacturer of the driver
    std::wstring version;          ///< Version of the driver
    std::wstring provider;         ///< Provider of the driver    
    std::wstring status;           ///< Current status of the driver e.g "Active", "Disabled"
    std::wstring deviceClass;      ///< Class of the device the driver is associated with
    std::wstring installDate;      ///< Installation date of the driver
};

/**
 * @class DriverScanner
 * @brief Scans and retrieves information about installed device drivers on the system.
 */

class DriverScanner {
public:
    DriverScanner();

    std::vector<DriverInfo> fetchDrivers();

private:

    std::wstring getProperty(void* hDevInfo, void* devInfoData, unsigned long property);

};

#endif // DRIVER_SCANNER_H



