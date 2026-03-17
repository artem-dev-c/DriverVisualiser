#include "HealthScoreEvaluator.h"
#include <algorithm>
#include <chrono>
#include <windows.h>
#include <cfgmgr32.h>

// ============================================================================
// Constants
// ============================================================================

namespace {
    constexpr int PENALTY_CRITICAL = -40;
    constexpr int PENALTY_WARNING_HIGH = -15;
    constexpr int PENALTY_WARNING = -10;
    constexpr int PENALTY_CAUTION_HIGH = -5;
    constexpr int PENALTY_CAUTION_MED = -3;
    constexpr int PENALTY_CAUTION_LOW = -2;
    constexpr int PENALTY_NONE = 0;

    // Graduated age thresholds (days)
    constexpr int AGE_2_YEARS = 730;   // Aging - minor concern
    constexpr int AGE_3_YEARS = 1095;  // Outdated - moderate concern
    constexpr int AGE_5_YEARS = 1825;  // Very old - significant concern
    constexpr int AGE_7_YEARS = 2555;  // Ancient - critical security risk
}

// ============================================================================
// Main Evaluation
// ============================================================================

HealthResult HealthScoreEvaluator::evaluate(const DriverInfo& driver)
{
    HealthResult result;
    result.score = 100;

    evaluateCriticalFlags(driver, result);
    evaluateErrorLogFrequency(driver, result);  // MOVED UP - evaluate before warning flags
    evaluateWarningFlags(driver, result);        // Now includes outdated driver
    evaluateCautionFlags(driver, result);
    evaluateInfoFlags(driver, result);

    result.score = std::max(0, result.score);
    return result;
}

// ============================================================================
// Tier 1: Critical Flags
// ============================================================================

void HealthScoreEvaluator::evaluateCriticalFlags(const DriverInfo& driver, HealthResult& result)
{
    // Driver blocked by Windows
    if (driver.rawStatus & DN_DRIVER_BLOCKED) {
        addFlag(result, L"DRIVER_BLOCKED", L"Driver was blocked by Windows",
                HealthFlagSeverity::Critical, PENALTY_CRITICAL);
    }

    // Problem code present (real errors only, not user-disabled)
    if (driver.problemCode != 0 && isCriticalProblemCode(driver.problemCode)) {
        addFlag(result, L"PROBLEM_CODE_CRITICAL", getProblemCodeDescription(driver.problemCode),
                HealthFlagSeverity::Critical, PENALTY_CRITICAL);
    }

    // NOTE: File integrity check removed
    // Reason: Cannot reliably determine which drivers require .sys files
    // - Composite/software devices legitimately have no .sys
    // - Service registry lookup is unreliable
    // - False positive rate too high
    // Solution: .sys paths are shown in DriverInfoPopup for manual verification
}

// ============================================================================
// Tier 2: Warning Flags
// ============================================================================

void HealthScoreEvaluator::evaluateWarningFlags(const DriverInfo& driver, HealthResult& result)
{
    // Unsigned driver
    if (!driver.isSigned) {
        addFlag(result, L"UNSIGNED_DRIVER", L"Driver is not digitally signed",
                HealthFlagSeverity::Warning, PENALTY_WARNING_HIGH);
    }

    // Ghost device (not physically present)
    if (!driver.isPresent) {
        // Check how long it's been gone (estimate from install date)
        bool isStaleGhost = false;
        
        if (driver.installDate.has_value()) {
            auto now = std::chrono::system_clock::now();
            auto nowDays = std::chrono::floor<std::chrono::days>(now);
            auto age = nowDays - driver.installDate.value();
            
            // If installed 6+ months ago and still not present, it's stale
            if (age.count() > 180) {
                isStaleGhost = true;
            }
        }
        
        if (isStaleGhost) {
            addFlag(result, L"GHOST_DEVICE_STALE", 
                    L"Device not present for 6+ months (safe to remove)",
                    HealthFlagSeverity::Caution, PENALTY_CAUTION_MED);
        } else {
            addFlag(result, L"GHOST_DEVICE", 
                    L"Device is not physically present",
                    HealthFlagSeverity::Warning, PENALTY_WARNING);
        }
    }

    // Needs restart
    if (driver.rawStatus & DN_NEED_RESTART) {
        addFlag(result, L"NEEDS_RESTART", L"System restart required - driver may not function correctly until reboot",
                HealthFlagSeverity::Warning, PENALTY_WARNING_HIGH);
    }

    // Device disconnected (only if not already flagged as ghost)
    if ((driver.rawStatus & DN_DEVICE_DISCONNECTED) && 
        !hasFlag(result, L"GHOST_DEVICE") && 
        !hasFlag(result, L"GHOST_DEVICE_STALE")) {
        addFlag(result, L"DEVICE_DISCONNECTED", L"Device is disconnected",
                HealthFlagSeverity::Warning, PENALTY_WARNING);
    }

    // Outdated driver check
    evaluateOutdatedDriver(driver, result);
}

