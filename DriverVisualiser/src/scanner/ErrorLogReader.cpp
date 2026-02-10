#include "ErrorLogReader.h"
#include <algorithm>

// ============================================================================
// Helper Functions
// ============================================================================

/// Decode HTML entities in XML (&amp; -> &, &lt; -> <, etc.)
static std::wstring decodeHtmlEntities(const std::wstring& encoded) {
    std::wstring decoded = encoded;
    
    // Replace common HTML entities
    size_t pos = 0;
    while ((pos = decoded.find(L"&amp;", pos)) != std::wstring::npos) {
        decoded.replace(pos, 5, L"&");
        pos += 1;
    }
    
    pos = 0;
    while ((pos = decoded.find(L"&lt;", pos)) != std::wstring::npos) {
        decoded.replace(pos, 4, L"<");
        pos += 1;
    }
    
    pos = 0;
    while ((pos = decoded.find(L"&gt;", pos)) != std::wstring::npos) {
        decoded.replace(pos, 4, L">");
        pos += 1;
    }
    
    pos = 0;
    while ((pos = decoded.find(L"&quot;", pos)) != std::wstring::npos) {
        decoded.replace(pos, 6, L"\"");
        pos += 1;
    }
    
    pos = 0;
    while ((pos = decoded.find(L"&apos;", pos)) != std::wstring::npos) {
        decoded.replace(pos, 6, L"'");
        pos += 1;
    }
    
    return decoded;
}

// ============================================================================
// Public API
// ============================================================================

