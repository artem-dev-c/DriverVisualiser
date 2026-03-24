#pragma once

#include "ErrorLogEntry.h"
#include "DriverInfo.h"
#include <vector>
#include <unordered_set>
#include <chrono>
#include <windows.h>
#include <winevt.h>

/// ============================================================================
/// ErrorLogReader
/// ============================================================================
/// Queries Windows Event Log for driver-related events using a hybrid
/// whitelist + dynamic discovery approach.
///
/// Architecture:
///   1. Core Windows providers (always queried)
///   2. Dynamic hardware provider discovery (errors/warnings only)
///   3. Event classification (DeviceMapped vs SystemWide vs SwdBackground)
///   4. Event filtering (whitelist lifecycle events, blacklist spam)
///
/// Query Strategy:
///   - System log: All errors/warnings + whitelisted lifecycle events
///   - Configuration log: Kernel-PnP lifecycle events (400, 410, 420, 430, 431)
///   - Dynamic providers: Discovered on first scan, cached for session
///
/// Event Filtering:
///   CAPTURE:
///     - All Critical/Error/Warning (any provider)
///     - Event 400, 420, 430, 431 (Kernel-PnP lifecycle)
///     - Event 410 (stored but hidden via isVisibleInUi = false)
///   
///   EXCLUDE:
///     - Event 6 (FilterManager - filter loaded spam)
///     - Event 7021 (Netwtw14 - WiFi telemetry)
///     - Event 11, 16 (Kernel-General - registry spam)
///     - Event 44 (WindowsUpdateClient - download spam)
///     - Event 153 (Kernel-Boot - VBS status)
/// ============================================================================
class ErrorLogReader {
public:
    /// Query System + Configuration logs for all driver-related events
    /// @param days  Lookback window in days (7, 30, 90, or 180). Default: 7.
    /// @return  Vector of all captured events, sorted newest-first
    static std::vector<ErrorLogEntry> querySystemLog(int days = 7);
    
    /// Match events to specific driver by Instance ID (supports parent-child matching)
    /// @param allEvents  Complete event collection from querySystemLog()
    /// @param driverInstanceId  Device Instance ID to filter by
    /// @return  Vector of events matching this driver
    static std::vector<ErrorLogEntry> filterForDriver(
        const std::vector<ErrorLogEntry>& allEvents,
        const std::wstring& driverInstanceId
    );
    
    /// Populate errorLog field for all drivers
    /// @param drivers  Vector of DriverInfo to populate
    /// @param days  Lookback window passed through to querySystemLog()
    static void populateErrorLogs(std::vector<DriverInfo>& drivers, int days = 7);

    // ========================================================================
    // XML Parsing Helpers (Public - used by event message builder)
    // ========================================================================
    
    /// Extract EventData field from XML (e.g., DeviceInstanceId, DriverVersion)
    static std::optional<std::wstring> extractEventDataField(
        const std::wstring& xml, 
        const std::wstring& fieldName
    );
    
    /// Extract System field from XML (generic tag extraction)
    static std::wstring extractXmlField(const std::wstring& xml, const std::wstring& tagName);
    
    /// Extract Event ID from XML
    static int extractEventId(const std::wstring& xml);
    
    /// Extract timestamp from XML (handles both single and double quote formats)
    static std::chrono::system_clock::time_point extractTimestamp(const std::wstring& xml);

private:
    // ========================================================================
    // Query Execution
    // ========================================================================
    
    /// Query a single event log with XPath filter
    /// @param logName  Log name (e.g., "System", "Microsoft-Windows-Kernel-PnP/Configuration")
    /// @param queryXPath  XPath query string
    /// @param uliCutoff  Time cutoff in FILETIME format
    /// @param days  Lookback window (for debug logging)
    /// @return  Vector of events from this log
    static std::vector<ErrorLogEntry> querySingleLog(
        const std::wstring& logName,
        const std::wstring& queryXPath,
        ULARGE_INTEGER uliCutoff,
        int days
    );
    
    // ========================================================================
    // Event Parsing & Classification
    // ========================================================================
    
    /// Parse single event from EVT_HANDLE
    /// @param hEvent  Event handle from EvtNext()
    /// @return  Parsed ErrorLogEntry with classification applied
    static ErrorLogEntry parseEvent(EVT_HANDLE hEvent);
    
    /// Classify event into category (DeviceMapped/SystemWide/SwdBackground)
    /// @param entry  Event to classify (modified in-place)
    static void classifyEvent(ErrorLogEntry& entry);
    
    /// Check if event should be excluded (spam filter)
    /// @param entry  Event to check
    /// @return  true if event should be discarded
    static bool isExcludedEvent(const ErrorLogEntry& entry);
    
    /// Check if event should be hidden in UI but still stored
    /// @param entry  Event to check
    /// @return  true if event should be hidden (e.g., Event 410)
    static bool isHiddenEvent(const ErrorLogEntry& entry);
    
    // ========================================================================
    // Message Building
    // ========================================================================
    
    /// Build human-readable message from event XML
    /// @param eventId  Event ID
    /// @param provider  Provider name (for context-specific messages)
    /// @param xml  Full event XML
    /// @return  Human-readable message string
    static std::wstring buildEventMessage(int eventId, const std::wstring& provider, const std::wstring& xml);
    
    /// Extract message from Windows Event Log formatter (fallback)
    /// @param hEvent  Event handle
    /// @return  Formatted message from system or empty string
    static std::wstring extractFormattedMessage(EVT_HANDLE hEvent);
    
    // ========================================================================
    // Provider Discovery (Dynamic)
    // ========================================================================
    
    /// Discover hardware-specific providers that have errors/warnings
    /// Caches results for current session (no persistent storage)
    /// @param uliCutoff  Time cutoff for discovery scan
    /// @return  Set of provider names to query
    static std::unordered_set<std::wstring> discoverHardwareProviders(ULARGE_INTEGER uliCutoff);
    
    /// Build XPath query for dynamic providers
    /// @param providers  Set of provider names
    /// @return  XPath query string for errors/warnings only
    static std::wstring buildDynamicProviderQuery(const std::unordered_set<std::wstring>& providers);
    
    // ========================================================================
    // Session Cache (in-memory, no persistence)
    // ========================================================================
    
    static std::unordered_set<std::wstring> s_discoveredProviders;  ///< Discovered providers (cached)
    static bool s_discoveryComplete;                                 ///< true if discovery ran this session
};