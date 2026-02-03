#include "DriverScanner.h"

#define INITGUID

#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <devpkey.h>
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

std::vector<DriverInfo> DriverScanner::fetchDrivers()
{
    std::vector<DriverInfo> driverList;

    HDEVINFO hDevInfo = SetupDiGetClassDevsW(
        nullptr,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_ALLCLASSES
    );

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        return driverList;
    }

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++) {
        DriverInfo info{};

        // Basic information
        info.name         = getProperty(hDevInfo, &devInfoData, SPDRP_DEVICEDESC);
        info.manufacturer = getProperty(hDevInfo, &devInfoData, SPDRP_MFG);
        info.provider     = getProperty(hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME);
        info.deviceClassName = getProperty(hDevInfo, &devInfoData, SPDRP_CLASS);
        info.deviceClassGuid = devInfoData.ClassGuid;

        // Instance ID
        wchar_t instanceId[MAX_DEVICE_ID_LEN];
        if (SetupDiGetDeviceInstanceIdW(hDevInfo, &devInfoData, instanceId, MAX_DEVICE_ID_LEN, nullptr)) {
            info.instanceId = instanceId;
        } else {
            info.instanceId = L"Unknown";
        }

        // Container ID (critical for grouping)
        info.containerId = getDevicePropertyGuid(hDevInfo, &devInfoData, &DEVPKEY_Device_ContainerId);

        // Parent device
        DEVINST parentDevInst;
        if (CM_Get_Parent(&parentDevInst, devInfoData.DevInst, 0) == CR_SUCCESS) {
            wchar_t parentId[MAX_DEVICE_ID_LEN];
            if (CM_Get_Device_IDW(parentDevInst, parentId, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS) {
                info.parentInstanceId = parentId;
            }
        }

        // Hardware IDs
        auto hardwareIds = getDevicePropertyMultiString(hDevInfo, &devInfoData, &DEVPKEY_Device_HardwareIds);
        if (!hardwareIds.empty()) {
            info.hardwareId = hardwareIds[0];
            info.compatibleIds = hardwareIds;
        }

        // Location information
        info.locationPath = getDeviceProperty(hDevInfo, &devInfoData, &DEVPKEY_Device_LocationInfo);

        // Status and problem code
        ULONG status = 0;
        ULONG problemCode = 0;
        if (CM_Get_DevNode_Status(&status, &problemCode, devInfoData.DevInst, 0) == CR_SUCCESS) {
            info.problemCode = problemCode;
            info.status = getDeviceStatus(devInfoData.DevInst);
        } else {
            info.status = DriverStatus::Unknown;
        }

        // Driver version and date
        if (SetupDiBuildDriverInfoList(hDevInfo, &devInfoData, SPDIT_COMPATDRIVER)) {
            SP_DRVINFO_DATA_W drvData;
            drvData.cbSize = sizeof(SP_DRVINFO_DATA_W);

            if (SetupDiEnumDriverInfoW(hDevInfo, &devInfoData, SPDIT_COMPATDRIVER, 0, &drvData)) {
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

                // Driver INF path
                SP_DRVINFO_DETAIL_DATA_W detailData;
                detailData.cbSize = sizeof(SP_DRVINFO_DETAIL_DATA_W);
                if (SetupDiGetDriverInfoDetailW(hDevInfo, &devInfoData, &drvData, &detailData, sizeof(detailData), nullptr) ||
                    GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                    info.driverInfPath = detailData.InfFileName;
                }
            }

            SetupDiDestroyDriverInfoList(hDevInfo, &devInfoData, SPDIT_COMPATDRIVER);
        }

        info.isSigned = true;

        driverList.push_back(info);
    }

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

//================== Modern Device Property API Methods ==================


//================== private getDeviceProperty() ==================
// Reads a device property using the modern SetupDiGetDeviceProperty API

std::wstring DriverScanner::getDeviceProperty(void* hDevInfo, void* devInfoData, const void* propertyKey)
{
    HDEVINFO handle = static_cast<HDEVINFO>(hDevInfo);
    PSP_DEVINFO_DATA data = static_cast<PSP_DEVINFO_DATA>(devInfoData);
    const DEVPROPKEY* key = static_cast<const DEVPROPKEY*>(propertyKey);

    DEVPROPTYPE propertyType;
    DWORD requiredSize = 0;

    SetupDiGetDevicePropertyW(handle, data, key, &propertyType, nullptr, 0, &requiredSize, 0);

    if (requiredSize > 0) {
        std::vector<BYTE> buffer(requiredSize);
        if (SetupDiGetDevicePropertyW(handle, data, key, &propertyType,
            buffer.data(), requiredSize, nullptr, 0)) {
            
            if (propertyType == DEVPROP_TYPE_STRING) {
                return std::wstring(reinterpret_cast<wchar_t*>(buffer.data()));
            }
        }
    }

    return L"";
}

//================== private getDevicePropertyGuid() ==================
// Reads a GUID device property using the modern SetupDiGetDeviceProperty API

GUID DriverScanner::getDevicePropertyGuid(void* hDevInfo, void* devInfoData, const void* propertyKey)
{
    HDEVINFO handle = static_cast<HDEVINFO>(hDevInfo);
    PSP_DEVINFO_DATA data = static_cast<PSP_DEVINFO_DATA>(devInfoData);
    const DEVPROPKEY* key = static_cast<const DEVPROPKEY*>(propertyKey);

    DEVPROPTYPE propertyType;
    GUID guid = {0};
    DWORD requiredSize = sizeof(GUID);

    if (SetupDiGetDevicePropertyW(handle, data, key, &propertyType,
        reinterpret_cast<PBYTE>(&guid), requiredSize, nullptr, 0)) {
        
        if (propertyType == DEVPROP_TYPE_GUID) {
            return guid;
        }
    }

    return {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};
}

//================== private getDevicePropertyMultiString() ==================
// Reads a multi-string device property using the modern SetupDiGetDeviceProperty API

std::vector<std::wstring> DriverScanner::getDevicePropertyMultiString(void* hDevInfo, void* devInfoData, const void* propertyKey)
{
    HDEVINFO handle = static_cast<HDEVINFO>(hDevInfo);
    PSP_DEVINFO_DATA data = static_cast<PSP_DEVINFO_DATA>(devInfoData);
    const DEVPROPKEY* key = static_cast<const DEVPROPKEY*>(propertyKey);

    std::vector<std::wstring> result;
    DEVPROPTYPE propertyType;
    DWORD requiredSize = 0;

    SetupDiGetDevicePropertyW(handle, data, key, &propertyType, nullptr, 0, &requiredSize, 0);

    if (requiredSize > 0) {
        std::vector<BYTE> buffer(requiredSize);
        if (SetupDiGetDevicePropertyW(handle, data, key, &propertyType,
            buffer.data(), requiredSize, nullptr, 0)) {
            
            if (propertyType == DEVPROP_TYPE_STRING_LIST) {
                const wchar_t* ptr = reinterpret_cast<const wchar_t*>(buffer.data());
                while (*ptr != L'\0') {
                    result.push_back(ptr);
                    ptr += wcslen(ptr) + 1;
                }
            }
        }
    }

    return result;
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


