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

    constexpr int OUTDATED_THRESHOLD_DAYS = 1095;  // ~3 years
}

// ============================================================================
// Main Evaluation
// ============================================================================

HealthResult HealthScoreEvaluator::evaluate(const DriverInfo& driver)
{
    HealthResult result;
    result.score = 100;

    evaluateCriticalFlags(driver, result);
    evaluateWarningFlags(driver, result);
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
        addFlag(result, L"GHOST_DEVICE", L"Device is not physically present",
                HealthFlagSeverity::Warning, PENALTY_WARNING);
    }

    // Needs restart
    if (driver.rawStatus & DN_NEED_RESTART) {
        addFlag(result, L"NEEDS_RESTART", L"System restart required - driver may not function correctly until reboot",
                HealthFlagSeverity::Warning, PENALTY_WARNING_HIGH);
    }

    // Device disconnected (only if not already flagged as ghost)
    if ((driver.rawStatus & DN_DEVICE_DISCONNECTED) && !hasFlag(result, L"GHOST_DEVICE")) {
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

    if (age.count() > OUTDATED_THRESHOLD_DAYS) {
        std::wstring description = usingInstallDate
            ? L"Driver is at least 3 years old (based on install date)"
            : L"Driver is more than 3 years old";

        addFlag(result, L"OUTDATED_DRIVER", description,
                HealthFlagSeverity::Warning, PENALTY_WARNING);
    }
}

// ============================================================================
// Tier 3: Caution Flags
// ============================================================================

void HealthScoreEvaluator::evaluateCautionFlags(const DriverInfo& driver, HealthResult& result)
{
    // No version info
    if (!driver.version.hasVersion) {
        addFlag(result, L"NO_VERSION_INFO", L"No version information available",
                HealthFlagSeverity::Caution, PENALTY_CAUTION_HIGH);
    }

    // No driver date
    if (!driver.driverDate.has_value()) {
        addFlag(result, L"NO_DRIVER_DATE", L"No driver date information available",
                HealthFlagSeverity::Caution, PENALTY_CAUTION_MED);
    }

    // Unknown manufacturer
    if (driver.manufacturer == L"Unknown" || driver.manufacturer.empty()) {
        addFlag(result, L"UNKNOWN_MANUFACTURER", L"Manufacturer information is missing",
                HealthFlagSeverity::Caution, PENALTY_CAUTION_LOW);
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