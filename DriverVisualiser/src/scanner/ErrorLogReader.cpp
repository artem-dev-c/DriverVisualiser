#include "ErrorLogReader.h"
#include <algorithm>

// ============================================================================
// Public API
// ============================================================================

std::vector<ErrorLogEntry> ErrorLogReader::querySystemLog()
{
    std::vector<ErrorLogEntry> entries;
    
    // XPath query: Get all driver-related events from last 7 days
    // Providers: Kernel-PnP, DriverFrameworks-UserMode, DriverFrameworks-KernelMode
    // Levels: 1=Critical, 2=Error, 3=Warning
    // Time: Last 604800000 milliseconds (7 days)
    std::wstring query = LR"(
        *[System[
            (Provider[@Name='Microsoft-Windows-Kernel-PnP']
             or Provider[@Name='Microsoft-Windows-DriverFrameworks-UserMode']
             or Provider[@Name='Microsoft-Windows-DriverFrameworks-KernelMode'])
            and
            (Level=1 or Level=2 or Level=3)
            and
            TimeCreated[timediff(@SystemTime) <= 604800000]
        ]]
    )";
    
    // Execute query on System channel
    EVT_HANDLE hResults = EvtQuery(
        nullptr,                // Local machine
        L"System",              // Channel name
        query.c_str(),          // XPath query
        EvtQueryChannelPath | EvtQueryForwardDirection
    );
    
    if (!hResults) {
        // Query failed - return empty list
        return entries;
    }
    
    // Fetch events in batches
    const DWORD BATCH_SIZE = 10;
    EVT_HANDLE events[BATCH_SIZE];
    DWORD returned = 0;
    
    while (EvtNext(hResults, BATCH_SIZE, events, INFINITE, 0, &returned)) {
        for (DWORD i = 0; i < returned; i++) {
            ErrorLogEntry entry = parseEvent(events[i]);
            
            // Only add if we successfully parsed it
            if (entry.eventId != 0) {
                entries.push_back(entry);
            }
            
            EvtClose(events[i]);
        }
    }
    
    EvtClose(hResults);
    return entries;
}

std::vector<ErrorLogEntry> ErrorLogReader::filterForDriver(
    const std::vector<ErrorLogEntry>& allEvents,
    const std::wstring& driverInstanceId)
{
    std::vector<ErrorLogEntry> matched;
    
    for (const auto& event : allEvents) {
        // Instance ID exact match only (conservative approach)
        if (event.deviceInstanceId.has_value() &&
            event.deviceInstanceId.value() == driverInstanceId) {
            matched.push_back(event);
        }
    }
    
    return matched;
}

void ErrorLogReader::populateErrorLogs(std::vector<DriverInfo>& drivers)
{
    // Query all events once
    std::vector<ErrorLogEntry> allEvents = querySystemLog();
    
    // Match events to each driver
    for (auto& driver : drivers) {
        driver.errorLog = filterForDriver(allEvents, driver.instanceId);
    }
}

// ============================================================================
// Event Parsing
// ============================================================================

ErrorLogEntry ErrorLogReader::parseEvent(EVT_HANDLE hEvent)
{
    ErrorLogEntry entry;
    
    // Get event as XML string
    DWORD bufferSize = 0;
    DWORD bufferUsed = 0;
    DWORD propertyCount = 0;
    
    // First call to get required buffer size
    EvtRender(nullptr, hEvent, EvtRenderEventXml, 
              bufferSize, nullptr, &bufferUsed, &propertyCount);
    
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return entry;  // Failed to get size
    }
    
    // Allocate buffer
    std::vector<wchar_t> buffer(bufferUsed / sizeof(wchar_t));
    bufferSize = bufferUsed;
    
    // Second call to get actual data
    if (!EvtRender(nullptr, hEvent, EvtRenderEventXml,
                   bufferSize, buffer.data(), &bufferUsed, &propertyCount)) {
        return entry;  // Failed to render
    }
    
    std::wstring xml(buffer.data());
    
    // Parse XML fields
    entry.eventId = extractEventId(xml);
    entry.level = extractXmlField(xml, L"Level");
    entry.provider = extractXmlField(xml, L"Provider");
    entry.timestamp = extractTimestamp(xml);
    
    // Get user-friendly message (full event text)
    entry.message = extractXmlField(xml, L"RenderingInfo");
    if (entry.message.empty()) {
        entry.message = L"Event " + std::to_wstring(entry.eventId);
    }
    
    // Extract EventData fields
    entry.deviceInstanceId = extractEventDataField(xml, L"DeviceInstanceId");
    if (!entry.deviceInstanceId.has_value()) {
        // Try alternate field names
        entry.deviceInstanceId = extractEventDataField(xml, L"InstanceId");
    }
    if (!entry.deviceInstanceId.has_value()) {
        entry.deviceInstanceId = extractEventDataField(xml, L"TargetInstanceId");
    }
    
    // Try to extract status code
    auto statusStr = extractEventDataField(xml, L"Status");
    if (statusStr.has_value()) {
        try {
            entry.statusCode = std::stoul(statusStr.value(), nullptr, 16);
        } catch (...) {
            // Failed to parse, leave as nullopt
        }
    }
    
    return entry;
}

