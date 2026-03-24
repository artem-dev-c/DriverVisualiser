#include "SystemHealthSummary.h"
#include <algorithm>
#include <numeric>
#include <windows.h>

// ============================================================================
// Constants
// ============================================================================

namespace {
    // Importance weights for weighted average
    constexpr int WEIGHT_CRITICAL  = 4;
    constexpr int WEIGHT_IMPORTANT = 2;
    constexpr int WEIGHT_OPTIONAL  = 1;
    constexpr int WEIGHT_VIRTUAL   = 0;  // Excluded from scoring

    // System-level penalty constants
    constexpr int PENALTY_PER_CRITICAL_DRIVER = -8;
    constexpr int MAX_CRITICAL_DRIVER_PENALTY  = -25;
    constexpr int PENALTY_MULTIPLE_WARNINGS    = -5;
    constexpr int PENALTY_UNSIGNED_PRESENT     = -5;
    constexpr int PENALTY_POOR_METADATA        = -3;

    // Health tier thresholds (must match MainWindow/CategoryProcessor)
    constexpr int THRESHOLD_CRITICAL = 70;
    constexpr int THRESHOLD_WARNING  = 90;

    // Flags shown in the dashboard issue list
    // All others (outdated, no version, etc.) are noise for this surface
    const std::vector<std::wstring> ACTIONABLE_FLAG_IDS = {
        L"DRIVER_BLOCKED",
        L"PROBLEM_CODE_CRITICAL",
        L"UNSIGNED_DRIVER",
        L"NEEDS_RESTART",
        L"DEVICE_DISCONNECTED",
        L"USER_DISABLED",
        // Error log frequency flags
        L"CRITICAL_ERROR_RATE_SEVERE",
        L"CRITICAL_ERROR_RATE_HIGH",
        L"CRITICAL_ERROR_RATE_MODERATE",
        L"CRITICAL_ERROR_RATE_LOW",
        L"WARNING_RATE_SEVERE",
        L"WARNING_RATE_HIGH",
        L"WARNING_RATE_MODERATE",
        // Graduated age flags (ancient/very old warrant dashboard visibility)
        L"OUTDATED_DRIVER_ANCIENT",      // 7+ years
        L"OUTDATED_DRIVER_VERY_OLD",     // 5+ years
        // Critical device metadata flags (escalated to Warning severity)
        L"NO_VERSION_INFO_CRITICAL",
        L"NO_DRIVER_DATE_CRITICAL",
        // SystemWide error rate flags
        L"SYSTEMWIDE_ERROR_RATE_SEVERE",
        L"SYSTEMWIDE_ERROR_RATE_HIGH",
        L"SYSTEMWIDE_ERROR_RATE_MODERATE",
        L"SYSTEMWIDE_ERROR_RATE_LOW",
        // SystemWide warning rate flags
        L"SYSTEMWIDE_WARNING_RATE_SEVERE",
        L"SYSTEMWIDE_WARNING_RATE_HIGH",
        L"SYSTEMWIDE_WARNING_RATE_MODERATE",
        // Critical hardware failure types (separate flags)
        L"GPU_CRASHES_DETECTED",
        L"UMDF_FRAMEWORK_FAILURES",
        L"DISK_CONTROLLER_ERRORS",
        L"STORAGE_CONTROLLER_ERRORS",
        L"NETWORK_ADAPTER_FAILURES",
        L"VM_NETWORK_ERRORS"
    };
}

// ============================================================================
// Public API
// ============================================================================

SystemHealthResult SystemHealthSummary::compute(const std::vector<DriverInfo>& drivers,
                                               const std::vector<ErrorLogEntry>& allEvents,
                                               int scanDays)
{
    SystemHealthResult result;

    if (drivers.empty()) {
        return result;
    }

    // === Driver counts (excluding user-disabled from problem counts) ===
    for (const auto& driver : drivers) {
        bool isUserDisabled = (driver.problemCode == 22);  // CM_PROB_DISABLED

        result.totalDrivers++;

        if (isUserDisabled) {
            result.healthyDrivers++;  // Intentional action, count as healthy
        } else if (driver.healthScore < THRESHOLD_CRITICAL) {
            result.criticalDrivers++;
        } else if (driver.healthScore < THRESHOLD_WARNING) {
            result.warningDrivers++;
        } else {
            result.healthyDrivers++;
        }
    }

    // === Step 1: Weighted average base score ===
    int baseScore = computeWeightedScore(drivers);

    // === Step 2: Apply system-level penalty flags ===
    int finalScore = applySystemFlags(baseScore, drivers, allEvents, scanDays, result);

    result.score = std::clamp(finalScore, 0, 100);

    // === Step 3: Extract actionable issues for display ===
    result.issues = extractIssues(drivers, result);

    return result;
}

