#include "ErrorLogReader.h"
#include <algorithm>

// ============================================================================
// Helper Functions
// ============================================================================

static std::wstring decodeHtmlEntities(const std::wstring& encoded) {
    std::wstring decoded = encoded;
    size_t pos = 0;
    while ((pos = decoded.find(L"&amp;", pos)) != std::wstring::npos) { decoded.replace(pos, 5, L"&"); pos += 1; }
    pos = 0;
    while ((pos = decoded.find(L"&lt;", pos)) != std::wstring::npos)  { decoded.replace(pos, 4, L"<"); pos += 1; }
    pos = 0;
    while ((pos = decoded.find(L"&gt;", pos)) != std::wstring::npos)  { decoded.replace(pos, 4, L">"); pos += 1; }
    pos = 0;
    while ((pos = decoded.find(L"&quot;", pos)) != std::wstring::npos){ decoded.replace(pos, 6, L"\""); pos += 1; }
    pos = 0;
    while ((pos = decoded.find(L"&apos;", pos)) != std::wstring::npos){ decoded.replace(pos, 6, L"'"); pos += 1; }
    return decoded;
}

// Forward declarations
static ErrorLogEntry parseEvent(EVT_HANDLE hEvent);
static std::wstring buildEventMessage(int eventId, const std::wstring& xml);

// ============================================================================
// MULTI-LOG QUERY ARCHITECTURE
// ============================================================================

// Helper: Query a single event log with provider/level filters
static std::vector<ErrorLogEntry> querySingleLog(
    const std::wstring& logName,
    const std::wstring& queryXPath,
    ULARGE_INTEGER uliCutoff,
    int days)
{
    std::vector<ErrorLogEntry> entries;
    
    OutputDebugStringW((L"[ErrorLogReader] === Querying log: " + logName + L" ===\n").c_str());
    
    EVT_HANDLE hResults = EvtQuery(nullptr, logName.c_str(), queryXPath.c_str(),
                                   EvtQueryChannelPath | EvtQueryForwardDirection);
    if (!hResults) {
        DWORD error = GetLastError();
        OutputDebugStringW((L"[ErrorLogReader] Query FAILED for " + logName + 
                            L"! Error: " + std::to_wstring(error) + L"\n").c_str());
        return entries;  // Return empty - log might not exist on this system
    }

    OutputDebugStringW(L"[ErrorLogReader] Query succeeded, fetching events...\n");

    const DWORD BATCH_SIZE = 10;
    EVT_HANDLE events[BATCH_SIZE];
    DWORD returned = 0;
    int totalFetched = 0;

    while (EvtNext(hResults, BATCH_SIZE, events, INFINITE, 0, &returned)) {
        totalFetched += returned;
        for (DWORD i = 0; i < returned; i++) {
            ErrorLogEntry entry = parseEvent(events[i]);

            // DEBUG: Show lifecycle events when found
            if (entry.eventId == 400 || entry.eventId == 403 || entry.eventId == 410 ||
                entry.eventId == 430 || entry.eventId == 431) {
                OutputDebugStringW((L"[ErrorLogReader] >>> LIFECYCLE EVENT " +
                                    std::to_wstring(entry.eventId) + L" FOUND! <<<\n").c_str());
            }

            if (entry.eventId != 0) {
                // Time filtering
                auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                              entry.timestamp.time_since_epoch()).count();

                if (us <= 0) {
                    entries.push_back(entry);  // Failed parse - include conservatively
                } else {
                    ULARGE_INTEGER uliEvent;
                    uliEvent.QuadPart = static_cast<ULONGLONG>(us) * 10ULL + 116444736000000000ULL;
                    if (uliEvent.QuadPart >= uliCutoff.QuadPart)
                        entries.push_back(entry);
                }
            }
            EvtClose(events[i]);
        }
    }

    OutputDebugStringW((L"[ErrorLogReader] " + logName + L": Fetched " +
                        std::to_wstring(totalFetched) + L" total, " +
                        std::to_wstring(entries.size()) + L" within time window\n").c_str());

    EvtClose(hResults);
    return entries;
}

// ============================================================================
// Public API - Multi-Log Query
// ============================================================================