void HealthScoreEvaluator::evaluateOutdatedDriver(const DriverInfo& driver, HealthResult& result)
{
    std::optional<std::chrono::sys_days> dateToCheck;
    bool usingInstallDate = false;

    // Prefer valid driver date, fall back to install date
    if (isValidDriverDate(driver.driverDate)) {
        dateToCheck = driver.driverDate;
    } else if (driver.installDate.has_value()) {
        dateToCheck = driver.installDate;
        usingInstallDate = true;
    }

    if (!dateToCheck.has_value()) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto nowDays = std::chrono::floor<std::chrono::days>(now);
    auto age = nowDays - *dateToCheck;
    int ageDays = static_cast<int>(age.count());

    // Graduated penalties based on age
    if (ageDays > AGE_7_YEARS) {
        // 7+ years - ancient, critical security risk
        std::wstring description = usingInstallDate
            ? L"Driver is more than 7 years old (critical security risk, based on install date)"
            : L"Driver is more than 7 years old (critical security risk)";
        
        addFlag(result, L"OUTDATED_DRIVER_ANCIENT", description,
                HealthFlagSeverity::Warning, PENALTY_WARNING_HIGH);
                
    } else if (ageDays > AGE_5_YEARS) {
        // 5+ years - very old
        std::wstring description = usingInstallDate
            ? L"Driver is more than 5 years old (based on install date)"
            : L"Driver is more than 5 years old";
        
        addFlag(result, L"OUTDATED_DRIVER_VERY_OLD", description,
                HealthFlagSeverity::Warning, -12);
                
    } else if (ageDays > AGE_3_YEARS) {
        // 3+ years - outdated
        std::wstring description = usingInstallDate
            ? L"Driver is more than 3 years old (based on install date)"
            : L"Driver is more than 3 years old";
        
        addFlag(result, L"OUTDATED_DRIVER", description,
                HealthFlagSeverity::Warning, PENALTY_WARNING);
                
    } else if (ageDays > AGE_2_YEARS) {
        // 2+ years - aging
        std::wstring description = usingInstallDate
            ? L"Driver is more than 2 years old (based on install date)"
            : L"Driver is more than 2 years old";
        
        addFlag(result, L"OUTDATED_DRIVER_AGING", description,
                HealthFlagSeverity::Caution, PENALTY_CAUTION_MED);
    }
}

// ============================================================================
// Tier 3: Caution Flags
// ============================================================================

