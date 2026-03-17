#include "DriverScanner.h"
#include "HealthScoreEvaluator.h"

#define INITGUID
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <devpkey.h>
#include <cfgmgr32.h>

std::vector<DriverInfo> DriverScanner::fetchDrivers()
{
    std::vector<DriverInfo> driverList;

    // Get handle to all present devices
    HDEVINFO hDevInfo = SetupDiGetClassDevsW(
        nullptr, nullptr, nullptr,
        DIGCF_PRESENT | DIGCF_ALLCLASSES
    );

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        return driverList;
    }

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    // Enumerate each device
    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++) {
        DriverInfo info{};

        // === Basic Properties ===
        info.name = getProperty(hDevInfo, &devInfoData, SPDRP_DEVICEDESC);
        info.manufacturer = getProperty(hDevInfo, &devInfoData, SPDRP_MFG);
        info.provider = getProperty(hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME);
        info.deviceClassName = getProperty(hDevInfo, &devInfoData, SPDRP_CLASS);
        info.deviceClassGuid = devInfoData.ClassGuid;

        // === Instance ID ===
        wchar_t instanceId[MAX_DEVICE_ID_LEN];
        if (SetupDiGetDeviceInstanceIdW(hDevInfo, &devInfoData, instanceId, MAX_DEVICE_ID_LEN, nullptr)) {
            info.instanceId = instanceId;
        } else {
            info.instanceId = L"Unknown";
        }

        // === Container ID (for device grouping) ===
        info.containerId = getDevicePropertyGuid(hDevInfo, &devInfoData, &DEVPKEY_Device_ContainerId);

        // === Parent Device ===
        DEVINST parentDevInst;
        if (CM_Get_Parent(&parentDevInst, devInfoData.DevInst, 0) == CR_SUCCESS) {
            wchar_t parentId[MAX_DEVICE_ID_LEN];
            if (CM_Get_Device_IDW(parentDevInst, parentId, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS) {
                info.parentInstanceId = parentId;
                // Get parent status
                info.parentStatus = getDeviceStatus(parentDevInst);
            }
        }

        // === Hardware IDs ===
        auto hardwareIds = getDevicePropertyMultiString(hDevInfo, &devInfoData, &DEVPKEY_Device_HardwareIds);
        if (!hardwareIds.empty()) {
            info.hardwareId = hardwareIds[0];
            info.compatibleIds = hardwareIds;
        }

        // === Location ===
        info.locationPath = getDeviceProperty(hDevInfo, &devInfoData, &DEVPKEY_Device_LocationInfo);

        // === Status and Problem Code ===
        ULONG status = 0;
        ULONG problemCode = 0;
        if (CM_Get_DevNode_Status(&status, &problemCode, devInfoData.DevInst, 0) == CR_SUCCESS) {
            info.problemCode = problemCode;
            info.rawStatus = status;
            info.status = getDeviceStatus(devInfoData.DevInst);
        } else {
            info.status = DriverStatus::Unknown;
        }

        // === Driver Version ===
        std::wstring versionStr = getDeviceProperty(hDevInfo, &devInfoData, &DEVPKEY_Device_DriverVersion);
        if (!versionStr.empty()) {
            unsigned int major = 0, minor = 0, build = 0, revision = 0;
            if (swscanf_s(versionStr.c_str(), L"%u.%u.%u.%u", &major, &minor, &build, &revision) >= 1) {
                info.version.major = static_cast<uint16_t>(major);
                info.version.minor = static_cast<uint16_t>(minor);
                info.version.build = static_cast<uint16_t>(build);
                info.version.revision = static_cast<uint16_t>(revision);
                info.version.hasVersion = true;
            }
        }

        // === Dates ===
        info.driverDate = getDevicePropertyFileTime(hDevInfo, &devInfoData, &DEVPKEY_Device_DriverDate);
        info.installDate = getDevicePropertyFileTime(hDevInfo, &devInfoData, &DEVPKEY_Device_InstallDate);

        // === INF Path ===
        info.driverInfPath = getDeviceProperty(hDevInfo, &devInfoData, &DEVPKEY_Device_DriverInfPath);

        // === Driver Files ===
        info.driverFiles = getDriverFiles(hDevInfo, &devInfoData);

        // === Defaults ===
        info.isSigned = true;  // TODO: Implement actual signature check

        // === Health Evaluation ===
        HealthResult healthResult = HealthScoreEvaluator::evaluate(info);
        info.healthScore = healthResult.score;
        info.healthFlags = healthResult.flags;

        driverList.push_back(info);
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return driverList;
}

DriverStatus DriverScanner::getDeviceStatus(DEVINST devInst)
{
    ULONG status = 0;
    ULONG problem = 0;

    if (CM_Get_DevNode_Status(&status, &problem, devInst, 0) != CR_SUCCESS) {
        return DriverStatus::Unknown;
    }

    if (status & DN_HAS_PROBLEM) {
        return DriverStatus::Error;
    }

    if (status & DN_STARTED) {
        return DriverStatus::Ok;
    }

    return DriverStatus::NotStarted;
}

// ============================================================================
// Property Retrieval Methods
// ============================================================================

std::wstring DriverScanner::getProperty(HDEVINFO hDevInfo, PSP_DEVINFO_DATA devInfoData, DWORD property)
{
    DWORD requiredSize = 0;

    // First call: get required buffer size
    SetupDiGetDeviceRegistryPropertyW(hDevInfo, devInfoData, property, nullptr, nullptr, 0, &requiredSize);

    if (requiredSize > 0) {
        std::vector<wchar_t> buffer(requiredSize / sizeof(wchar_t) + 1);
        
        // Second call: retrieve the property
        if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, devInfoData, property, nullptr,
                reinterpret_cast<PBYTE>(buffer.data()), requiredSize, nullptr)) {
            return std::wstring(buffer.data());
        }
    }

    return L"Unknown";
}

