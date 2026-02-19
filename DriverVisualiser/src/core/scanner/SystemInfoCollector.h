#pragma once

#include "SystemInfo.h"
#include <windows.h>

/// Collects system information for troubleshooting reports
class SystemInfoCollector {
public:
    /// Collect system information from Windows APIs
    static SystemInfo collect();

private:
    /// Read a string value from registry
    static std::wstring readRegString(HKEY hKey, const std::wstring& valueName);
};