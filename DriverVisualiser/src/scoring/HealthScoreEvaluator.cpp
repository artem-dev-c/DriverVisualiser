#include "HealthScoreEvaluator.h"
#include <algorithm>
#include <chrono>
#include <windows.h>
#include <cfgmgr32.h>

HealthResult HealthScoreEvaluator::evaluate(const DriverInfo& driver)
{
    HealthResult result;
    result.score = 100;
    
    // Evaluate all tiers
    evaluateCriticalFlags(driver, result);
    evaluateWarningFlags(driver, result);
    evaluateCautionFlags(driver, result);
    evaluateInfoFlags(driver, result);
    
    // Floor at 0
    result.score = std::max(0, result.score);
    
    return result;
}

void HealthScoreEvaluator::evaluateCriticalFlags(const DriverInfo& driver, HealthResult& result)
{
    // DRIVER_BLOCKED - highest severity (from rawStatus)
    if (driver.rawStatus & DN_DRIVER_BLOCKED) {
        addFlag(result,
                L"DRIVER_BLOCKED",
                L"Driver was blocked by Windows",
                HealthFlagSeverity::Critical,
                -40);
    }
    
    // PROBLEM_CODE_CRITICAL - real errors (not user-disabled)
    // This is our PRIMARY indicator of actual issues
    if (driver.problemCode != 0 && isCriticalProblemCode(driver.problemCode)) {
        addFlag(result, 
                L"PROBLEM_CODE_CRITICAL",
                getProblemCodeDescription(driver.problemCode),
                HealthFlagSeverity::Critical,
                -40);
    }
}

void HealthScoreEvaluator::evaluateWarningFlags(const DriverInfo& driver, HealthResult& result)
{
    // UNSIGNED_DRIVER
    if (!driver.isSigned) {
        addFlag(result,
                L"UNSIGNED_DRIVER",
                L"Driver is not digitally signed",
                HealthFlagSeverity::Warning,
                -15);
    }
    
    // GHOST_DEVICE - not physically present
    if (!driver.isPresent) {
        addFlag(result,
                L"GHOST_DEVICE",
                L"Device is not physically present",
                HealthFlagSeverity::Warning,
                -10);
    }
    
    // NEEDS_RESTART - pending reboot (from rawStatus)
    if (driver.rawStatus & DN_NEED_RESTART) {
        addFlag(result,
                L"NEEDS_RESTART",
                L"System restart required to complete driver operation",
                HealthFlagSeverity::Warning,
                -10);
    }
    
    // DEVICE_DISCONNECTED (from rawStatus)
    // Only flag if not already flagged as ghost
    if ((driver.rawStatus & DN_DEVICE_DISCONNECTED) && !hasFlag(result, L"GHOST_DEVICE")) {
        addFlag(result,
                L"DEVICE_DISCONNECTED",
                L"Device is disconnected",
                HealthFlagSeverity::Warning,
                -10);
    }
    
    // OUTDATED_DRIVER - driver date > 3 years old
    if (driver.driverDate.has_value()) {
        auto now = std::chrono::system_clock::now();
        auto nowDays = std::chrono::floor<std::chrono::days>(now);
        auto driverAge = nowDays - *driver.driverDate;
        
        // 3 years = ~1095 days
        if (driverAge.count() > 1095) {
            addFlag(result,
                    L"OUTDATED_DRIVER",
                    L"Driver is more than 3 years old",
                    HealthFlagSeverity::Warning,
                    -10);
        }
    }
}

void HealthScoreEvaluator::evaluateCautionFlags(const DriverInfo& driver, HealthResult& result)
{
    // NO_VERSION_INFO
    if (!driver.version.hasVersion) {
        addFlag(result,
                L"NO_VERSION_INFO",
                L"No version information available",
                HealthFlagSeverity::Caution,
                -5);
    }
    
    // NO_DRIVER_DATE
    if (!driver.driverDate.has_value()) {
        addFlag(result,
                L"NO_DRIVER_DATE",
                L"No driver date information available",
                HealthFlagSeverity::Caution,
                -3);
    }
    
    // UNKNOWN_MANUFACTURER
    if (driver.manufacturer == L"Unknown" || driver.manufacturer.empty()) {
        addFlag(result,
                L"UNKNOWN_MANUFACTURER",
                L"Manufacturer information is missing",
                HealthFlagSeverity::Caution,
                -2);
    }
}