// ============================================================================
// Weighted Score
// ============================================================================

int SystemHealthSummary::computeWeightedScore(const std::vector<DriverInfo>& drivers)
{
    long long weightedSum = 0;
    int totalWeight = 0;

    for (const auto& driver : drivers) {
        int weight = importanceWeight(driver.importanceLevel);
        if (weight == 0) {
            continue;  // Skip Virtual devices entirely
        }

        weightedSum += static_cast<long long>(driver.healthScore) * weight;
        totalWeight += weight;
    }

    if (totalWeight == 0) {
        return 100;  // All virtual, no real devices
    }

    return static_cast<int>(weightedSum / totalWeight);
}

// ============================================================================
// System-Level Penalty Flags
// ============================================================================

int SystemHealthSummary::applySystemFlags(int baseScore,
                                          const std::vector<DriverInfo>& drivers,
                                          const std::vector<ErrorLogEntry>& allEvents,
                                          int scanDays,
                                          SystemHealthResult& result)
{
    int score = baseScore;

    // --- Flag 1: Critical drivers present ---
    if (result.criticalDrivers > 0) {
        int penalty = std::max(
            PENALTY_PER_CRITICAL_DRIVER * result.criticalDrivers,
            MAX_CRITICAL_DRIVER_PENALTY
        );

        SystemHealthFlag flag;
        flag.id          = L"CRITICAL_DRIVERS_PRESENT";
        flag.description = std::to_wstring(result.criticalDrivers) +
                           (result.criticalDrivers == 1
                                ? L" driver has a critical failure"
                                : L" drivers have critical failures");
        flag.severity    = HealthFlagSeverity::Critical;
        flag.penalty     = penalty;

        result.systemFlags.push_back(flag);
        score += penalty;
    }

    // --- Flag 2: Multiple warning drivers ---
    if (result.warningDrivers >= 3) {
        SystemHealthFlag flag;
        flag.id          = L"MULTIPLE_WARNINGS";
        flag.description = std::to_wstring(result.warningDrivers) +
                           L" drivers have warnings";
        flag.severity    = HealthFlagSeverity::Warning;
        flag.penalty     = PENALTY_MULTIPLE_WARNINGS;

        result.systemFlags.push_back(flag);
        score += PENALTY_MULTIPLE_WARNINGS;
    }

    // --- Flag 3: Unsigned drivers present ---
    bool hasUnsigned = false;
    for (const auto& driver : drivers) {
        if (!driver.isSigned) {
            hasUnsigned = true;
            break;
        }
    }

    if (hasUnsigned) {
        SystemHealthFlag flag;
        flag.id          = L"UNSIGNED_DRIVERS_PRESENT";
        flag.description = L"One or more drivers are not digitally signed";
        flag.severity    = HealthFlagSeverity::Warning;
        flag.penalty     = PENALTY_UNSIGNED_PRESENT;

        result.systemFlags.push_back(flag);
        score += PENALTY_UNSIGNED_PRESENT;
    }

    // --- Flag 4: Poor metadata on important/critical drivers ---
    int importantWithNoDate = 0;
    int totalImportantOrCritical = 0;

    for (const auto& driver : drivers) {
        if (driver.importanceLevel == DriverImportance::Critical ||
            driver.importanceLevel == DriverImportance::Important) {
            totalImportantOrCritical++;
            if (!driver.driverDate.has_value() && !driver.installDate.has_value()) {
                importantWithNoDate++;
            }
        }
    }

    if (totalImportantOrCritical > 0 &&
        importantWithNoDate > totalImportantOrCritical / 2) {
        SystemHealthFlag flag;
        flag.id          = L"POOR_DRIVER_METADATA";
        flag.description = L"Many drivers are missing date information";
        flag.severity    = HealthFlagSeverity::Caution;
        flag.penalty     = PENALTY_POOR_METADATA;

        result.systemFlags.push_back(flag);
        score += PENALTY_POOR_METADATA;
    }

    // ========================================================================
    // Flag 5: SystemWide Event Analysis (Enhanced)
    // ========================================================================
    
    // Count SystemWide events by severity
    int criticalSystemWide = 0;
    int warningSystemWide = 0;
    
    // Track critical hardware failure types with occurrence counts
    int gpuCrashCount = 0;
    int umdfCrashCount = 0;
    int diskErrorCount = 0;
    int storageErrorCount = 0;
    int networkFailureCount = 0;
    int vmErrorCount = 0;
    
    for (const auto& entry : allEvents) {
        if (entry.category != ErrorLogEntry::Category::SystemWide) {
            continue;
        }
        
        // Convert provider to lowercase for case-insensitive matching
        std::wstring providerLower = entry.provider;
        std::transform(providerLower.begin(), providerLower.end(), 
                      providerLower.begin(), ::towlower);
        
        // Count by severity level
        if (entry.level == L"Critical" || entry.level == L"Error") {
            criticalSystemWide++;
            
            // Detect and count critical hardware failure types
            
            // GPU crashes (NVIDIA, AMD)
            if ((providerLower == L"nvlddmkm" || 
                 providerLower == L"amdkmdag" || 
                 providerLower == L"atikmdag") && 
                entry.eventId == 153) {
                gpuCrashCount++;
            }
            
            // UMDF framework crashes (USB device failures)
            else if (providerLower.find(L"umdf") != std::wstring::npos &&
                (entry.eventId == 10116 || entry.eventId == 10120 || entry.eventId == 10121)) {
                umdfCrashCount++;
            }
            
            // Disk controller errors
            else if (providerLower == L"disk" && 
                (entry.eventId == 11 || entry.eventId == 153)) {
                diskErrorCount++;
            }
            
            // Storage controller errors (SATA, NVMe)
            else if ((providerLower == L"storahci" || 
                 providerLower == L"stornvme" || 
                 providerLower == L"iastorac") &&
                (entry.eventId == 129 || entry.eventId == 153)) {
                storageErrorCount++;
            }
            
            // Network adapter failures
            else if ((providerLower.find(L"netwtw") != std::wstring::npos || // Intel WiFi
                 providerLower == L"e1i65x64" ||                        // Intel Ethernet
                 providerLower == L"rt640x64" ||                        // Realtek Ethernet
                 providerLower == L"rtwlane" ||                         // Realtek WiFi
                 providerLower == L"rtux64w10" ||                       // Realtek USB WiFi
                 providerLower == L"bcmwl63a") &&                       // Broadcom WiFi
                (entry.eventId == 5002 || entry.eventId == 5005 || 
                 entry.eventId == 5007 || entry.eventId == 5010)) {
                networkFailureCount++;
            }
            
            // VirtualBox errors
            else if ((providerLower == L"vboxnetlwf" || providerLower == L"vboxusbmon") &&
                entry.eventId == 12) {
                vmErrorCount++;
            }
        }
        else if (entry.level == L"Warning") {
            warningSystemWide++;
        }
    }
    
    // Calculate daily event rates (avoid division by zero)
    float errorRate = (scanDays > 0) ? (float)criticalSystemWide / scanDays : 0.0f;
    float warningRate = (scanDays > 0) ? (float)warningSystemWide / scanDays : 0.0f;
    
    // Debug output
    OutputDebugStringW((std::wstring(L"[SystemHealthSummary] SystemWide event analysis:\n") +
                        L"  Total events in allEvents: " + std::to_wstring(allEvents.size()) + L"\n" +
                        L"  Critical/Error SystemWide: " + std::to_wstring(criticalSystemWide) + L"\n" +
                        L"  Warning SystemWide: " + std::to_wstring(warningSystemWide) + L"\n" +
                        L"  Scan days: " + std::to_wstring(scanDays) + L"\n" +
                        L"  Error rate: " + std::to_wstring(errorRate) + L"/day\n" +
                        L"  GPU crashes: " + std::to_wstring(gpuCrashCount) + L"\n" +
                        L"  UMDF crashes: " + std::to_wstring(umdfCrashCount) + L"\n" +
                        L"  Disk errors: " + std::to_wstring(diskErrorCount) + L"\n" +
                        L"  Storage errors: " + std::to_wstring(storageErrorCount) + L"\n" +
                        L"  Network failures: " + std::to_wstring(networkFailureCount) + L"\n" +
                        L"  VM errors: " + std::to_wstring(vmErrorCount) + L"\n").c_str());
    
    // --- Flag 5a: SystemWide Error Rate (4 tiers) ---
    int errorRatePenalty = 0;
    std::wstring errorRateSeverity;
    
    if (errorRate >= 1.0f) {
        errorRatePenalty = -12;
        errorRateSeverity = L"SEVERE";
    } else if (errorRate >= 0.5f) {
        errorRatePenalty = -8;
        errorRateSeverity = L"HIGH";
    } else if (errorRate >= 0.3f) {
        errorRatePenalty = -5;
        errorRateSeverity = L"MODERATE";
    } else if (errorRate >= 0.1f) {
        errorRatePenalty = -3;
        errorRateSeverity = L"LOW";
    }
    
    if (errorRatePenalty < 0) {
        SystemHealthFlag flag;
        flag.id = L"SYSTEMWIDE_ERROR_RATE_" + errorRateSeverity;
        
        // Description varies by severity
        if (errorRateSeverity == L"SEVERE") {
            flag.description = L"Hardware stability critical: " + 
                               std::to_wstring(criticalSystemWide) + 
                               L" errors in " + std::to_wstring(scanDays) + L" days";
        } else if (errorRateSeverity == L"HIGH") {
            flag.description = L"Hardware stability issues: " + 
                               std::to_wstring(criticalSystemWide) + 
                               L" errors in " + std::to_wstring(scanDays) + L" days";
        } else if (errorRateSeverity == L"MODERATE") {
            flag.description = L"Moderate error rate: " + 
                               std::to_wstring(criticalSystemWide) + 
                               L" errors in " + std::to_wstring(scanDays) + L" days";
        } else {
            flag.description = L"Minor stability issues: " + 
                               std::to_wstring(criticalSystemWide) + 
                               L" errors in " + std::to_wstring(scanDays) + L" days";
        }
        
        flag.severity = HealthFlagSeverity::Warning;
        flag.penalty = errorRatePenalty;

        result.systemFlags.push_back(flag);
        score += errorRatePenalty;
    }
    
    // --- Flag 5b: Critical Hardware Failures (separate flags for each type) ---
    
    // GPU crashes
    if (gpuCrashCount > 0) {
        SystemHealthFlag flag;
        flag.id = L"GPU_CRASHES_DETECTED";
        flag.description = L"GPU driver crashes detected (" + 
                           std::to_wstring(gpuCrashCount) + 
                           (gpuCrashCount == 1 ? L" occurrence)" : L" occurrences)");
        flag.severity = HealthFlagSeverity::Critical;
        flag.penalty = -2;

        result.systemFlags.push_back(flag);
        score += -2;
    }
    
    // UMDF framework failures
    if (umdfCrashCount > 0) {
        SystemHealthFlag flag;
        flag.id = L"UMDF_FRAMEWORK_FAILURES";
        flag.description = L"USB device framework failures (" + 
                           std::to_wstring(umdfCrashCount) + 
                           (umdfCrashCount == 1 ? L" occurrence)" : L" occurrences)");
        flag.severity = HealthFlagSeverity::Critical;
        flag.penalty = -3;

        result.systemFlags.push_back(flag);
        score += -3;
    }
    
    // Disk controller errors
    if (diskErrorCount > 0) {
        SystemHealthFlag flag;
        flag.id = L"DISK_CONTROLLER_ERRORS";
        flag.description = L"Disk controller errors detected (" + 
                           std::to_wstring(diskErrorCount) + 
                           (diskErrorCount == 1 ? L" occurrence)" : L" occurrences)");
        flag.severity = HealthFlagSeverity::Critical;
        flag.penalty = -2;

        result.systemFlags.push_back(flag);
        score += -2;
    }
    
    // Storage controller errors
    if (storageErrorCount > 0) {
        SystemHealthFlag flag;
        flag.id = L"STORAGE_CONTROLLER_ERRORS";
        flag.description = L"Storage controller timeouts (" + 
                           std::to_wstring(storageErrorCount) + 
                           (storageErrorCount == 1 ? L" occurrence)" : L" occurrences)");
        flag.severity = HealthFlagSeverity::Critical;
        flag.penalty = -2;

        result.systemFlags.push_back(flag);
        score += -2;
    }
    
    // Network adapter failures
    if (networkFailureCount > 0) {
        SystemHealthFlag flag;
        flag.id = L"NETWORK_ADAPTER_FAILURES";
        flag.description = L"Network adapter failures (" + 
                           std::to_wstring(networkFailureCount) + 
                           (networkFailureCount == 1 ? L" occurrence)" : L" occurrences)");
        flag.severity = HealthFlagSeverity::Warning;
        flag.penalty = -1;

        result.systemFlags.push_back(flag);
        score += -1;
    }
    
    // VM network errors
    if (vmErrorCount > 0) {
        SystemHealthFlag flag;
        flag.id = L"VM_NETWORK_ERRORS";
        flag.description = L"Virtual machine network errors (" + 
                           std::to_wstring(vmErrorCount) + 
                           (vmErrorCount == 1 ? L" occurrence)" : L" occurrences)");
        flag.severity = HealthFlagSeverity::Warning;
        flag.penalty = -1;

        result.systemFlags.push_back(flag);
        score += -1;
    }
    
    // --- Flag 5c: SystemWide Warning Rate (3 tiers) ---
    int warningRatePenalty = 0;
    std::wstring warningRateSeverity;
    
    if (warningRate >= 2.0f) {
        warningRatePenalty = -6;
        warningRateSeverity = L"SEVERE";
    } else if (warningRate >= 1.0f) {
        warningRatePenalty = -4;
        warningRateSeverity = L"HIGH";
    } else if (warningRate >= 0.5f) {
        warningRatePenalty = -2;
        warningRateSeverity = L"MODERATE";
    }
    
    if (warningRatePenalty < 0) {
        SystemHealthFlag flag;
        flag.id = L"SYSTEMWIDE_WARNING_RATE_" + warningRateSeverity;
        flag.description = L"Elevated warning rate: " + 
                           std::to_wstring(warningSystemWide) + 
                           L" warnings in " + std::to_wstring(scanDays) + L" days";
        flag.severity = HealthFlagSeverity::Caution;
        flag.penalty = warningRatePenalty;

        result.systemFlags.push_back(flag);
        score += warningRatePenalty;
    }

    return score;
}

