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
 * Retrieves error/warning events from the System log for a configurable
 * number of days (default: 7) from driver-related providers
 * (Kernel-PnP, DriverFrameworks-UserMode).
 * Matches events to specific drivers using Instance ID.
 */
class ErrorLogReader {
public:
    /// Query System log for all driver-related events
    /// @param days  Lookback window in days (7, 30, or 90). Default: 7.
    static std::vector<ErrorLogEntry> querySystemLog(int days = 7);
    
    /// Match events to specific driver by Instance ID (exact match only)
    static std::vector<ErrorLogEntry> filterForDriver(
        const std::vector<ErrorLogEntry>& allEvents,
        const std::wstring& driverInstanceId
    );
    
    /// Populate errorLog field for all drivers
    /// @param days  Lookback window in days passed through to querySystemLog.
    static void populateErrorLogs(std::vector<DriverInfo>& drivers, int days = 7);
    
    /// Extract EventData field from XML (used by event message builder)
    static std::optional<std::wstring> extractEventDataField(
        const std::wstring& xml, 
        const std::wstring& fieldName
    );

private:
    /// Parse single event handle into ErrorLogEntry
    static ErrorLogEntry parseEvent(EVT_HANDLE hEvent);
    
    /// Extract XML field value from event
    static std::wstring extractXmlField(const std::wstring& xml, const std::wstring& tagName);
    
    /// Extract Event ID from XML
    static int extractEventId(const std::wstring& xml);
    
    /// Extract timestamp from XML
    static std::chrono::system_clock::time_point extractTimestamp(const std::wstring& xml);
};