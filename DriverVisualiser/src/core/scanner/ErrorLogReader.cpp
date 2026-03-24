#include "ErrorLogReader.h"
#include <algorithm>

// ============================================================================
// Static Member Initialization
// ============================================================================

std::unordered_set<std::wstring> ErrorLogReader::s_discoveredProviders;
bool ErrorLogReader::s_discoveryComplete = false;

// ============================================================================
// Core Provider Whitelist (Always Queried)
// ============================================================================

namespace {
    /// Core Windows driver providers - always queried for lifecycle events
    const std::vector<std::wstring> CORE_PROVIDERS = {
        L"Microsoft-Windows-Kernel-PnP",
        L"Microsoft-Windows-DriverFrameworks-UserMode",
        L"Microsoft-Windows-UserPnP"
    };
    
    /// Hardware providers to query (errors/warnings only)
    /// Covers ~90% of consumer hardware - no dynamic discovery needed
    const std::vector<std::wstring> HARDWARE_PROVIDERS = {
        // GPU drivers
        L"nvlddmkm",           // NVIDIA GPU
        L"amdkmdag",           // AMD GPU  
        L"atikmdag",           // AMD GPU (older)
        L"igfx",               // Intel GPU (older)
        L"igd10iumd64",        // Intel GPU (newer)
        L"igdkmd64",           // Intel GPU (alternate)
        
        // Storage drivers
        L"disk",               // Generic disk
        L"storahci",           // SATA AHCI
        L"stornvme",           // NVMe
        L"iaStorAC",           // Intel storage
        
        // Network drivers - Intel WiFi (multiple versions)
        L"Netwtw10",           // Intel WiFi 5
        L"Netwtw14",           // Intel WiFi 6/6E
        L"Netwtw16",           // Intel WiFi 7 (future-proofing)
        L"e1i65x64",           // Intel Ethernet
        
        // Network drivers - Realtek
        L"rt640x64",           // Realtek Ethernet
        L"rtwlane",            // Realtek WiFi 802.11ac
        L"rtux64w10",          // Realtek USB WiFi
        L"RTKVHD64",           // Realtek Audio
        
        // Network drivers - Broadcom
        L"bcmwl63a",           // Broadcom WiFi (common in Dell/HP)
        
        // AMD chipset
        L"amdgpio2",           // AMD GPIO controller
        L"amdi2c",             // AMD I2C controller
        
        // USB/Chipset
        L"USBXHCI",            // USB 3.0
        L"MEIx64",             // Intel Management Engine
        
        // VirtualBox (if installed)
        L"VBoxNetLwf",
        L"VBoxUSBMon"
    };
    
    // ========================================================================
    // Event ID Whitelists & Blacklists
    // ========================================================================
    
    /// Lifecycle events to capture (Information level from Kernel-PnP)
    const std::unordered_set<int> LIFECYCLE_EVENT_IDS = {
        400,   // Device installed/updated (VISIBLE)
        // 410 - Device configured (EXCLUDED - 1:1 duplicate of Event 400, adds no value)
        420,   // Restart required (VISIBLE)
        430,   // Device removal started (VISIBLE)
        431    // Device removal complete (VISIBLE)
    };
    
    /// Events to completely exclude (spam/noise)
    struct ExcludedEvent {
        int eventId;
        std::wstring provider;  // Empty = any provider
    };
    
    const std::vector<ExcludedEvent> EXCLUDED_EVENTS = {
        // PROVEN spam (confirmed useless from PowerShell analysis):
        { 6,    L"Microsoft-Windows-FilterManager" },       // Filter loaded successfully - boot spam
        { 6062, L"Netwtw14" },                              // LSO (Large Send Offload) triggered - performance feature, not an error
        { 7021, L"Netwtw14" },                              // WiFi "Connection telemetry" - confirmed telemetry
        { 11,   L"Microsoft-Windows-Kernel-General" },      // Registry TxR - not driver-related
        { 16,   L"Microsoft-Windows-Kernel-General" },      // Registry hive access - system maintenance
        { 44,   L"Microsoft-Windows-WindowsUpdateClient" }, // Update download started - not driver-specific  
        { 153,  L"Microsoft-Windows-Kernel-Boot" }          // VBS status - boot info, not driver health
    };
    
