#pragma once

#include "ErrorLogEntry.h"
#include "DriverInfo.h"
#include <vector>
#include <chrono>
#include <windows.h>
#include <winevt.h>

/**
 * @class ErrorLogReader
 * @brief Queries Windows Event Log for driver-related events.
 *
 * Retrieves error/warning events from the System log for the last 7 days
 * from driver-related providers (Kernel-PnP, DriverFrameworks).
 * Matches events to specific drivers using Instance ID.
 */
class ErrorLogReader {
public:
    /// Query System log for all driver-related events (last 7 days)
    static std::vector<ErrorLogEntry> querySystemLog();
    
    /// Match events to specific driver by Instance ID (exact match only)
    static std::vector<ErrorLogEntry> filterForDriver(
        const std::vector<ErrorLogEntry>& allEvents,
        const std::wstring& driverInstanceId
    );
    
    /// Populate errorLog field for all drivers
    static void populateErrorLogs(std::vector<DriverInfo>& drivers);

private:
    /// Parse single event handle into ErrorLogEntry
    static ErrorLogEntry parseEvent(EVT_HANDLE hEvent);
    
    /// Extract XML field value from event
    static std::wstring extractXmlField(const std::wstring& xml, const std::wstring& tagName);
    
    /// Extract Event ID from XML
    static int extractEventId(const std::wstring& xml);
    
    /// Extract timestamp from XML
    static std::chrono::system_clock::time_point extractTimestamp(const std::wstring& xml);
    
    /// Extract EventData field (e.g., DeviceInstanceId)
    static std::optional<std::wstring> extractEventDataField(
        const std::wstring& xml, 
        const std::wstring& fieldName
    );
};