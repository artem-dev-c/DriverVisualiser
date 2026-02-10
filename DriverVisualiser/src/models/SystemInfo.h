#pragma once

#include <string>

/// System information for troubleshooting reports
struct SystemInfo {
    std::wstring osName;           ///< e.g., "Windows 11 Pro"
    std::wstring displayVersion;   ///< e.g., "23H2"
    std::wstring buildNumber;      ///< e.g., "22631"
    std::wstring ubr;              ///< Update Build Revision, e.g., "4602"
    std::wstring architecture;     ///< "x64" or "x86"
};