    // ========================================================================
    // Event ID Whitelists & Blacklists
    // ========================================================================
}
// Forward declaration
static std::wstring buildHardwareProviderQuery(const std::vector<std::wstring>& providers);

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
// Public API - Multi-Log Query with Dynamic Discovery
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
    uliCutoff.QuadPart -= static_cast<ULONGLONG>(days + 1) * 86400ULL * 10000000ULL;

    // ========================================================================
    // QUERY 1: System Log - Core Providers (Errors/Warnings + Lifecycle)
    // ========================================================================
    
    OutputDebugStringW(L"[ErrorLogReader] === QUERY 1: System Log (Core Providers) ===\n");
    
    std::wstring coreQuery =
        L"*[System["
        L"  (Provider[@Name='Microsoft-Windows-Kernel-PnP']"
        L"   or Provider[@Name='Microsoft-Windows-DriverFrameworks-UserMode']"
        L"   or Provider[@Name='Microsoft-Windows-UserPnP'])"
        L"  and ("
        L"    (Level=1 or Level=2 or Level=3)"  // All errors/warnings
        L"    or (Level=4 and ("  // Information - lifecycle events only
        L"      EventID=400"   // Device installed/updated
        L"      or EventID=410"  // Device configured (stored, hidden)
        L"      or EventID=420"  // Restart required
        L"      or EventID=430"  // Device removal started
        L"      or EventID=431"  // Device removal complete
        L"    ))"
        L"  )"
        L"]]";
    
    auto coreEvents = querySingleLog(L"System", coreQuery, uliCutoff, days);
    allEntries.insert(allEntries.end(), coreEvents.begin(), coreEvents.end());

    // ========================================================================
    // QUERY 2: Configuration Log - Kernel-PnP Lifecycle Events
    // ========================================================================
    
    OutputDebugStringW(L"[ErrorLogReader] === QUERY 2: Configuration Log (Lifecycle) ===\n");
    
    std::wstring configQuery =
        L"*[System["
        L"  Provider[@Name='Microsoft-Windows-Kernel-PnP']"
        L"  and Level=4"  // Information only
        L"  and ("
        L"    EventID=400 or EventID=410 or EventID=420"
        L"    or EventID=430 or EventID=431"
        L"  )"
        L"]]";
    
    auto configEvents = querySingleLog(L"Microsoft-Windows-Kernel-PnP/Configuration",
                                       configQuery, uliCutoff, days);
    allEntries.insert(allEntries.end(), configEvents.begin(), configEvents.end());

    // ========================================================================
    // QUERY 3: Hardware Providers (Errors/Warnings Only)
    // ========================================================================
    
    OutputDebugStringW(L"[ErrorLogReader] === QUERY 3: Hardware Providers ===\n");
    
    if (!HARDWARE_PROVIDERS.empty()) {
        // Split providers into batches of 10 to avoid XPath query length limits
        const size_t BATCH_SIZE = 10;
        size_t totalProviders = HARDWARE_PROVIDERS.size();
        
        OutputDebugStringW((L"[ErrorLogReader] Querying " + 
                            std::to_wstring(totalProviders) + 
                            L" hardware providers for errors/warnings\n").c_str());
        
        for (size_t i = 0; i < totalProviders; i += BATCH_SIZE) {
            size_t end = std::min(i + BATCH_SIZE, totalProviders);
            std::vector<std::wstring> batch(
                HARDWARE_PROVIDERS.begin() + i,
                HARDWARE_PROVIDERS.begin() + end
            );
            
            std::wstring hwQuery = buildHardwareProviderQuery(batch);
            auto hwEvents = querySingleLog(L"System", hwQuery, uliCutoff, days);
            allEntries.insert(allEntries.end(), hwEvents.begin(), hwEvents.end());
        }
    }

    // ========================================================================
    // Post-Processing: Remove Duplicates, Classify, Filter
    // ========================================================================
    
    OutputDebugStringW(L"\n[ErrorLogReader] === Post-Processing ===\n");
    
    auto beforeDedup = allEntries.size();
    
    // Remove duplicates (same event from System + Configuration logs)
    std::sort(allEntries.begin(), allEntries.end(),
              [](const ErrorLogEntry& a, const ErrorLogEntry& b) {
                  if (a.timestamp != b.timestamp) return a.timestamp < b.timestamp;
                  if (a.eventId != b.eventId) return a.eventId < b.eventId;
                  return a.provider < b.provider;
              });
    
    allEntries.erase(
        std::unique(allEntries.begin(), allEntries.end(),
                    [](const ErrorLogEntry& a, const ErrorLogEntry& b) {
                        return a.timestamp == b.timestamp &&
                               a.eventId == b.eventId &&
                               a.provider == b.provider;
                    }),
        allEntries.end()
    );
    
    auto afterDedup = allEntries.size();
    if (beforeDedup != afterDedup) {
        OutputDebugStringW((L"[ErrorLogReader] Removed " + 
                            std::to_wstring(beforeDedup - afterDedup) + 
                            L" duplicate events\n").c_str());
    }
    
    // Classify events (DeviceMapped/SystemWide/SwdBackground)
    int deviceMappedCount = 0;
    int systemWideCount = 0;
    int swdCount = 0;
    
    for (auto& entry : allEntries) {
        classifyEvent(entry);
        
        // Count by category
        if (entry.category == ErrorLogEntry::Category::DeviceMapped) deviceMappedCount++;
        else if (entry.category == ErrorLogEntry::Category::SystemWide) systemWideCount++;
        else if (entry.category == ErrorLogEntry::Category::SwdBackground) swdCount++;
    }
    
    OutputDebugStringW((L"[ErrorLogReader] Classification: DeviceMapped=" + 
                        std::to_wstring(deviceMappedCount) + 
                        L", SystemWide=" + std::to_wstring(systemWideCount) +
                        L", SWD=" + std::to_wstring(swdCount) + L"\n").c_str());
    
    // Remove excluded events (spam filter)
    auto beforeFilter = allEntries.size();
    allEntries.erase(
        std::remove_if(allEntries.begin(), allEntries.end(),
                      [](const ErrorLogEntry& e) { return isExcludedEvent(e); }),
        allEntries.end()
    );
    
    auto afterFilter = allEntries.size();
    if (beforeFilter != afterFilter) {
        OutputDebugStringW((L"[ErrorLogReader] Filtered out " + 
                            std::to_wstring(beforeFilter - afterFilter) + 
                            L" spam events\n").c_str());
    }

    // Sort by timestamp (newest first)
    std::sort(allEntries.begin(), allEntries.end(),
              [](const ErrorLogEntry& a, const ErrorLogEntry& b) {
                  return a.timestamp > b.timestamp;
              });

    OutputDebugStringW(L"\n[ErrorLogReader] ========================================\n");
    OutputDebugStringW((L"[ErrorLogReader] TOTAL EVENTS COLLECTED: " + 
                        std::to_wstring(allEntries.size()) + L"\n").c_str());
    OutputDebugStringW((L"[ErrorLogReader]   From System (core): " + 
                        std::to_wstring(coreEvents.size()) + L"\n").c_str());
    OutputDebugStringW((L"[ErrorLogReader]   From Configuration: " + 
                        std::to_wstring(configEvents.size()) + L"\n").c_str());
    OutputDebugStringW(L"[ErrorLogReader] ========================================\n\n");

    return allEntries;
}

