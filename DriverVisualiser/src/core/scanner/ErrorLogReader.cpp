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

// ============================================================================
// Public API
// ============================================================================

std::vector<ErrorLogEntry> ErrorLogReader::querySystemLog(int days)
{
    std::vector<ErrorLogEntry> entries;

    OutputDebugStringW(L"[ErrorLogReader] Starting query...\n");

    // NOTE: XPath time filtering (timediff / @SystemTime comparisons) is
    // unreliable across Windows versions. We filter by timestamp in C++ instead.
    // Compute cutoff as FILETIME (100ns since 1601-01-01 UTC).
    SYSTEMTIME stNow;
    GetSystemTime(&stNow);
    FILETIME ftNow;
    SystemTimeToFileTime(&stNow, &ftNow);
    ULARGE_INTEGER uliCutoff;
    uliCutoff.LowPart  = ftNow.dwLowDateTime;
    uliCutoff.HighPart = ftNow.dwHighDateTime;
    // Add 1-day grace buffer so events from the boundary day are always included.
    // Without this, events from early morning on day-7 get excluded if the scan
    // runs in the evening (cutoff = scan_time - 7d, which is mid-day 7 days ago).
    uliCutoff.QuadPart -= static_cast<ULONGLONG>(days + 1) * 86400ULL * 10000000ULL;

    OutputDebugStringW((L"[ErrorLogReader] Filtering last " +
                        std::to_wstring(days) + L" days in C++\n").c_str());

    // Query by provider + level only (time filter handled in C++ below)
    std::wstring query =
        L"*[System["
        L"(Provider[@Name='Microsoft-Windows-Kernel-PnP']"
        L" or Provider[@Name='Microsoft-Windows-DriverFrameworks-UserMode'])"
        L" and (Level=1 or Level=2 or Level=3)"
        L"]]";

    EVT_HANDLE hResults = EvtQuery(nullptr, L"System", query.c_str(),
                                   EvtQueryChannelPath | EvtQueryForwardDirection);
    if (!hResults) {
        OutputDebugStringW((L"[ErrorLogReader] Query FAILED! Error: " +
                            std::to_wstring(GetLastError()) + L"\n").c_str());
        return entries;
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

            if (entry.eventId != 0) {
                // Convert entry.timestamp back to FILETIME and compare with cutoff.
                // extractTimestamp() stores microseconds since Unix epoch (1970).
                // Inverse: FILETIME = (us * 10) + 116444736000000000
                // If timestamp is at epoch (us <= 0), parsing failed - include conservatively.
                auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                              entry.timestamp.time_since_epoch()).count();

                if (us <= 0) {
                    entries.push_back(entry);  // failed parse: include
                } else {
                    ULARGE_INTEGER uliEvent;
                    uliEvent.QuadPart = static_cast<ULONGLONG>(us) * 10ULL
                                        + 116444736000000000ULL;
                    if (uliEvent.QuadPart >= uliCutoff.QuadPart)
                        entries.push_back(entry);
                }
            }
            EvtClose(events[i]);
        }
    }

    OutputDebugStringW((L"[ErrorLogReader] Total fetched from log: " +
                        std::to_wstring(totalFetched) + L"\n").c_str());
    OutputDebugStringW((L"[ErrorLogReader] Events within " + std::to_wstring(days) +
                        L"-day window: " + std::to_wstring(entries.size()) + L"\n").c_str());

    EvtClose(hResults);
    return entries;
}

std::vector<ErrorLogEntry> ErrorLogReader::filterForDriver(
    const std::vector<ErrorLogEntry>& allEvents,
    const std::wstring& driverInstanceId)
{
    std::vector<ErrorLogEntry> matched;
    for (const auto& event : allEvents) {
        if (event.deviceInstanceId.has_value() &&
            event.deviceInstanceId.value() == driverInstanceId)
            matched.push_back(event);
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
        case 400:  return L"Device installation completed";
        case 410:  return L"Device configured successfully";
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

ErrorLogEntry ErrorLogReader::parseEvent(EVT_HANDLE hEvent)
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
    entry.eventId = extractEventId(xml);
    OutputDebugStringW((L"[ErrorLogReader] Parsing Event ID: " +
                        std::to_wstring(entry.eventId) + L"\n").c_str());

    std::wstring levelNum = extractXmlField(xml, L"Level");
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

    entry.timestamp = extractTimestamp(xml);
    entry.message   = buildEventMessage(entry.eventId, xml);

    entry.deviceInstanceId = extractEventDataField(xml, L"DeviceInstanceId");
    if (!entry.deviceInstanceId.has_value())
        entry.deviceInstanceId = extractEventDataField(xml, L"InstanceId");
    if (!entry.deviceInstanceId.has_value())
        entry.deviceInstanceId = extractEventDataField(xml, L"TargetInstanceId");
    if (!entry.deviceInstanceId.has_value())
        entry.deviceInstanceId = extractEventDataField(xml, L"DriverName");

    if (entry.deviceInstanceId.has_value()) {
        entry.deviceInstanceId = decodeHtmlEntities(entry.deviceInstanceId.value());
        OutputDebugStringW((L"[ErrorLogReader] Device Instance ID: " +
                            entry.deviceInstanceId.value() + L"\n").c_str());
    } else {
        OutputDebugStringW(L"[ErrorLogReader] NO Device Instance ID found!\n");
    }

    auto statusStr = extractEventDataField(xml, L"Status");
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
    // Windows event XML uses SINGLE quotes for the SystemTime attribute:
    //   <TimeCreated SystemTime='2026-02-09T14:32:15.1234567Z'/>
    // We must search for SystemTime=' (with single quote), not SystemTime="

    size_t pos = xml.find(L"SystemTime='");
    size_t quoteLen = 12;  // length of "SystemTime='"
    wchar_t closeQuote = L'\'';

    if (pos == std::wstring::npos) {
        // Fallback: try double quotes
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

    // Parse: 2026-02-09T14:32:15.1234567Z
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

    // FILETIME to microseconds since Unix epoch (1970-01-01)
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