std::wstring DriverScanner::getDeviceProperty(HDEVINFO hDevInfo, PSP_DEVINFO_DATA devInfoData, const DEVPROPKEY* key)
{
    DEVPROPTYPE propertyType;
    DWORD requiredSize = 0;

    // First call: get required buffer size
    SetupDiGetDevicePropertyW(hDevInfo, devInfoData, key, &propertyType, nullptr, 0, &requiredSize, 0);

    if (requiredSize > 0) {
        std::vector<BYTE> buffer(requiredSize);
        
        // Second call: retrieve the property
        if (SetupDiGetDevicePropertyW(hDevInfo, devInfoData, key, &propertyType,
                buffer.data(), requiredSize, nullptr, 0)) {
            if (propertyType == DEVPROP_TYPE_STRING) {
                return std::wstring(reinterpret_cast<wchar_t*>(buffer.data()));
            }
        }
    }

    return L"";
}

GUID DriverScanner::getDevicePropertyGuid(HDEVINFO hDevInfo, PSP_DEVINFO_DATA devInfoData, const DEVPROPKEY* key)
{
    DEVPROPTYPE propertyType;
    GUID guid = {};
    DWORD requiredSize = sizeof(GUID);

    if (SetupDiGetDevicePropertyW(hDevInfo, devInfoData, key, &propertyType,
            reinterpret_cast<PBYTE>(&guid), requiredSize, nullptr, 0)) {
        if (propertyType == DEVPROP_TYPE_GUID) {
            return guid;
        }
    }

    return {};
}