std::vector<ErrorLogEntry> ErrorLogReader::querySystemLog(int days)
{
    std::vector<ErrorLogEntry> allEntries;

    OutputDebugStringW(L"\n[ErrorLogReader] ========================================\n");
    OutputDebugStringW((L"[ErrorLogReader] Starting MULTI-LOG query (last " +
                        std::to_wstring(days) + L" days)\n").c_str());
    OutputDebugStringW(L"[ErrorLogReader] ========================================\n\n");

    // Compute time cutoff (once for all logs)
    SYSTEMTIME stNow;
    GetSystemTime(&stNow);
    FILETIME ftNow;
    SystemTimeToFileTime(&stNow, &ftNow);
    ULARGE_INTEGER uliCutoff;
    uliCutoff.LowPart  = ftNow.dwLowDateTime;
    uliCutoff.HighPart = ftNow.dwHighDateTime;
    // Add 1-day grace buffer
    uliCutoff.QuadPart -= static_cast<ULONGLONG>(days + 1) * 86400ULL * 10000000ULL;

    // ====================
    // LOG 1: System (Errors/Warnings/Critical + UserPnP Information)
    // ====================
    std::wstring systemQuery =
        L"*[System["
        L"(Provider[@Name='Microsoft-Windows-Kernel-PnP']"
        L" or Provider[@Name='Microsoft-Windows-DriverFrameworks-UserMode']"
        L" or Provider[@Name='Microsoft-Windows-UserPnP'])"
        L" and (Level=1 or Level=2 or Level=3 or Level=4)"  // All levels
        L"]]";
    
    auto systemEvents = querySingleLog(L"System", systemQuery, uliCutoff, days);
    allEntries.insert(allEntries.end(), systemEvents.begin(), systemEvents.end());

    // ====================
    // LOG 2: Configuration (Kernel-PnP Information - Lifecycle Events)
    // ====================
    std::wstring configQuery =
        L"*[System["
        L"Provider[@Name='Microsoft-Windows-Kernel-PnP']"
        L" and Level=4"  // Information only
        L"]]";
    
    auto configEvents = querySingleLog(L"Microsoft-Windows-Kernel-PnP/Configuration",
                                       configQuery, uliCutoff, days);
    allEntries.insert(allEntries.end(), configEvents.begin(), configEvents.end());

    // ====================
    // Filter out events without DeviceInstanceId
    // ====================
    // Event 20003 has no DeviceInstanceId, so filter it out
    auto beforeFilter = allEntries.size();
    allEntries.erase(
        std::remove_if(allEntries.begin(), allEntries.end(),
            [](const ErrorLogEntry& e) {
                // Keep only events that have a DeviceInstanceId
                return !e.deviceInstanceId.has_value();
            }),
        allEntries.end()
    );
    
    auto filteredCount = beforeFilter - allEntries.size();
    if (filteredCount > 0) {
        OutputDebugStringW((L"[ErrorLogReader] Filtered out " + 
                            std::to_wstring(filteredCount) + 
                            L" events without DeviceInstanceId\n").c_str());
    }

    // ====================
    // Sort by timestamp (newest first)
    // ====================
    std::sort(allEntries.begin(), allEntries.end(),
              [](const ErrorLogEntry& a, const ErrorLogEntry& b) {
                  return a.timestamp > b.timestamp;
              });

    OutputDebugStringW(L"\n[ErrorLogReader] ========================================\n");
    OutputDebugStringW((L"[ErrorLogReader] TOTAL EVENTS COLLECTED: " + 
                        std::to_wstring(allEntries.size()) + L"\n").c_str());
    OutputDebugStringW((L"[ErrorLogReader]   From System log: " + 
                        std::to_wstring(systemEvents.size()) + L"\n").c_str());
    OutputDebugStringW((L"[ErrorLogReader]   From Configuration log: " + 
                        std::to_wstring(configEvents.size()) + L"\n").c_str());
    OutputDebugStringW(L"[ErrorLogReader] ========================================\n\n");

    return allEntries;
}

