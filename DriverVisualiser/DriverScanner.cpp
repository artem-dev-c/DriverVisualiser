#include "DriverScanner.h"
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <cfgmgr32.h>   // Required for the status retrieval



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
        DriverInfo info;

        info.name         = getProperty(hDevInfo, &devInfoData, SPDRP_DEVICEDESC);
        info.manufacturer = getProperty(hDevInfo, &devInfoData, SPDRP_MFG);
        info.provider     = getProperty(hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME);
        info.deviceClass  = getProperty(hDevInfo, &devInfoData, SPDRP_CLASS);
        
        // --- SECOND API CALL TO GET VERSION, INSTALATION DATE AND STATUS ---
        if (SetupDiBuildDriverInfoList(hDevInfo, &devInfoData, SPDIT_COMPATDRIVER)){

            SP_DRVINFO_DATA_W drvData;
            drvData.cbSize = sizeof(SP_DRVINFO_DATA_W);

            if (SetupDiEnumDriverInfoW(hDevInfo, &devInfoData, SPDIT_COMPATDRIVER, 0, &drvData)){
                
                // Version is a 64-bit value.
                DWORDLONG version = drvData.DriverVersion;
                wchar_t verBuf[100];
                swprintf(verBuf, 100, L"%u.%u.%u.%u", 
                         (unsigned)((version >> 48) & 0xFFFF), 
                         (unsigned)((version >> 32) & 0xFFFF), 
                         (unsigned)((version >> 16) & 0xFFFF), 
                         (unsigned)(version & 0xFFFF));
                info.version = verBuf;

                SYSTEMTIME st;
                FileTimeToSystemTime(&drvData.DriverDate, &st);
                wchar_t dateBuf[50];
                swprintf(dateBuf, 50, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
                info.installDate = dateBuf;
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