void HealthScoreEvaluator::evaluateCautionFlags(const DriverInfo& driver, HealthResult& result)
{
    // No version info - escalate penalty for critical devices
    if (!driver.version.hasVersion) {
        if (driver.importanceLevel == DriverImportance::Critical) {
            addFlag(result, L"NO_VERSION_INFO_CRITICAL", 
                    L"Critical device has no version information (risky)",
                    HealthFlagSeverity::Warning, -8);
        } else {
            addFlag(result, L"NO_VERSION_INFO", 
                    L"No version information available",
                    HealthFlagSeverity::Caution, PENALTY_CAUTION_HIGH);
        }
    }

    // No driver date - escalate penalty for critical devices
    if (!driver.driverDate.has_value()) {
        if (driver.importanceLevel == DriverImportance::Critical) {
            addFlag(result, L"NO_DRIVER_DATE_CRITICAL", 
                    L"Critical device has no driver date (risky)",
                    HealthFlagSeverity::Warning, PENALTY_CAUTION_HIGH);
        } else {
            addFlag(result, L"NO_DRIVER_DATE", 
                    L"No driver date information available",
                    HealthFlagSeverity::Caution, PENALTY_CAUTION_MED);
        }
    }

    // Unknown manufacturer - escalate penalty for critical/important devices
    if (driver.manufacturer == L"Unknown" || driver.manufacturer.empty()) {
        if (driver.importanceLevel == DriverImportance::Critical ||
            driver.importanceLevel == DriverImportance::Important) {
            addFlag(result, L"UNKNOWN_MANUFACTURER_IMPORTANT", 
                    L"Important device has missing manufacturer information",
                    HealthFlagSeverity::Caution, PENALTY_CAUTION_MED);
        } else {
            addFlag(result, L"UNKNOWN_MANUFACTURER", 
                    L"Manufacturer information is missing",
                    HealthFlagSeverity::Caution, PENALTY_CAUTION_LOW);
        }
    }
}

// ============================================================================
// Tier 4: Info Flags (No Penalty)
// ============================================================================

void HealthScoreEvaluator::evaluateInfoFlags(const DriverInfo& driver, HealthResult& result)
{
    // User-disabled device (shows as Critical/red but no penalty - intentional action)
    if (driver.problemCode == CM_PROB_DISABLED) {
        addFlag(result, L"USER_DISABLED", L"Device was disabled by user",
                HealthFlagSeverity::Critical, PENALTY_NONE);
    }

    // Not started by design (normal for resource devices)
    if (driver.status == DriverStatus::NotStarted && driver.problemCode == 0) {
        addFlag(result, L"NOT_STARTED_BY_DESIGN", L"Device is not started (normal for resource devices)",
                HealthFlagSeverity::Info, PENALTY_NONE);
    }

    // Driver not loaded by design
    if (!(driver.rawStatus & DN_DRIVER_LOADED) && driver.problemCode == 0) {
        if (!hasFlag(result, L"NOT_STARTED_BY_DESIGN")) {
            addFlag(result, L"DRIVER_NOT_LOADED_BY_DESIGN", L"No driver loaded (normal for some devices)",
                    HealthFlagSeverity::Info, PENALTY_NONE);
        }
    }

    // System critical device
    if (!(driver.rawStatus & DN_DISABLEABLE)) {
        addFlag(result, L"SYSTEM_CRITICAL", L"System-critical device (cannot be disabled)",
                HealthFlagSeverity::Info, PENALTY_NONE);
    }

    // Virtual device
    if (!driver.hardwareId.empty()) {
        const std::wstring& hwid = driver.hardwareId;
        if (hwid.find(L"SWD\\") == 0 || hwid.find(L"SW\\") == 0 ||
            hwid.find(L"ROOT\\") == 0 || hwid.find(L"UMB\\") == 0) {
            addFlag(result, L"VIRTUAL_DEVICE", L"This is a virtual/software device",
                    HealthFlagSeverity::Info, PENALTY_NONE);
        }
    }

    // Generic Microsoft driver
    if (driver.provider.find(L"Microsoft") != std::wstring::npos ||
        driver.manufacturer.find(L"Microsoft") != std::wstring::npos) {
        addFlag(result, L"GENERIC_DRIVER", L"Using Microsoft generic driver",
                HealthFlagSeverity::Info, PENALTY_NONE);
    }
}

// ============================================================================
// Tier 5: Error Log Frequency Evaluation
// ============================================================================

// NOTE: These thresholds are educated guesses based on MVP development.
// They should be tuned based on real-world telemetry and user feedback.
// Consider collecting data on: error frequency vs driver failures, user reports,
// and industry benchmarks to refine these values in future versions.

