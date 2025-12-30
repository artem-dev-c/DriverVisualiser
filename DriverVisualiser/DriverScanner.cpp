#include "DriverScanner.h"
#include "DriverInfo.h"
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <cfgmgr32.h>   // Required for the status retrieval
// TODO: Retrieve device status via CM_Get_DevNode_Status

//================== Constructor ==================

DriverScanner::DriverScanner(){

}

//================== private getProperty() ==================
// Reads a single registry property for a given device
// Returns "Unknown" if the property is unavailable

std::wstring DriverScanner::getProperty(void* hDevInfo, void* devInfodata, unsigned long property){
    // Pointer casting
    HDEVINFO handle = static_cast<HDEVINFO>(hDevInfo);
    PSP_DEVINFO_DATA data = static_cast<PSP_DEVINFO_DATA>(devInfodata);

    DWORD requiredSize = 0;

    // Call 1: Get the required size
    SetupDiGetDeviceRegistryPropertyW(handle, data, property, nullptr, nullptr, 0, &requiredSize);

    if (requiredSize > 0) {
        std::vector<wchar_t> buffer(requiredSize / sizeof(wchar_t) + 1);
        // Call 2: Fill the dynamic buffer
        if (SetupDiGetDeviceRegistryPropertyW(handle, data, property, nullptr, 
            reinterpret_cast<PBYTE>(buffer.data()), requiredSize, nullptr)) {
            return std::wstring(buffer.data());
        }
    }
    return L"Unknown";
}

//================== public fetchDrivers() ==================
// Enumerates all present devices and collects basic driver information

std::vector<DriverInfo> DriverScanner::fetchDrivers(){
    std::vector<DriverInfo> driverList;

    HDEVINFO hDevInfo = SetupDiGetClassDevsW(
        nullptr,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_ALLCLASSES
    );

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        return driverList; // Return empty list on failure
    }

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    // Loop through every device Windows found
    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++) {
        DriverInfo info{};

        info.name         = getProperty(hDevInfo, &devInfoData, SPDRP_DEVICEDESC);
        info.manufacturer = getProperty(hDevInfo, &devInfoData, SPDRP_MFG);
        info.provider     = getProperty(hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME);
        
        info.deviceClassName = getProperty(hDevInfo, &devInfoData, SPDRP_CLASS);
        info.deviceClassGuid = devInfoData.ClassGuid;

        // Default values in case second API call fails
        info.instanceId  = L"Unknown";

        info.status      = getDeviceStatus(devInfoData.DevInst);

        wchar_t instanceId[MAX_DEVICE_ID_LEN];
        if (SetupDiGetDeviceInstanceIdW(hDevInfo, &devInfoData, instanceId, MAX_DEVICE_ID_LEN, nullptr)) {
        // This is the unique 'Primary Key' for the hardware
        info.instanceId = instanceId; 
        } 
        
        // Build compatible driver list to retrieve version and install date
        if (SetupDiBuildDriverInfoList(hDevInfo, &devInfoData, SPDIT_COMPATDRIVER)){

            SP_DRVINFO_DATA_W drvData;
            drvData.cbSize = sizeof(SP_DRVINFO_DATA_W);

            if (SetupDiEnumDriverInfoW(hDevInfo, &devInfoData, SPDIT_COMPATDRIVER, 0, &drvData)) {

                // Extract 64-bit version
                DWORDLONG version = drvData.DriverVersion;

                info.version.major    = static_cast<uint16_t>((version >> 48) & 0xFFFF);
                info.version.minor    = static_cast<uint16_t>((version >> 32) & 0xFFFF);
                info.version.build    = static_cast<uint16_t>((version >> 16) & 0xFFFF);
                info.version.revision = static_cast<uint16_t>(version & 0xFFFF);
                info.version.hasVersion = true;

                SYSTEMTIME st;
                if (FileTimeToSystemTime(&drvData.DriverDate, &st)) {
                    info.installDate = std::chrono::sys_days{
                        std::chrono::year{st.wYear} /
                        std::chrono::month{st.wMonth} /
                        std::chrono::day{st.wDay}
                    };
                }
            }

            // Clean up driver info list
            SetupDiDestroyDriverInfoList(hDevInfo, &devInfoData, SPDIT_COMPATDRIVER);
            
        }

        driverList.push_back(info);
    }

    // Clean up the handle to prevent memory leaks
    SetupDiDestroyDeviceInfoList(hDevInfo);

    return driverList;
}

//================== private getDeviceStatus() ==================
// Retrieves the current status of the device driver

DriverStatus DriverScanner::getDeviceStatus(DEVINST devInst) {
    ULONG status = 0;
    ULONG problem = 0;

    if (CM_Get_DevNode_Status(&status, &problem, devInst, 0) != CR_SUCCESS) {
        return DriverStatus::Unknown;
    }

    // EARLY VERSION: Simplified status evaluation
    if (status & DN_HAS_PROBLEM) {
        return DriverStatus::Error;
    }
       

    if (status & DN_STARTED) {
        return DriverStatus::Ok;
    }

    return DriverStatus::NotStarted;
}

//================== private problemCodeToString() ==================
// Converts a problem code into a human-readable string

// ==================================================================
//                           NOT USED YET
// ==================================================================
std::wstring DriverScanner::problemCodeToString(ULONG problem) {
    switch (problem) {
        case CM_PROB_DISABLED:
            return L"Disabled by user";

        case CM_PROB_DRIVER_FAILED_LOAD:
            return L"Driver failed to load";

        case CM_PROB_FAILED_START:
            return L"Driver failed to start";

        case CM_PROB_NOT_CONFIGURED:
            return L"No driver installed";

        case CM_PROB_DEVICE_NOT_THERE:
            return L"Device not present";

        case CM_PROB_OUT_OF_MEMORY:
            return L"Out of memory";

        default:
            return L"Unknown problem";
    }
}