// Helper: Check if two device instance IDs match (exact or parent-child relationship)
static bool deviceIdsMatch(const std::wstring& eventDeviceId, const std::wstring& driverDeviceId)
{
    // Exact match
    if (eventDeviceId == driverDeviceId) {
        OutputDebugStringW(L"[ErrorLogReader] ✓ EXACT MATCH\n");
        OutputDebugStringW((L"  ID: " + eventDeviceId + L"\n").c_str());
        return true;
    }
    
    // Case-insensitive comparison
    std::wstring eventLower = eventDeviceId;
    std::wstring driverLower = driverDeviceId;
    std::transform(eventLower.begin(), eventLower.end(), eventLower.begin(), ::towlower);
    std::transform(driverLower.begin(), driverLower.end(), driverLower.begin(), ::towlower);
    
    if (eventLower == driverLower) {
        OutputDebugStringW(L"[ErrorLogReader] ✓ CASE-INSENSITIVE MATCH\n");
        OutputDebugStringW((L"  Event:  " + eventDeviceId + L"\n").c_str());
        OutputDebugStringW((L"  Driver: " + driverDeviceId + L"\n").c_str());
        return true;
    }
    
    // Parent-child relationship
    auto extractBaseId = [](const std::wstring& id) -> std::wstring {
        size_t lastSlash = id.find_last_of(L'\\');
        if (lastSlash == std::wstring::npos)
            return id;
        return id.substr(0, lastSlash);
    };
    
    std::wstring eventBase = extractBaseId(eventLower);
    std::wstring driverBase = extractBaseId(driverLower);
    
    // Check if one is a prefix of the other (parent-child relationship)
    if (eventBase.find(driverBase) == 0 || driverBase.find(eventBase) == 0) {
        OutputDebugStringW(L"[ErrorLogReader] ✓ PREFIX MATCH (parent-child)\n");
        OutputDebugStringW((L"  Event:  " + eventDeviceId + L"\n").c_str());
        OutputDebugStringW((L"  Driver: " + driverDeviceId + L"\n").c_str());
        return true;
    }
    
    // Check vendor/product ID match for USB devices
    auto extractVidPid = [](const std::wstring& id) -> std::wstring {
        size_t vidPos = id.find(L"vid_");
        if (vidPos == std::wstring::npos)
            return L"";
        
        size_t pidPos = id.find(L"&pid_", vidPos);
        if (pidPos == std::wstring::npos)
            return L"";
        
        size_t endPos = id.find_first_of(L"&\\", pidPos + 5);
        if (endPos == std::wstring::npos)
            endPos = id.length();
        
        return id.substr(vidPos, endPos - vidPos);
    };
    
    std::wstring eventVidPid = extractVidPid(eventLower);
    std::wstring driverVidPid = extractVidPid(driverLower);
    
    // If both have VID/PID and they match, consider it a match
    if (!eventVidPid.empty() && eventVidPid == driverVidPid) {
        OutputDebugStringW(L"[ErrorLogReader] ✓ VID/PID MATCH\n");
        OutputDebugStringW((L"  Event:  " + eventDeviceId + L"\n").c_str());
        OutputDebugStringW((L"  Driver: " + driverDeviceId + L"\n").c_str());
        OutputDebugStringW((L"  VID/PID: " + eventVidPid + L"\n").c_str());
        return true;
    }
    
    return false;
}