std::vector<std::wstring> DriverScanner::getDevicePropertyMultiString(HDEVINFO hDevInfo, PSP_DEVINFO_DATA devInfoData, const DEVPROPKEY* key)
{
    std::vector<std::wstring> result;
    DEVPROPTYPE propertyType;
    DWORD requiredSize = 0;

    // First call: get required buffer size
    SetupDiGetDevicePropertyW(hDevInfo, devInfoData, key, &propertyType, nullptr, 0, &requiredSize, 0);

    if (requiredSize > 0) {
        std::vector<BYTE> buffer(requiredSize);
        
        // Second call: retrieve the property
        if (SetupDiGetDevicePropertyW(hDevInfo, devInfoData, key, &propertyType,
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

std::optional<std::chrono::sys_days> DriverScanner::getDevicePropertyFileTime(HDEVINFO hDevInfo, PSP_DEVINFO_DATA devInfoData, const DEVPROPKEY* key)
{
    DEVPROPTYPE propertyType;
    FILETIME ft = {};
    DWORD requiredSize = sizeof(FILETIME);

    if (SetupDiGetDevicePropertyW(hDevInfo, devInfoData, key, &propertyType,
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
std::vector<std::wstring> DriverScanner::getDriverFiles(HDEVINFO hDevInfo, PSP_DEVINFO_DATA devInfoData)
{
    std::vector<std::wstring> files;
    
    // Get driver info data to access file list
    SP_DRVINFO_DATA drvInfoData;
    drvInfoData.cbSize = sizeof(SP_DRVINFO_DATA);
    
    // Build driver list for this device
    if (!SetupDiBuildDriverInfoList(hDevInfo, devInfoData, SPDIT_COMPATDRIVER)) {
        return files;
    }
    
    // Get the selected/installed driver (index 0)
    if (!SetupDiEnumDriverInfo(hDevInfo, devInfoData, SPDIT_COMPATDRIVER, 0, &drvInfoData)) {
        SetupDiDestroyDriverInfoList(hDevInfo, devInfoData, SPDIT_COMPATDRIVER);
        return files;
    }
    
    // Get driver detail info (includes INF path)
    DWORD requiredSize = 0;
    SetupDiGetDriverInfoDetailW(hDevInfo, devInfoData, &drvInfoData, nullptr, 0, &requiredSize);
    
    if (requiredSize > 0) {
        // Allocate buffer for detail structure
        std::vector<BYTE> buffer(requiredSize);
        PSP_DRVINFO_DETAIL_DATA detailData = reinterpret_cast<PSP_DRVINFO_DETAIL_DATA>(buffer.data());
        detailData->cbSize = sizeof(SP_DRVINFO_DETAIL_DATA);
        
        if (SetupDiGetDriverInfoDetailW(hDevInfo, devInfoData, &drvInfoData, 
                                         detailData, requiredSize, nullptr)) {
            // Add INF file path
            if (detailData->InfFileName[0] != L'\0') {
                files.push_back(detailData->InfFileName);
            }
        }
    }
    
    // Now get the actual .sys file from the service registry
    // Get the service name for this device
    DWORD serviceNameSize = 0;
    SetupDiGetDeviceRegistryPropertyW(hDevInfo, devInfoData, SPDRP_SERVICE, 
                                      nullptr, nullptr, 0, &serviceNameSize);
    
    if (serviceNameSize > 0) {
        std::vector<BYTE> serviceBuffer(serviceNameSize);
        DWORD propertyType = 0;
        
        if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, devInfoData, SPDRP_SERVICE, 
                                              &propertyType, serviceBuffer.data(), 
                                              serviceNameSize, nullptr)) {
            std::wstring serviceName = reinterpret_cast<LPCWSTR>(serviceBuffer.data());
            
            // Query the service registry to get the .sys file path
            std::wstring serviceRegPath = L"SYSTEM\\CurrentControlSet\\Services\\" + serviceName;
            HKEY hServiceKey = nullptr;
            
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, serviceRegPath.c_str(), 0, 
                             KEY_QUERY_VALUE, &hServiceKey) == ERROR_SUCCESS) {
                
                // Get ImagePath value (contains the .sys file path)
                DWORD imagePathSize = 0;
                DWORD valueType = 0;
                
                if (RegQueryValueExW(hServiceKey, L"ImagePath", nullptr, &valueType, 
                                    nullptr, &imagePathSize) == ERROR_SUCCESS) {
                    
                    std::vector<BYTE> imagePathBuffer(imagePathSize);
                    
                    if (RegQueryValueExW(hServiceKey, L"ImagePath", nullptr, &valueType,
                                        imagePathBuffer.data(), &imagePathSize) == ERROR_SUCCESS) {
                        
                        std::wstring imagePath = reinterpret_cast<LPCWSTR>(imagePathBuffer.data());
                        
                        // ImagePath might be relative like "system32\drivers\xyz.sys"
                        // Convert to absolute path
                        if (imagePath.find(L':') == std::wstring::npos) {
                            // Relative path - prepend Windows directory
                            wchar_t winDir[MAX_PATH];
                            if (GetWindowsDirectoryW(winDir, MAX_PATH) > 0) {
                                imagePath = std::wstring(winDir) + L"\\" + imagePath;
                            }
                        }
                        
                        // Clean up path (remove \??\ prefix if present)
                        if (imagePath.starts_with(L"\\??\\")) {
                            imagePath = imagePath.substr(4);
                        }
                        
                        // Only add if it's a .sys file
                        if (imagePath.ends_with(L".sys") || imagePath.ends_with(L".SYS")) {
                            files.push_back(imagePath);
                        }
                    }
                }
                
                RegCloseKey(hServiceKey);
            }
        }
    }
    
    // Clean up driver info list
    SetupDiDestroyDriverInfoList(hDevInfo, devInfoData, SPDIT_COMPATDRIVER);
    
    return files;
}