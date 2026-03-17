#pragma once

#include <string>
#include <chrono>
#include <optional>

struct ErrorLogEntry {
    std::chrono::system_clock::time_point timestamp;  ///< When the event occurred
    int eventId = 0;                                  ///< Event ID (e.g., 219 = driver load failed)
    std::wstring level;                               ///< "Critical", "Error", "Warning", "Information"
    std::wstring provider;                            ///< Provider name (e.g., "Microsoft-Windows-Kernel-PnP")
    std::wstring message;                             ///< Full event message text
    std::wstring driverName;                          ///< Friendly driver name (e.g., "NVIDIA Display Driver")
    
    // Matching fields (from EventData)
    std::optional<std::wstring> deviceInstanceId;     ///< Device Instance ID (for matching)
    std::optional<uint32_t> statusCode;               ///< Error status code if available
};