std::vector<ErrorLogEntry> ErrorLogReader::filterForDriver(
    const std::vector<ErrorLogEntry>& allEvents,
    const std::wstring& driverInstanceId)
{
    std::vector<ErrorLogEntry> matched;
    for (const auto& event : allEvents) {
        if (event.deviceInstanceId.has_value() &&
            deviceIdsMatch(event.deviceInstanceId.value(), driverInstanceId))
            matched.push_back(event);
    }
    
    // ====================
    // DEDUPLICATION - Remove identical events
    // ====================
    // Windows sometimes logs the same event multiple times (e.g., Event 400 logged 3x)
    // Deduplicate by: EventID + Timestamp(second) + DeviceInstanceId
    
    auto beforeDedup = matched.size();
    
    // Sort by timestamp first to ensure stable deduplication
    std::sort(matched.begin(), matched.end(),
              [](const ErrorLogEntry& a, const ErrorLogEntry& b) {
                  return a.timestamp > b.timestamp;
              });
    
    // Remove duplicates using std::unique
    matched.erase(
        std::unique(matched.begin(), matched.end(),
            [](const ErrorLogEntry& a, const ErrorLogEntry& b) {
                // Two events are duplicates if they have:
                // 1. Same event ID
                // 2. Same timestamp (within same second)
                // 3. Same device instance ID
                
                if (a.eventId != b.eventId)
                    return false;
                
                if (a.deviceInstanceId != b.deviceInstanceId)
                    return false;
                
                // Compare timestamps at second precision
                auto a_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                                     a.timestamp.time_since_epoch()).count();
                auto b_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                                     b.timestamp.time_since_epoch()).count();
                
                return a_seconds == b_seconds;
            }),
        matched.end()
    );
    
    auto dedupCount = beforeDedup - matched.size();
    
    if (!matched.empty()) {
        OutputDebugStringW((L"\n[ErrorLogReader] Driver: " + driverInstanceId + L"\n").c_str());
        OutputDebugStringW((L"[ErrorLogReader] Matched " + std::to_wstring(beforeDedup) + L" events\n").c_str());
        if (dedupCount > 0) {
            OutputDebugStringW((L"[ErrorLogReader] Removed " + std::to_wstring(dedupCount) + 
                                L" duplicate events\n").c_str());
        }
        OutputDebugStringW((L"[ErrorLogReader] Final count: " + std::to_wstring(matched.size()) + L" events\n").c_str());
    }
    
    return matched;
}

void ErrorLogReader::populateErrorLogs(std::vector<DriverInfo>& drivers, int days)
{
    std::vector<ErrorLogEntry> allEvents = querySystemLog(days);
    for (auto& driver : drivers)
        driver.errorLog = filterForDriver(allEvents, driver.instanceId);
}

// ============================================================================
// Event Message Building
// ============================================================================

static std::wstring buildEventMessage(int eventId, const std::wstring& xml) {
    switch (eventId) {
        case 219: {
            auto fn = ErrorLogReader::extractEventDataField(xml, L"FailureName");
            auto st = ErrorLogReader::extractEventDataField(xml, L"Status");
            if (fn.has_value()) {
                std::wstring msg = L"Driver load failed: " + fn.value();
                if (st.has_value()) msg += L" (Status: 0x" + st.value() + L")";
                return msg;
            }
            return L"Driver load failed";
        }
        
        // Lifecycle events (Information - Level 4)
        case 400:  return L"Device installation completed";
        case 403:  return L"Driver install started";
        case 410:  return L"Device configured successfully";
        case 430:  return L"Device removal started";
        case 431:  return L"Device removal completed";
        
        // Note: Event 20001/20002 don't exist on Windows 11
        // Note: Event 20003 filtered out (no DeviceInstanceId)
        
        case 411: {
            auto st = ErrorLogReader::extractEventDataField(xml, L"Status");
            return st.has_value()
                ? L"Device removal failed (Status: 0x" + st.value() + L")"
                : L"Device removal failed";
        }
        case 420:  return L"Device requires system restart to function";
        case 442:  return L"Device was disabled";
        case 443:  return L"Device was enabled";
        
        case 1001: return L"Driver took too long to load";
        case 1002: return L"Driver initialization timed out";
        case 10000: {
            auto ex = ErrorLogReader::extractEventDataField(xml, L"ExceptionCode");
            return ex.has_value()
                ? L"Driver crashed (Exception: 0x" + ex.value() + L")"
                : L"Driver crashed";
        }
        case 10010: return L"Driver stopped responding";
        case 10100: return L"Driver framework reflector warning";
        case 2004:  return L"Kernel driver warning";
        case 10001: return L"Kernel driver error";
        default:    return L"Event " + std::to_wstring(eventId);
    }
}

// ============================================================================
// Event Parsing
// ============================================================================