void HealthScoreEvaluator::evaluateErrorLogFrequency(const DriverInfo& driver, HealthResult& result)
{
    if (driver.errorLog.empty()) {
        return;  // No errors - healthy driver
    }

    // Count critical/error and warning events
    int criticalCount = 0;
    int warningCount = 0;

    for (const auto& entry : driver.errorLog) {
        if (entry.level == L"Critical" || entry.level == L"Error") {
            criticalCount++;
        } else if (entry.level == L"Warning") {
            warningCount++;
        }
        // Ignore "Information" level events
    }

    // Use the actual scan window from the driver
    int scanDays = driver.errorLogWindowDays;
    
    // Fallback: If we have timestamps and scanDays wasn't set, calculate actual coverage
    if (driver.errorLog.size() >= 2 && scanDays <= 0) {
        auto oldest = driver.errorLog.front().timestamp;
        auto newest = driver.errorLog.back().timestamp;
        auto duration = std::chrono::duration_cast<std::chrono::days>(newest - oldest);
        if (duration.count() > 0) {
            scanDays = std::max(1, static_cast<int>(duration.count()));
        } else {
            scanDays = 30;  // Absolute fallback
        }
    }

    // Calculate average errors per day
    double criticalPerDay = static_cast<double>(criticalCount) / scanDays;
    double warningPerDay = static_cast<double>(warningCount) / scanDays;

    // Penalize based on frequency
    // Critical/Error events: High penalty
    if (criticalCount > 0) {
        int penalty = 0;
        std::wstring description;

        if (criticalPerDay >= 5.0) {
            // 5+ critical errors per day - severe instability
            penalty = PENALTY_CRITICAL;
            description = L"Very high critical error rate (" + 
                         std::to_wstring(criticalCount) + L" errors in " + 
                         std::to_wstring(scanDays) + L" days)";
            addFlag(result, L"CRITICAL_ERROR_RATE_SEVERE", description,
                    HealthFlagSeverity::Critical, penalty);
        } else if (criticalPerDay >= 2.0) {
            // 2-4 critical errors per day - high instability
            penalty = PENALTY_WARNING_HIGH;
            description = L"High critical error rate (" + 
                         std::to_wstring(criticalCount) + L" errors in " + 
                         std::to_wstring(scanDays) + L" days)";
            addFlag(result, L"CRITICAL_ERROR_RATE_HIGH", description,
                    HealthFlagSeverity::Warning, penalty);
        } else if (criticalPerDay >= 0.5) {
            // 1 error every 2 days - moderate concern (but still Warning - active problem!)
            penalty = PENALTY_WARNING;
            description = L"Moderate critical error rate (" + 
                         std::to_wstring(criticalCount) + L" errors in " + 
                         std::to_wstring(scanDays) + L" days)";
            addFlag(result, L"CRITICAL_ERROR_RATE_MODERATE", description,
                    HealthFlagSeverity::Warning, penalty);
        } else {
            // Occasional critical errors - still Warning (active failures)
            penalty = PENALTY_CAUTION_HIGH;
            description = L"Occasional critical errors (" + 
                         std::to_wstring(criticalCount) + L" errors in " + 
                         std::to_wstring(scanDays) + L" days)";
            addFlag(result, L"CRITICAL_ERROR_RATE_LOW", description,
                    HealthFlagSeverity::Warning, penalty);
        }
    }

    // Warning events: Lower penalty but still important
    if (warningCount > 0) {
        int penalty = 0;
        std::wstring description;

        if (warningPerDay >= 5.0) {
            // 5+ warnings per day - very significant issue (Warning severity)
            penalty = PENALTY_WARNING;
            description = L"Very high warning rate (" + 
                         std::to_wstring(warningCount) + L" warnings in " + 
                         std::to_wstring(scanDays) + L" days)";
            addFlag(result, L"WARNING_RATE_SEVERE", description,
                    HealthFlagSeverity::Warning, penalty);
        } else if (warningPerDay >= 1.0) {
            // 1+ warning per day - significant issue (Warning severity)
            penalty = PENALTY_CAUTION_HIGH;
            description = L"High warning rate (" + 
                         std::to_wstring(warningCount) + L" warnings in " + 
                         std::to_wstring(scanDays) + L" days)";
            addFlag(result, L"WARNING_RATE_HIGH", description,
                    HealthFlagSeverity::Warning, penalty);
        } else if (warningPerDay >= 0.2) {
            // 0.2+ warnings per day (6+ per month) - moderate concern (Warning severity)
            penalty = PENALTY_CAUTION_MED;
            description = L"Moderate warning rate (" + 
                         std::to_wstring(warningCount) + L" warnings in " + 
                         std::to_wstring(scanDays) + L" days)";
            addFlag(result, L"WARNING_RATE_MODERATE", description,
                    HealthFlagSeverity::Warning, penalty);
        } else {
            // Very occasional warnings (Caution - less than 6 per month)
            penalty = PENALTY_CAUTION_LOW;
            description = L"Occasional warnings (" + 
                         std::to_wstring(warningCount) + L" warnings in " + 
                         std::to_wstring(scanDays) + L" days)";
            addFlag(result, L"WARNING_RATE_LOW", description,
                    HealthFlagSeverity::Caution, penalty);
        }
    }
}

