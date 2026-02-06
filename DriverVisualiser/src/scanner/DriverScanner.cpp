#include "DriverScanner.h"
#include "HealthScoreEvaluator.h"

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
            info.rawStatus = status;
            info.status = getDeviceStatus(devInfoData.DevInst);
        } else {
            info.status = DriverStatus::Unknown;
        }

        // Driver version and date - use DEVPKEY for currently installed driver
        // (SetupDiBuildDriverInfoList with SPDIT_COMPATDRIVER returns available drivers, not installed)
        
        // Get installed driver version from device property
        std::wstring versionStr = getDeviceProperty(hDevInfo, &devInfoData, &DEVPKEY_Device_DriverVersion);
        if (!versionStr.empty()) {
            // Parse version string like "32.0.15.9174"
            unsigned int major = 0, minor = 0, build = 0, revision = 0;
            if (swscanf_s(versionStr.c_str(), L"%u.%u.%u.%u", &major, &minor, &build, &revision) >= 1) {
                info.version.major = static_cast<uint16_t>(major);
                info.version.minor = static_cast<uint16_t>(minor);
                info.version.build = static_cast<uint16_t>(build);
                info.version.revision = static_cast<uint16_t>(revision);
                info.version.hasVersion = true;
            }
        }

        // Get installed driver date from device property
        info.driverDate = getDevicePropertyFileTime(hDevInfo, &devInfoData, &DEVPKEY_Device_DriverDate);

        // Get actual install date (when driver was installed on THIS system)
        info.installDate = getDevicePropertyFileTime(hDevInfo, &devInfoData, &DEVPKEY_Device_InstallDate);

        // Get driver INF path from device property
        info.driverInfPath = getDeviceProperty(hDevInfo, &devInfoData, &DEVPKEY_Device_DriverInfPath);

        info.isSigned = true;

        // Evaluate health score and flags
        HealthResult healthResult = HealthScoreEvaluator::evaluate(info);
        info.healthScore = healthResult.score;
        info.healthFlags = healthResult.flags;

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

//================== private getDevicePropertyFileTime() ==================
// Reads a FILETIME device property and converts to sys_days

std::optional<std::chrono::sys_days> DriverScanner::getDevicePropertyFileTime(void* hDevInfo, void* devInfoData, const void* propertyKey)
{
    HDEVINFO handle = static_cast<HDEVINFO>(hDevInfo);
    PSP_DEVINFO_DATA data = static_cast<PSP_DEVINFO_DATA>(devInfoData);
    const DEVPROPKEY* key = static_cast<const DEVPROPKEY*>(propertyKey);

    DEVPROPTYPE propertyType;
    FILETIME ft = {0};
    DWORD requiredSize = sizeof(FILETIME);

    if (SetupDiGetDevicePropertyW(handle, data, key, &propertyType,
        reinterpret_cast<PBYTE>(&ft), requiredSize, nullptr, 0)) {
        
        if (propertyType == DEVPROP_TYPE_FILETIME) {
            SYSTEMTIME st;
            if (FileTimeToSystemTime(&ft, &st)) {
                return std::chrono::sys_days{
                    std::chrono::year{st.wYear} /
                    std::chrono::month{st.wMonth} /
                    std::chrono::day{st.wDay}
                };
            }
        }
    }

    return std::nullopt;
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