static ErrorLogEntry parseEvent(EVT_HANDLE hEvent)
{
    ErrorLogEntry entry;
    DWORD bufferSize = 0, bufferUsed = 0, propertyCount = 0;

    EvtRender(nullptr, hEvent, EvtRenderEventXml,
              bufferSize, nullptr, &bufferUsed, &propertyCount);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        OutputDebugStringW(L"[ErrorLogReader] Failed to get event size\n");
        return entry;
    }

    std::vector<wchar_t> buffer(bufferUsed / sizeof(wchar_t));
    bufferSize = bufferUsed;
    if (!EvtRender(nullptr, hEvent, EvtRenderEventXml,
                   bufferSize, buffer.data(), &bufferUsed, &propertyCount)) {
        OutputDebugStringW(L"[ErrorLogReader] Failed to render event\n");
        return entry;
    }

    std::wstring xml(buffer.data());
    entry.eventId = ErrorLogReader::extractEventId(xml);
    OutputDebugStringW((L"[ErrorLogReader] Parsing Event ID: " +
                        std::to_wstring(entry.eventId) + L"\n").c_str());

    std::wstring levelNum = ErrorLogReader::extractXmlField(xml, L"Level");
    if      (levelNum == L"1") entry.level = L"Critical";
    else if (levelNum == L"2") entry.level = L"Error";
    else if (levelNum == L"3") entry.level = L"Warning";
    else if (levelNum == L"4") entry.level = L"Information";
    else                       entry.level = L"Unknown";
    OutputDebugStringW((L"[ErrorLogReader] Level: " + entry.level + L"\n").c_str());

    size_t providerPos = xml.find(L"<Provider Name='");
    if (providerPos != std::wstring::npos) {
        providerPos += 16;
        size_t endPos = xml.find(L"'", providerPos);
        if (endPos != std::wstring::npos)
            entry.provider = xml.substr(providerPos, endPos - providerPos);
    }

    entry.timestamp = ErrorLogReader::extractTimestamp(xml);
    entry.message   = buildEventMessage(entry.eventId, xml);

    entry.deviceInstanceId = ErrorLogReader::extractEventDataField(xml, L"DeviceInstanceId");
    if (!entry.deviceInstanceId.has_value())
        entry.deviceInstanceId = ErrorLogReader::extractEventDataField(xml, L"InstanceId");
    if (!entry.deviceInstanceId.has_value())
        entry.deviceInstanceId = ErrorLogReader::extractEventDataField(xml, L"TargetInstanceId");
    if (!entry.deviceInstanceId.has_value())
        entry.deviceInstanceId = ErrorLogReader::extractEventDataField(xml, L"DriverName");

    if (entry.deviceInstanceId.has_value()) {
        entry.deviceInstanceId = decodeHtmlEntities(entry.deviceInstanceId.value());
        OutputDebugStringW((L"[ErrorLogReader] Device Instance ID: " +
                            entry.deviceInstanceId.value() + L"\n").c_str());
    } else {
        OutputDebugStringW(L"[ErrorLogReader] NO Device Instance ID found!\n");
    }

    // Extract lifecycle metadata for Information events (Event 400, 20001, 20002, etc.)
    if (entry.level == L"Information") {
        // Extract driver version
        entry.driverVersion = ErrorLogReader::extractEventDataField(xml, L"DriverVersion");
        
        // Extract driver provider/manufacturer
        entry.driverProvider = ErrorLogReader::extractEventDataField(xml, L"DriverProvider");
        
        // Extract INF filename (field name varies by event)
        entry.infName = ErrorLogReader::extractEventDataField(xml, L"DriverName");
        if (!entry.infName.has_value())
            entry.infName = ErrorLogReader::extractEventDataField(xml, L"InfName");
        
        // Check if this was an update vs fresh install (Event 400)
        if (entry.eventId == 400) {
            auto deviceUpdated = ErrorLogReader::extractEventDataField(xml, L"DeviceUpdated");
            if (deviceUpdated.has_value()) {
                entry.isDriverUpdate = (deviceUpdated.value() == L"true");
            }
        }
        
        // Enhance message with version info if available
        if (entry.driverVersion.has_value() && !entry.driverVersion.value().empty()) {
            if (entry.isDriverUpdate.value_or(false)) {
                entry.message = L"Driver updated to version " + entry.driverVersion.value();
            } else if (entry.eventId == 400 || entry.eventId == 20002) {
                entry.message = L"Driver installed (version " + entry.driverVersion.value() + L")";
            }
        }
    }

    auto statusStr = ErrorLogReader::extractEventDataField(xml, L"Status");
    if (statusStr.has_value()) {
        try { entry.statusCode = std::stoul(statusStr.value(), nullptr, 10); }
        catch (...) {}
    }

    return entry;
}