void HealthScoreEvaluator::evaluateInfoFlags(const DriverInfo& driver, HealthResult& result)
{
    // USER_DISABLED - user intentionally disabled (not a health issue)
    if (driver.problemCode == CM_PROB_DISABLED) {
        addFlag(result,
                L"USER_DISABLED",
                L"Device was disabled by user",
                HealthFlagSeverity::Info,
                0);
    }
    
    // NOT_STARTED_BY_DESIGN - device not started but no problem code
    // This is normal for resource/stub devices like "Motherboard resources"
    if (driver.status == DriverStatus::NotStarted && driver.problemCode == 0) {
        addFlag(result,
                L"NOT_STARTED_BY_DESIGN",
                L"Device is not started (normal for resource devices)",
                HealthFlagSeverity::Info,
                0);
    }
    
    // DRIVER_NOT_LOADED_BY_DESIGN - driver not loaded but no problem
    // Similar to above - some devices don't need active drivers
    if (!(driver.rawStatus & DN_DRIVER_LOADED) && driver.problemCode == 0) {
        // Don't duplicate if already flagged as not started
        if (!hasFlag(result, L"NOT_STARTED_BY_DESIGN")) {
            addFlag(result,
                    L"DRIVER_NOT_LOADED_BY_DESIGN",
                    L"No driver loaded (normal for some devices)",
                    HealthFlagSeverity::Info,
                    0);
        }
    }
    
    // SYSTEM_CRITICAL - cannot be disabled (from rawStatus)
    if (!(driver.rawStatus & DN_DISABLEABLE)) {
        addFlag(result,
                L"SYSTEM_CRITICAL",
                L"System-critical device (cannot be disabled)",
                HealthFlagSeverity::Info,
                0);
    }
    
    // VIRTUAL_DEVICE - software device, not physical hardware
    if (!driver.hardwareId.empty()) {
        std::wstring hwid = driver.hardwareId;
        if (hwid.find(L"SWD\\") == 0 || 
            hwid.find(L"SW\\") == 0 ||
            hwid.find(L"ROOT\\") == 0 ||
            hwid.find(L"UMB\\") == 0) {
            addFlag(result,
                    L"VIRTUAL_DEVICE",
                    L"This is a virtual/software device",
                    HealthFlagSeverity::Info,
                    0);
        }
    }
    
    // GENERIC_DRIVER - Microsoft inbox driver
    if (driver.provider.find(L"Microsoft") != std::wstring::npos ||
        driver.manufacturer.find(L"Microsoft") != std::wstring::npos) {
        addFlag(result,
                L"GENERIC_DRIVER",
                L"Using Microsoft generic driver",
                HealthFlagSeverity::Info,
                0);
    }
}

bool HealthScoreEvaluator::hasFlag(const HealthResult& result, const std::wstring& flagId)
{
    for (const auto& flag : result.flags) {
        if (flag.id == flagId) {
            return true;
        }
    }
    return false;
}

void HealthScoreEvaluator::addFlag(HealthResult& result,
                                   const std::wstring& id,
                                   const std::wstring& description,
                                   HealthFlagSeverity severity,
                                   int penalty)
{
    HealthFlag flag;
    flag.id = id;
    flag.description = description;
    flag.severity = severity;
    flag.penalty = penalty;
    
    result.flags.push_back(flag);
    result.score += penalty;  // penalty is negative, so this subtracts
}

bool HealthScoreEvaluator::isCriticalProblemCode(uint32_t problemCode)
{
    // User-disabled is NOT a critical problem - it's intentional
    if (problemCode == CM_PROB_DISABLED) {
        return false;
    }
    
    // All other non-zero problem codes indicate real issues
    return true;
}

std::wstring HealthScoreEvaluator::getProblemCodeDescription(uint32_t problemCode)
{
    // Map common problem codes to human-readable descriptions
    switch (problemCode) {
        case CM_PROB_NOT_CONFIGURED:       // 1
            return L"No driver installed for this device";
        case CM_PROB_OUT_OF_MEMORY:        // 3
            return L"Out of memory";
        case CM_PROB_FAILED_START:         // 10
            return L"Device failed to start";
        case CM_PROB_DEVICE_NOT_THERE:     // 24
            return L"Device hardware not found";
        case CM_PROB_DRIVER_FAILED_LOAD:   // 39
            return L"Driver failed to load";
        case CM_PROB_DRIVER_BLOCKED:       // 48
            return L"Driver is blocked";
        case CM_PROB_FAILED_POST_START:    // 43
            return L"Device failed after starting";
        case CM_PROB_DISABLED:             // 22
            return L"Device is disabled";
        case CM_PROB_FAILED_INSTALL:       // 28
            return L"Device installation failed";
        default:
            return L"Device has a problem (code: " + std::to_wstring(problemCode) + L")";
    }
}