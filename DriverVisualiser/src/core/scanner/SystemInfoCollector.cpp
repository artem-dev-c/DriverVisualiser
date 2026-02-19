#include "SystemInfoCollector.h"
#include <vector>

SystemInfo SystemInfoCollector::collect()
{
    SystemInfo info;
    
    // === Architecture ===
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            info.architecture = L"x64";
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            info.architecture = L"x86";
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            info.architecture = L"ARM64";
            break;
        default:
            info.architecture = L"Unknown";
            break;
    }
    
    // === OS Information from Registry ===
    HKEY hKey;
    LONG result = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        0,
        KEY_READ,
        &hKey
    );
    
    if (result == ERROR_SUCCESS) {
        info.osName = readRegString(hKey, L"ProductName");
        info.displayVersion = readRegString(hKey, L"DisplayVersion");
        info.buildNumber = readRegString(hKey, L"CurrentBuild");
        
        // UBR (Update Build Revision) - might not exist on older Windows
        std::wstring ubr = readRegString(hKey, L"UBR");
        info.ubr = ubr.empty() ? L"0" : ubr;
        
        RegCloseKey(hKey);
    } else {
        info.osName = L"Unknown";
        info.displayVersion = L"Unknown";
        info.buildNumber = L"Unknown";
        info.ubr = L"0";
    }
    
    return info;
}

std::wstring SystemInfoCollector::readRegString(HKEY hKey, const std::wstring& valueName)
{
    DWORD dataType;
    DWORD dataSize = 0;
    
    // First call: get size
    LONG result = RegQueryValueExW(
        hKey,
        valueName.c_str(),
        nullptr,
        &dataType,
        nullptr,
        &dataSize
    );
    
    if (result != ERROR_SUCCESS || dataType != REG_SZ) {
        return L"";
    }
    
    // Second call: get data
    std::vector<WCHAR> buffer(dataSize / sizeof(WCHAR));
    result = RegQueryValueExW(
        hKey,
        valueName.c_str(),
        nullptr,
        &dataType,
        reinterpret_cast<LPBYTE>(buffer.data()),
        &dataSize
    );
    
    if (result == ERROR_SUCCESS) {
        return std::wstring(buffer.data());
    }
    
    return L"";
}