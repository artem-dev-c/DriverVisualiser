#include "DriverScanner.h"
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>

DriverScanner::DriverScanner(){

}

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
        
        // Version and status retrieval would typically require more complex logic
        info.version      = L"Pending...";
        info.status       = L"Active"; 
        info.installDate  = L"Pending...";

        driverList.push_back(info);
    }

    // Clean up the handle to prevent memory leaks
    SetupDiDestroyDeviceInfoList(hDevInfo);

    return driverList;
}