// ============================================================================
// Issue Extraction
// ============================================================================

std::vector<DashboardIssue> SystemHealthSummary::extractIssues(
    const std::vector<DriverInfo>& drivers,
    const SystemHealthResult& result)
{
    std::vector<DashboardIssue> issues;

    // Extract issues from per-driver health flags
    for (const auto& driver : drivers) {
        for (const auto& flag : driver.healthFlags) {
            if (!isActionableFlag(flag.id)) {
                continue;
            }

            DashboardIssue issue;

            // Use friendly name: prefer provider (friendly name), fall back to name
            if (!driver.provider.empty() && driver.provider != L"Unknown") {
                issue.driverName = driver.provider;
            } else {
                issue.driverName = driver.name;
            }

            issue.driverInstanceId = driver.instanceId;
            issue.categoryName     = driver.deviceClassName;
            issue.flagId           = flag.id;
            issue.description      = flag.description;
            issue.severity         = flag.severity;
            issue.isUserDisabled   = (flag.id == L"USER_DISABLED");

            issues.push_back(issue);
        }
    }
    
    // Extract issues from SystemWide flags (NEW)
    for (const auto& flag : result.systemFlags) {
        if (!isActionableFlag(flag.id)) {
            continue;
        }

        DashboardIssue issue;
        issue.driverName       = L"System";           // Generic name for SystemWide
        issue.driverInstanceId = L"";                 // No specific device
        issue.categoryName     = L"";                 // Empty triggers orange "System" badge
        issue.flagId           = flag.id;
        issue.description      = flag.description;
        issue.severity         = flag.severity;
        issue.isUserDisabled   = false;

        issues.push_back(issue);
    }

    // Sort: Critical first, then Warning, then user-disabled last
    std::sort(issues.begin(), issues.end(),
        [](const DashboardIssue& a, const DashboardIssue& b) {
            // User-disabled always last
            if (a.isUserDisabled != b.isUserDisabled) {
                return !a.isUserDisabled;
            }
            // Critical before Warning
            if (a.severity != b.severity) {
                return a.severity < b.severity;
            }
            // Alphabetical by driver name
            return a.driverName < b.driverName;
        }
    );

    return issues;
}

// ============================================================================
// Helpers
// ============================================================================

bool SystemHealthSummary::isActionableFlag(const std::wstring& flagId)
{
    for (const auto& id : ACTIONABLE_FLAG_IDS) {
        if (id == flagId) {
            return true;
        }
    }
    return false;
}

int SystemHealthSummary::importanceWeight(DriverImportance importance)
{
    switch (importance) {
        case DriverImportance::Critical:  return WEIGHT_CRITICAL;
        case DriverImportance::Important: return WEIGHT_IMPORTANT;
        case DriverImportance::Optional:  return WEIGHT_OPTIONAL;
        case DriverImportance::Virtual:   return WEIGHT_VIRTUAL;
        default:                          return WEIGHT_OPTIONAL;
    }
}