#include "DriverScanner.h"
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

    // Buffer to hold property value
    wchar_t buffer[1024];
    DWORD requiredSize = 0;

    // Api call to get device property
    if (SetupDiGetDeviceRegistryPropertyW(
        handle,
        data,
        property,
        nullptr,
        reinterpret_cast<PBYTE>(buffer),
        sizeof(buffer),
        &requiredSize
    )){
        return std::wstring(buffer);
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
        info.deviceClass  = getProperty(hDevInfo, &devInfoData, SPDRP_CLASS);
        
        // Default values in case second API call fails
        info.version     = L"Unknown";
        info.installDate = L"Unknown";
        info.status      = getDeviceStatus(devInfoData.DevInst);

        
        // Build compatible driver list to retrieve version and install date
        if (SetupDiBuildDriverInfoList(hDevInfo, &devInfoData, SPDIT_COMPATDRIVER)){

            SP_DRVINFO_DATA_W drvData;
            drvData.cbSize = sizeof(SP_DRVINFO_DATA_W);

            if (SetupDiEnumDriverInfoW(hDevInfo, &devInfoData, SPDIT_COMPATDRIVER, 0, &drvData)){
                
                // Version is a 64-bit value.
                DWORDLONG version = drvData.DriverVersion;
                wchar_t verBuf[100];
                swprintf_s(verBuf, 100, L"%u.%u.%u.%u", 
                         (unsigned)((version >> 48) & 0xFFFF), 
                         (unsigned)((version >> 32) & 0xFFFF), 
                         (unsigned)((version >> 16) & 0xFFFF), 
                         (unsigned)(version & 0xFFFF));
                info.version = verBuf;

                SYSTEMTIME st;
                // If Windows api returns false
                if (FileTimeToSystemTime(&drvData.DriverDate, &st)){
                    wchar_t dateBuf[50];
                    swprintf_s(dateBuf, 50, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
                    info.installDate = dateBuf;
                    };
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

std::wstring DriverScanner::getDeviceStatus(DEVINST devInst) {
    ULONG status = 0;
    ULONG problem = 0;

    if (CM_Get_DevNode_Status(&status, &problem, devInst, 0) != CR_SUCCESS) {
        return L"Unknown";
    }

    if (status & DN_HAS_PROBLEM) {
        return problemCodeToString(problem);
    }

    if (status & DN_STARTED) {
        return L"OK";
    }

    return L"Not started";
}

//================== private problemCodeToString() ==================
// Converts a problem code into a human-readable string

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