// ============================================================================
// XML Parsing Helpers
// ============================================================================

std::wstring ErrorLogReader::extractXmlField(const std::wstring& xml,
                                              const std::wstring& tagName)
{
    std::wstring openTag  = L"<" + tagName;
    std::wstring closeTag = L"</" + tagName + L">";

    size_t start = xml.find(openTag);
    if (start == std::wstring::npos) return L"";
    size_t tagEnd = xml.find(L">", start);
    if (tagEnd == std::wstring::npos) return L"";
    start = tagEnd + 1;
    size_t end = xml.find(closeTag, start);
    if (end == std::wstring::npos) return L"";
    return xml.substr(start, end - start);
}

int ErrorLogReader::extractEventId(const std::wstring& xml)
{
    std::wstring idStr = extractXmlField(xml, L"EventID");
    if (idStr.empty()) return 0;
    try { return std::stoi(idStr); } catch (...) { return 0; }
}

std::chrono::system_clock::time_point ErrorLogReader::extractTimestamp(
    const std::wstring& xml)
{
    size_t pos = xml.find(L"SystemTime='");
    size_t quoteLen = 12;
    wchar_t closeQuote = L'\'';

    if (pos == std::wstring::npos) {
        pos = xml.find(L"SystemTime=\"");
        closeQuote = L'"';
        if (pos == std::wstring::npos) {
            OutputDebugStringW(L"[ErrorLogReader] Timestamp: SystemTime not found in XML\n");
            return std::chrono::system_clock::time_point{};
        }
    }

    pos += quoteLen;
    size_t end = xml.find(closeQuote, pos);
    if (end == std::wstring::npos) {
        OutputDebugStringW(L"[ErrorLogReader] Timestamp: closing quote not found\n");
        return std::chrono::system_clock::time_point{};
    }

    std::wstring timeStr = xml.substr(pos, end - pos);
    OutputDebugStringW((L"[ErrorLogReader] Timestamp: " + timeStr + L"\n").c_str());

    SYSTEMTIME st = {};
    int parsed = swscanf_s(timeStr.c_str(), L"%4hu-%2hu-%2huT%2hu:%2hu:%2hu",
                           &st.wYear, &st.wMonth, &st.wDay,
                           &st.wHour, &st.wMinute, &st.wSecond);
    if (parsed < 6) {
        OutputDebugStringW((L"[ErrorLogReader] Timestamp parse failed (" +
                            std::to_wstring(parsed) + L" fields): " + timeStr + L"\n").c_str());
        return std::chrono::system_clock::time_point{};
    }

    FILETIME ft;
    if (!SystemTimeToFileTime(&st, &ft)) {
        OutputDebugStringW(L"[ErrorLogReader] SystemTimeToFileTime failed\n");
        return std::chrono::system_clock::time_point{};
    }

    ULARGE_INTEGER ull;
    ull.LowPart  = ft.dwLowDateTime;
    ull.HighPart = ft.dwHighDateTime;

    auto duration = std::chrono::microseconds(
        (ull.QuadPart - 116444736000000000ULL) / 10);
    return std::chrono::system_clock::time_point(duration);
}

std::optional<std::wstring> ErrorLogReader::extractEventDataField(
    const std::wstring& xml,
    const std::wstring& fieldName)
{
    std::wstring pattern = L"<Data Name='" + fieldName + L"'>";
    size_t start = xml.find(pattern);
    if (start == std::wstring::npos) {
        pattern = L"<Data Name=\"" + fieldName + L"\">";
        start = xml.find(pattern);
    }
    if (start == std::wstring::npos) return std::nullopt;

    start += pattern.length();
    size_t end = xml.find(L"</Data>", start);
    if (end == std::wstring::npos) return std::nullopt;

    std::wstring value = xml.substr(start, end - start);
    if (value.empty()) return std::nullopt;
    return value;
}