// ============================================================================
// Query Single Log
// ============================================================================

std::vector<ErrorLogEntry> ErrorLogReader::querySingleLog(
    const std::wstring& logName,
    const std::wstring& queryXPath,
    ULARGE_INTEGER uliCutoff,
    int days)
{
    std::vector<ErrorLogEntry> entries;
    
    OutputDebugStringW((L"[ErrorLogReader] Querying log: " + logName + L"\n").c_str());
    
    EVT_HANDLE hResults = EvtQuery(nullptr, logName.c_str(), queryXPath.c_str(),
                                   EvtQueryChannelPath | EvtQueryForwardDirection);
    if (!hResults) {
        DWORD error = GetLastError();
        OutputDebugStringW((L"[ErrorLogReader] Query FAILED! Error: " + 
                            std::to_wstring(error) + L"\n").c_str());
        return entries;  // Log might not exist on this system
    }

    const DWORD BATCH_SIZE = 10;
    EVT_HANDLE events[BATCH_SIZE];
    DWORD returned = 0;
    int totalFetched = 0;

    while (EvtNext(hResults, BATCH_SIZE, events, INFINITE, 0, &returned)) {
        totalFetched += returned;
        for (DWORD i = 0; i < returned; i++) {
            ErrorLogEntry entry = parseEvent(events[i]);

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
// Event Filtering & Classification
// ============================================================================

void ErrorLogReader::classifyEvent(ErrorLogEntry& entry)
{
    // Check if this is a SWD\ software device
    if (entry.deviceInstanceId.has_value()) {
        const std::wstring& instanceId = entry.deviceInstanceId.value();
        if (instanceId.find(L"SWD\\") == 0 || instanceId.find(L"SW\\") == 0) {
            entry.category = ErrorLogEntry::Category::SwdBackground;
            return;
        }
    }
    
    // If has DeviceInstanceId → DeviceMapped, else SystemWide
    if (entry.deviceInstanceId.has_value() && !entry.deviceInstanceId.value().empty()) {
        entry.category = ErrorLogEntry::Category::DeviceMapped;
    } else {
        entry.category = ErrorLogEntry::Category::SystemWide;
    }
}

bool ErrorLogReader::isExcludedEvent(const ErrorLogEntry& entry)
{
    for (const auto& excluded : EXCLUDED_EVENTS) {
        if (entry.eventId == excluded.eventId) {
            // If provider specified, must match exactly
            if (!excluded.provider.empty() && entry.provider != excluded.provider) {
                continue;
            }
            return true;  // Event is excluded
        }
    }
    return false;
}


// ============================================================================
// Dynamic Provider Discovery
// ============================================================================

std::unordered_set<std::wstring> ErrorLogReader::discoverHardwareProviders(ULARGE_INTEGER uliCutoff)
{
    std::unordered_set<std::wstring> providers;
    
    // Build discovery query: errors/warnings containing hardware keywords
    std::wstring discoveryQuery = L"*[System[(Level=1 or Level=2 or Level=3)]]";
    
    EVT_HANDLE hResults = EvtQuery(nullptr, L"System", discoveryQuery.c_str(),
                                   EvtQueryChannelPath | EvtQueryReverseDirection);
    if (!hResults) {
        OutputDebugStringW(L"[ErrorLogReader] Discovery query failed\n");
        return providers;
    }
    
    const DWORD BATCH_SIZE = 500;  // Increased from 50 to catch older GPU/disk errors
    EVT_HANDLE events[BATCH_SIZE];
    DWORD returned = 0;
    
    if (EvtNext(hResults, BATCH_SIZE, events, INFINITE, 0, &returned)) {
        for (DWORD i = 0; i < returned; i++) {
            // Render event XML to get provider name
            DWORD bufferSize = 0;
            DWORD bufferUsed = 0;
            DWORD propertyCount = 0;
            
            EvtRender(nullptr, events[i], EvtRenderEventXml, 0, nullptr, &bufferUsed, &propertyCount);
            bufferSize = bufferUsed;
            std::vector<wchar_t> buffer(bufferSize / sizeof(wchar_t) + 1);
            
            if (EvtRender(nullptr, events[i], EvtRenderEventXml,
                         bufferSize, buffer.data(), &bufferUsed, &propertyCount)) {
                std::wstring xml(buffer.data());
                
                // Extract provider name
                size_t providerPos = xml.find(L"<Provider Name='");
                if (providerPos == std::wstring::npos) {
                    providerPos = xml.find(L"<Provider Name=\"");
                }
                
                if (providerPos != std::wstring::npos) {
                    providerPos += 16;
                    wchar_t quoteChar = xml[providerPos - 1];
                    size_t endPos = xml.find(quoteChar, providerPos);
                    
                    if (endPos != std::wstring::npos) {
                        std::wstring providerName = xml.substr(providerPos, endPos - providerPos);
                        
                        // Check if provider name matches any hardware provider
                        bool isHardwareProvider = false;
                        for (const auto& hwProvider : HARDWARE_PROVIDERS) {
                            if (providerName == hwProvider) {
                                isHardwareProvider = true;
                                break;
                            }
                        }
                        
                        // Skip core providers (already queried)
                        bool isCoreProvider = false;
                        for (const auto& core : CORE_PROVIDERS) {
                            if (providerName == core) {
                                isCoreProvider = true;
                                break;
                            }
                        }
                        
                        if (isHardwareProvider && !isCoreProvider) {
                            providers.insert(providerName);
                        }
                    }
                }
            }
            
            EvtClose(events[i]);
        }
    }
    
    EvtClose(hResults);
    return providers;
}

std::wstring ErrorLogReader::buildDynamicProviderQuery(const std::unordered_set<std::wstring>& providers)
{
    if (providers.empty()) {
        return L"";
    }
    
    std::wstring query = L"*[System[(Level=1 or Level=2 or Level=3) and (";
    
    bool first = true;
    for (const auto& provider : providers) {
        if (!first) query += L" or ";
        query += L"Provider[@Name='" + provider + L"']";
        first = false;
    }
    
    query += L")]]";
    return query;
}

// New function for static hardware provider list
static std::wstring buildHardwareProviderQuery(const std::vector<std::wstring>& providers)
{
    if (providers.empty()) {
        return L"";
    }
    
    std::wstring query = L"*[System[(Level=1 or Level=2 or Level=3) and (";
    
    bool first = true;
    for (const auto& provider : providers) {
        if (!first) query += L" or ";
        query += L"Provider[@Name='" + provider + L"']";
        first = false;
    }
    
    query += L")]]";
    return query;
}

// ============================================================================
// Filter for Specific Driver
// ============================================================================

std::vector<ErrorLogEntry> ErrorLogReader::filterForDriver(
    const std::vector<ErrorLogEntry>& allEvents,
    const std::wstring& driverInstanceId)
{
    std::vector<ErrorLogEntry> matched;

    // Normalize instance ID (uppercase)
    std::wstring normalizedTarget = driverInstanceId;
    std::transform(normalizedTarget.begin(), normalizedTarget.end(),
                   normalizedTarget.begin(), ::towupper);

    for (const auto& event : allEvents) {
        // Skip system-wide events (no device association)
        if (event.category == ErrorLogEntry::Category::SystemWide) {
            continue;
        }

        if (!event.deviceInstanceId.has_value()) {
            continue;
        }

        std::wstring normalizedEventId = event.deviceInstanceId.value();
        std::transform(normalizedEventId.begin(), normalizedEventId.end(),
                       normalizedEventId.begin(), ::towupper);

        // Exact match
        if (normalizedEventId == normalizedTarget) {
            matched.push_back(event);
            continue;
        }

        // Parent-child match (e.g., USB composite device)
        // If event's ID is parent of target, or target is parent of event's ID
        if (normalizedEventId.find(normalizedTarget) != std::wstring::npos ||
            normalizedTarget.find(normalizedEventId) != std::wstring::npos) {
            matched.push_back(event);
        }
    }

    return matched;
}

void ErrorLogReader::populateErrorLogs(std::vector<DriverInfo>& drivers, int days)
{
    // Query all events once
    auto allEvents = querySystemLog(days);

    OutputDebugStringW(L"\n[ErrorLogReader] === Populating Driver Error Logs ===\n");

    // Match events to each driver
    for (auto& driver : drivers) {
        driver.errorLog = filterForDriver(allEvents, driver.instanceId);
        driver.errorLogWindowDays = days;

        if (!driver.errorLog.empty()) {
            OutputDebugStringW((L"[ErrorLogReader] " + driver.name + 
                                L": " + std::to_wstring(driver.errorLog.size()) + 
                                L" events\n").c_str());
        }
    }

    OutputDebugStringW(L"[ErrorLogReader] =====================================\n\n");
}

// ============================================================================
// Event Parsing
// ============================================================================

ErrorLogEntry ErrorLogReader::parseEvent(EVT_HANDLE hEvent)
{
    ErrorLogEntry entry;

    // Render event as XML
    DWORD bufferSize = 0;
    DWORD bufferUsed = 0;
    DWORD propertyCount = 0;

    EvtRender(nullptr, hEvent, EvtRenderEventXml, 0, nullptr, &bufferUsed, &propertyCount);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        OutputDebugStringW(L"[ErrorLogReader] Failed to get buffer size\n");
        return entry;
    }

    bufferSize = bufferUsed;
    std::vector<wchar_t> buffer(bufferSize / sizeof(wchar_t) + 1);
    
    if (!EvtRender(nullptr, hEvent, EvtRenderEventXml,
                   bufferSize, buffer.data(), &bufferUsed, &propertyCount)) {
        OutputDebugStringW(L"[ErrorLogReader] Failed to render event\n");
        return entry;
    }

    std::wstring xml(buffer.data());
    entry.eventId = extractEventId(xml);
    
    // Extract level
    std::wstring levelNum = extractXmlField(xml, L"Level");
    if      (levelNum == L"1") entry.level = L"Critical";
    else if (levelNum == L"2") entry.level = L"Error";
    else if (levelNum == L"3") entry.level = L"Warning";
    else if (levelNum == L"4") entry.level = L"Information";
    else                       entry.level = L"Unknown";

    // Extract provider
    size_t providerPos = xml.find(L"<Provider Name='");
    if (providerPos == std::wstring::npos) {
        providerPos = xml.find(L"<Provider Name=\"");
    }
    
    if (providerPos != std::wstring::npos) {
        providerPos += 16;
        wchar_t quoteChar = xml[providerPos - 1];
        size_t endPos = xml.find(quoteChar, providerPos);
        if (endPos != std::wstring::npos) {
            entry.provider = xml.substr(providerPos, endPos - providerPos);
        }
    }

    entry.timestamp = extractTimestamp(xml);

    // ========================================================================
    // Extract Driver/Device Name (Friendly Name)
    // ========================================================================
    
    // Try multiple possible field names for friendly device/driver name
    entry.driverName = extractEventDataField(xml, L"DeviceName");
    if (!entry.driverName.has_value())
        entry.driverName = extractEventDataField(xml, L"DriverDescription");
    if (!entry.driverName.has_value())
        entry.driverName = extractEventDataField(xml, L"FriendlyName");
    if (!entry.driverName.has_value())
        entry.driverName = extractEventDataField(xml, L"DeviceDesc");

    // ========================================================================
    // Extract DeviceInstanceId (try multiple field names)
    // ========================================================================
    
    entry.deviceInstanceId = extractEventDataField(xml, L"DeviceInstanceId");
    if (!entry.deviceInstanceId.has_value())
        entry.deviceInstanceId = extractEventDataField(xml, L"InstanceId");
    if (!entry.deviceInstanceId.has_value())
        entry.deviceInstanceId = extractEventDataField(xml, L"TargetInstanceId");
    if (!entry.deviceInstanceId.has_value())
        entry.deviceInstanceId = extractEventDataField(xml, L"DriverName");

    if (entry.deviceInstanceId.has_value()) {
        entry.deviceInstanceId = decodeHtmlEntities(entry.deviceInstanceId.value());
    }

    // ========================================================================
    // Extract Lifecycle Metadata (Event 400, 420, 430, 431)
    // ========================================================================
    
    if (entry.level == L"Information" && LIFECYCLE_EVENT_IDS.count(entry.eventId) > 0) {
        // Driver version
        entry.driverVersion = extractEventDataField(xml, L"DriverVersion");
        
        // Driver provider/manufacturer
        entry.driverProvider = extractEventDataField(xml, L"DriverProvider");
        
        // INF filename
        entry.infName = extractEventDataField(xml, L"DriverName");
        if (!entry.infName.has_value())
            entry.infName = extractEventDataField(xml, L"InfName");
        
        // Check if this was an update vs fresh install (Event 400 only)
        if (entry.eventId == 400) {
            auto deviceUpdated = extractEventDataField(xml, L"DeviceUpdated");
            if (deviceUpdated.has_value()) {
                entry.isDriverUpdate = (deviceUpdated.value() == L"true");
            }
        }
    }

    // ========================================================================
    // Extract Error Context (for errors/warnings)
    // ========================================================================
    
    if (entry.level == L"Critical" || entry.level == L"Error" || entry.level == L"Warning") {
        // Status code
        auto statusStr = extractEventDataField(xml, L"Status");
        if (statusStr.has_value()) {
            try { 
                entry.statusCode = std::stoul(statusStr.value(), nullptr, 0);  // Auto-detect hex/decimal
            } catch (...) {}
        }
        
        // Failure reason (UMDF crashes, driver load failures)
        entry.failureReason = extractEventDataField(xml, L"HostProblem");
        if (!entry.failureReason.has_value())
            entry.failureReason = extractEventDataField(xml, L"FailureStatus");
        if (!entry.failureReason.has_value())
            entry.failureReason = extractEventDataField(xml, L"Problem");
    }

    // ========================================================================
    // Build Message
    // ========================================================================
    
    entry.message = buildEventMessage(entry.eventId, entry.provider, xml);

    // Enhance lifecycle messages with version info
    if (entry.eventId == 400 && entry.driverVersion.has_value()) {
        if (entry.isDriverUpdate.value_or(false)) {
            entry.message = L"Driver updated to version " + entry.driverVersion.value();
        } else {
            entry.message = L"Driver installed (version " + entry.driverVersion.value() + L")";
        }
    }

    return entry;
}

// ============================================================================
// Message Building
// ============================================================================

std::wstring ErrorLogReader::buildEventMessage(int eventId, const std::wstring& provider, const std::wstring& xml)
{
    // ========================================================================
    // Provider-Specific Event Templates
    // ========================================================================
    
    if (provider == L"Microsoft-Windows-Kernel-PnP") {
        switch (eventId) {
            case 219: {
                // Driver load failed - try to extract failure details
                auto fn = extractEventDataField(xml, L"FailureName");
                auto st = extractEventDataField(xml, L"Status");
                if (fn.has_value()) {
                    std::wstring msg = L"Driver load failed: " + fn.value();
                    if (st.has_value()) msg += L" (Status: 0x" + st.value() + L")";
                    return msg;
                }
                return L"Driver load failed";
            }
            
            // Lifecycle events
            case 400: return L"Device installation completed";
            case 403: return L"Driver install started";
            case 410: return L"Device configured successfully";
            case 411: {
                auto st = extractEventDataField(xml, L"Status");
                return st.has_value()
                    ? L"Device removal failed (Status: 0x" + st.value() + L")"
                    : L"Device removal failed";
            }
            case 420: return L"Device requires system restart to function";
            case 430: return L"Device removal started";
            case 431: return L"Device removal completed";
            case 442: return L"Device was disabled";
            case 443: return L"Device was enabled";
        }
    }
    
    if (provider == L"Microsoft-Windows-DriverFrameworks-UserMode") {
        switch (eventId) {
            case 10116: return L"Device entered error state (UMDF)";
            case 10120: return L"User-mode driver host encountered a problem";
            case 10121: {
                // Try to extract specific failure reason
                auto reason = extractEventDataField(xml, L"HostProblem");
                if (reason.has_value()) {
                    return L"User-mode driver runtime failure: " + reason.value();
                }
                return L"User-mode driver runtime failure";
            }
            case 1001: return L"Driver took too long to load";
            case 1002: return L"Driver initialization timed out";
            case 10000: {
                auto ex = extractEventDataField(xml, L"ExceptionCode");
                return ex.has_value()
                    ? L"Driver crashed (Exception: 0x" + ex.value() + L")"
                    : L"Driver crashed";
            }
            case 10010: return L"Driver stopped responding";
            case 10100: return L"Driver framework reflector warning";
            case 2004: return L"Kernel driver warning";
            case 10001: return L"Kernel driver error";
        }
    }
    
    // ========================================================================
    // Generic Messages by Event ID (cross-provider)
    // ========================================================================
    
    // Note: These might appear from multiple providers, so check them after
    // provider-specific handling
    
    if (eventId == 219) return L"Driver load failed";
    if (eventId == 153) return L"Driver error (Event 153)";
    if (eventId == 11)  return L"Driver-related error";
    
    // ========================================================================
    // Ultimate Fallback
    // ========================================================================
    
    return L"Event " + std::to_wstring(eventId);
}

// ============================================================================
// XML Parsing Helpers
// ============================================================================

std::wstring ErrorLogReader::extractXmlField(const std::wstring& xml, const std::wstring& tagName)
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

std::chrono::system_clock::time_point ErrorLogReader::extractTimestamp(const std::wstring& xml)
{
    size_t pos = xml.find(L"SystemTime='");
    size_t quoteLen = 12;
    wchar_t closeQuote = L'\'';

    if (pos == std::wstring::npos) {
        pos = xml.find(L"SystemTime=\"");
        closeQuote = L'"';
        if (pos == std::wstring::npos) {
            return std::chrono::system_clock::time_point{};
        }
    }

    pos += quoteLen;
    size_t end = xml.find(closeQuote, pos);
    if (end == std::wstring::npos) {
        return std::chrono::system_clock::time_point{};
    }

    std::wstring timeStr = xml.substr(pos, end - pos);

    SYSTEMTIME st = {};
    int parsed = swscanf_s(timeStr.c_str(), L"%4hu-%2hu-%2huT%2hu:%2hu:%2hu",
                           &st.wYear, &st.wMonth, &st.wDay,
                           &st.wHour, &st.wMinute, &st.wSecond);
    if (parsed < 6) {
        return std::chrono::system_clock::time_point{};
    }

    FILETIME ft;
    if (!SystemTimeToFileTime(&st, &ft)) {
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