std::vector<ErrorLogEntry> ErrorLogReader::querySystemLog()
{
    std::vector<ErrorLogEntry> entries;
    
    OutputDebugStringW(L"[ErrorLogReader] Starting query...\n");
    
    // XPath query: Get all driver-related events from last 7 days
    // Providers: Kernel-PnP (core), DriverFrameworks-UserMode (common)
    // Note: DriverFrameworks-KernelMode doesn't exist on all systems, removed
    // Levels: 1=Critical, 2=Error, 3=Warning
    // Time: Last 604800000 milliseconds (7 days)
    std::wstring query = LR"(
        *[System[
            (Provider[@Name='Microsoft-Windows-Kernel-PnP']
             or Provider[@Name='Microsoft-Windows-DriverFrameworks-UserMode'])
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
        DWORD error = GetLastError();
        OutputDebugStringW((L"[ErrorLogReader] Query FAILED! Error: " + std::to_wstring(error) + L"\n").c_str());
        return entries;
    }
    
    OutputDebugStringW(L"[ErrorLogReader] Query succeeded, fetching events...\n");
    
    // Fetch events in batches
    const DWORD BATCH_SIZE = 10;
    EVT_HANDLE events[BATCH_SIZE];
    DWORD returned = 0;
    
    int totalEvents = 0;
    while (EvtNext(hResults, BATCH_SIZE, events, INFINITE, 0, &returned)) {
        totalEvents += returned;
        OutputDebugStringW((L"[ErrorLogReader] Got batch of " + std::to_wstring(returned) + L" events\n").c_str());
        
        for (DWORD i = 0; i < returned; i++) {
            ErrorLogEntry entry = parseEvent(events[i]);
            
            // Only add if we successfully parsed it
            if (entry.eventId != 0) {
                entries.push_back(entry);
            }
            
            EvtClose(events[i]);
        }
    }
    
    OutputDebugStringW((L"[ErrorLogReader] Total events retrieved: " + std::to_wstring(totalEvents) + L"\n").c_str());
    OutputDebugStringW((L"[ErrorLogReader] Total events parsed successfully: " + std::to_wstring(entries.size()) + L"\n").c_str());
    
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
// Event Message Building
// ============================================================================

/// Build human-readable message for common driver event IDs
static std::wstring buildEventMessage(int eventId, const std::wstring& xml) {
    switch (eventId) {
        // === Kernel-PnP Events ===
        
        case 219: {  // Driver load failure
            auto failureName = ErrorLogReader::extractEventDataField(xml, L"FailureName");
            auto status = ErrorLogReader::extractEventDataField(xml, L"Status");
            
            if (failureName.has_value()) {
                std::wstring msg = L"Driver load failed: " + failureName.value();
                if (status.has_value()) {
                    msg += L" (Status: 0x" + status.value() + L")";
                }
                return msg;
            }
            return L"Driver load failed";
        }
        
        case 400:    // Device installation
            return L"Device installation completed";
        
        case 410:    // Device configured
            return L"Device configured successfully";
        
        case 411: {  // Device removal failed
            auto status = ErrorLogReader::extractEventDataField(xml, L"Status");
            if (status.has_value()) {
                return L"Device removal failed (Status: 0x" + status.value() + L")";
            }
            return L"Device removal failed";
        }
        
        case 420:    // Device restart required
            return L"Device requires system restart to function";
        
        case 442:    // Device disabled
            return L"Device was disabled";
        
        case 443:    // Device enabled
            return L"Device was enabled";
        
        // === DriverFrameworks-UserMode Events ===
        
        case 1001:   // UMDF driver slow load
            return L"Driver took too long to load";
        
        case 1002:   // UMDF driver slow initialization
            return L"Driver initialization timed out";
        
        case 10000: {  // UMDF driver crashed
            auto exception = ErrorLogReader::extractEventDataField(xml, L"ExceptionCode");
            if (exception.has_value()) {
                return L"Driver crashed (Exception: 0x" + exception.value() + L")";
            }
            return L"Driver crashed";
        }
        
        case 10010:  // UMDF driver timeout
            return L"Driver stopped responding";
        
        case 10100:  // UMDF reflector warning
            return L"Driver framework reflector warning";
        
        // === DriverFrameworks-KernelMode Events ===
        
        case 2004:   // KMDF driver warning
            return L"Kernel driver warning";
        
        case 10001:  // KMDF driver error
            return L"Kernel driver error";
        
        // === Generic fallback ===
        default:
            return L"Event " + std::to_wstring(eventId);
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
        OutputDebugStringW(L"[ErrorLogReader] Failed to get event size\n");
        return entry;  // Failed to get size
    }
    
    // Allocate buffer
    std::vector<wchar_t> buffer(bufferUsed / sizeof(wchar_t));
    bufferSize = bufferUsed;
    
    // Second call to get actual data
    if (!EvtRender(nullptr, hEvent, EvtRenderEventXml,
                   bufferSize, buffer.data(), &bufferUsed, &propertyCount)) {
        OutputDebugStringW(L"[ErrorLogReader] Failed to render event\n");
        return entry;  // Failed to render
    }
    
    std::wstring xml(buffer.data());
    
    // Parse XML fields
    entry.eventId = extractEventId(xml);
    
    OutputDebugStringW((L"[ErrorLogReader] Parsing Event ID: " + std::to_wstring(entry.eventId) + L"\n").c_str());
    
    // Convert Level number to text
    std::wstring levelNum = extractXmlField(xml, L"Level");
    if (levelNum == L"1") entry.level = L"Critical";
    else if (levelNum == L"2") entry.level = L"Error";
    else if (levelNum == L"3") entry.level = L"Warning";
    else if (levelNum == L"4") entry.level = L"Information";
    else entry.level = L"Unknown";
    
    OutputDebugStringW((L"[ErrorLogReader] Level: " + entry.level + L"\n").c_str());
    
    // Extract provider name from Provider element (Name attribute)
    size_t providerPos = xml.find(L"<Provider Name='");
    if (providerPos != std::wstring::npos) {
        providerPos += 16; // Length of "<Provider Name='"
        size_t endPos = xml.find(L"'", providerPos);
        if (endPos != std::wstring::npos) {
            entry.provider = xml.substr(providerPos, endPos - providerPos);
        }
    }
    
    entry.timestamp = extractTimestamp(xml);
    
    // Build user-friendly message from EventData using event-specific mapping
    entry.message = buildEventMessage(entry.eventId, xml);
    
    // Extract EventData fields - try multiple field name variations
    // Different event types use different field names for the device identifier
    entry.deviceInstanceId = extractEventDataField(xml, L"DeviceInstanceId");
    if (!entry.deviceInstanceId.has_value()) {
        entry.deviceInstanceId = extractEventDataField(xml, L"InstanceId");
    }
    if (!entry.deviceInstanceId.has_value()) {
        entry.deviceInstanceId = extractEventDataField(xml, L"TargetInstanceId");
    }
    if (!entry.deviceInstanceId.has_value()) {
        // Event 219 uses "DriverName" field for the device Instance ID
        entry.deviceInstanceId = extractEventDataField(xml, L"DriverName");
    }
    
    // Decode HTML entities (&amp; -> &, etc.)
    if (entry.deviceInstanceId.has_value()) {
        entry.deviceInstanceId = decodeHtmlEntities(entry.deviceInstanceId.value());
        OutputDebugStringW((L"[ErrorLogReader] Device Instance ID: " + entry.deviceInstanceId.value() + L"\n").c_str());
    } else {
        OutputDebugStringW(L"[ErrorLogReader] NO Device Instance ID found!\n");
    }
    
    // Try to extract status code
    auto statusStr = extractEventDataField(xml, L"Status");
    if (statusStr.has_value()) {
        try {
            // Status is stored as decimal in XML, not hex
            entry.statusCode = std::stoul(statusStr.value(), nullptr, 10);
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
    // Windows Event XML uses single quotes ('), not double quotes (")
    // Find: <Data Name='DeviceInstanceId'>PCI\VEN_...</Data>
    std::wstring searchPattern = L"<Data Name='" + fieldName + L"'>";
    size_t start = xml.find(searchPattern);
    
    // If not found with single quotes, try double quotes (just in case)
    if (start == std::wstring::npos) {
        searchPattern = L"<Data Name=\"" + fieldName + L"\">";
        start = xml.find(searchPattern);
    }
    
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