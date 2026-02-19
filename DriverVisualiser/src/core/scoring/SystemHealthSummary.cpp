#include "SystemHealthSummary.h"
#include <algorithm>
#include <numeric>

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
        L"USER_DISABLED"
    };
}

// ============================================================================
// Public API
// ============================================================================

SystemHealthResult SystemHealthSummary::compute(const std::vector<DriverInfo>& drivers)
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
    int finalScore = applySystemFlags(baseScore, drivers, result);

    result.score = std::clamp(finalScore, 0, 100);

    // === Step 3: Extract actionable issues for display ===
    result.issues = extractIssues(drivers);

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

    return score;
}

// ============================================================================
// Issue Extraction
// ============================================================================

std::vector<DashboardIssue> SystemHealthSummary::extractIssues(
    const std::vector<DriverInfo>& drivers)
{
    std::vector<DashboardIssue> issues;

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