// ============================================================================
// XML Parsing Helpers
// ============================================================================

std::wstring ErrorLogReader::extractXmlField(const std::wstring& xml, const std::wstring& tagName)
{
    std::wstring openTag = L"<" + tagName;
    std::wstring closeTag = L"</" + tagName + L">";
    
    size_t start = xml.find(openTag);
    if (start == std::wstring::npos) {
        return L"";
    }
    
    // Find end of opening tag (could have attributes)
    size_t tagEnd = xml.find(L">", start);
    if (tagEnd == std::wstring::npos) {
        return L"";
    }
    
    start = tagEnd + 1;
    size_t end = xml.find(closeTag, start);
    if (end == std::wstring::npos) {
        return L"";
    }
    
    return xml.substr(start, end - start);
}

int ErrorLogReader::extractEventId(const std::wstring& xml)
{
    std::wstring idStr = extractXmlField(xml, L"EventID");
    if (idStr.empty()) {
        return 0;
    }
    
    try {
        return std::stoi(idStr);
    } catch (...) {
        return 0;
    }
}

std::chrono::system_clock::time_point ErrorLogReader::extractTimestamp(const std::wstring& xml)
{
    // Find TimeCreated SystemTime attribute
    // Format: <TimeCreated SystemTime="2026-02-09T14:32:15.123Z"/>
    size_t pos = xml.find(L"SystemTime=\"");
    if (pos == std::wstring::npos) {
        return std::chrono::system_clock::now();
    }
    
    pos += 12;  // Skip 'SystemTime="'
    size_t end = xml.find(L"\"", pos);
    if (end == std::wstring::npos) {
        return std::chrono::system_clock::now();
    }
    
    std::wstring timeStr = xml.substr(pos, end - pos);
    
    // Parse ISO 8601 timestamp (basic parsing, good enough for now)
    // Format: 2026-02-09T14:32:15.123Z
    SYSTEMTIME st = {};
    swscanf_s(timeStr.c_str(), L"%4hu-%2hu-%2huT%2hu:%2hu:%2hu",
              &st.wYear, &st.wMonth, &st.wDay,
              &st.wHour, &st.wMinute, &st.wSecond);
    
    FILETIME ft;
    if (SystemTimeToFileTime(&st, &ft)) {
        ULARGE_INTEGER ull;
        ull.LowPart = ft.dwLowDateTime;
        ull.HighPart = ft.dwHighDateTime;
        
        // Convert FILETIME (100-nanosecond intervals since 1601) to time_point
        auto duration = std::chrono::microseconds((ull.QuadPart - 116444736000000000ULL) / 10);
        return std::chrono::system_clock::time_point(duration);
    }
    
    return std::chrono::system_clock::now();
}

std::optional<std::wstring> ErrorLogReader::extractEventDataField(
    const std::wstring& xml, 
    const std::wstring& fieldName)
{
    // Find: <Data Name="DeviceInstanceId">PCI\VEN_...</Data>
    std::wstring searchPattern = L"<Data Name=\"" + fieldName + L"\">";
    size_t start = xml.find(searchPattern);
    if (start == std::wstring::npos) {
        return std::nullopt;
    }
    
    start += searchPattern.length();
    size_t end = xml.find(L"</Data>", start);
    if (end == std::wstring::npos) {
        return std::nullopt;
    }
    
    std::wstring value = xml.substr(start, end - start);
    if (value.empty()) {
        return std::nullopt;
    }
    
    return value;
}