// ============================================================================
// Helper Methods
// ============================================================================

bool HealthScoreEvaluator::hasFlag(const HealthResult& result, const std::wstring& flagId)
{
    for (const auto& flag : result.flags) {
        if (flag.id == flagId) {
            return true;
        }
    }
    return false;
}

void HealthScoreEvaluator::addFlag(HealthResult& result, const std::wstring& id,
                                   const std::wstring& description,
                                   HealthFlagSeverity severity, int penalty)
{
    HealthFlag flag;
    flag.id = id;
    flag.description = description;
    flag.severity = severity;
    flag.penalty = penalty;

    result.flags.push_back(flag);
    result.score += penalty;
}

bool HealthScoreEvaluator::isCriticalProblemCode(uint32_t problemCode)
{
    // User-disabled is intentional, not a critical problem
    return problemCode != CM_PROB_DISABLED;
}

std::wstring HealthScoreEvaluator::getProblemCodeDescription(uint32_t problemCode)
{
    switch (problemCode) {
        case CM_PROB_NOT_CONFIGURED:    return L"No driver installed for this device";
        case CM_PROB_OUT_OF_MEMORY:     return L"Out of memory";
        case CM_PROB_FAILED_START:      return L"Device failed to start";
        case CM_PROB_DEVICE_NOT_THERE:  return L"Device hardware not found";
        case CM_PROB_DRIVER_FAILED_LOAD: return L"Driver failed to load";
        case CM_PROB_DRIVER_BLOCKED:    return L"Driver is blocked";
        case CM_PROB_FAILED_POST_START: return L"Device failed after starting";
        case CM_PROB_DISABLED:          return L"Device is disabled";
        case CM_PROB_FAILED_INSTALL:    return L"Device installation failed";
        default:
            return L"Device has a problem (code: " + std::to_wstring(problemCode) + L")";
    }
}

bool HealthScoreEvaluator::isValidDriverDate(const std::optional<std::chrono::sys_days>& date)
{
    if (!date.has_value()) {
        return false;
    }

    auto ymd = std::chrono::year_month_day{*date};
    int year = static_cast<int>(ymd.year());
    unsigned month = static_cast<unsigned>(ymd.month());
    unsigned day = static_cast<unsigned>(ymd.day());

    // Basic range check
    if (year < 2001 || year > 2100) {
        return false;
    }

    // Known placeholder dates
    // Microsoft: 2006-06-21, 2006-06-01
    if (year == 2006 && month == 6 && (day == 21 || day == 1)) {
        return false;
    }

    // Intel: 2009-04-21
    if (year == 2009 && month == 4 && day == 21) {
        return false;
    }

    return true;
}