#pragma once

#include <vector>
#include <string>
#include "DriverInfo.h"
#include "HealthFlag.h"

// ============================================================================
// System-level health flag IDs (distinct from per-driver HealthFlag IDs)
// ============================================================================

/// Aggregated issue entry for dashboard display
/// Represents a single real problem worth surfacing to the user
struct DashboardIssue {
    std::wstring driverName;          ///< Display name of the affected driver
    std::wstring driverInstanceId;    ///< For future scroll-to support
    std::wstring categoryName;        ///< Device class name (e.g. "HIDClass")
    std::wstring flagId;              ///< Original HealthFlag ID (e.g. "DRIVER_BLOCKED")
    std::wstring description;         ///< Human-readable description of the issue
    HealthFlagSeverity severity;      ///< Critical or Warning
    bool isUserDisabled = false;      ///< True if CM_PROB_DISABLED - shown differently
};

/// System-level health flag (analogous to per-driver HealthFlag)
struct SystemHealthFlag {
    std::wstring id;              ///< e.g. "CRITICAL_DRIVERS_PRESENT"
    std::wstring description;     ///< e.g. "3 drivers have critical failures"
    HealthFlagSeverity severity;
    int penalty;                  ///< Negative value applied to system score
};

/// Complete system health result computed from all drivers
struct SystemHealthResult {
    int score = 100;                          ///< Final system score 0-100

    // Driver counts
    int totalDrivers = 0;
    int criticalDrivers = 0;                  ///< Health < 70 (excluding user-disabled)
    int warningDrivers = 0;                   ///< Health 70-89
    int healthyDrivers = 0;                   ///< Health >= 90

    // System-level flags applied during scoring
    std::vector<SystemHealthFlag> systemFlags;

    // Filtered issue list for dashboard display (real issues only)
    std::vector<DashboardIssue> issues;
};

// ============================================================================
// SystemHealthSummary
// ============================================================================

/**
 * @class SystemHealthSummary
 * @brief Computes system-wide health score and extracts real issues for the dashboard.
 *
 * Scoring methodology:
 *   1. Weighted average of per-driver health scores (Virtual devices excluded)
 *      - Critical device weight: 4x
 *      - Important device weight: 2x
 *      - Optional device weight: 1x
 *   2. System-level penalty flags applied on top
 *
 * Issue filtering (for DashboardIssue list):
 *   Only flags considered "real" actionable problems are surfaced:
 *   DRIVER_BLOCKED, PROBLEM_CODE_CRITICAL, UNSIGNED_DRIVER,
 *   NEEDS_RESTART, DEVICE_DISCONNECTED, USER_DISABLED (greyed)
 */
class SystemHealthSummary
{
public:
    /// Compute full system health result from all scanned drivers
    static SystemHealthResult compute(const std::vector<DriverInfo>& drivers);

private:
    /// Compute weighted average score from non-virtual drivers
    static int computeWeightedScore(const std::vector<DriverInfo>& drivers);

    /// Apply system-level penalty flags and return adjusted score
    static int applySystemFlags(int baseScore,
                                const std::vector<DriverInfo>& drivers,
                                SystemHealthResult& result);

    /// Extract actionable issues from all drivers
    static std::vector<DashboardIssue> extractIssues(
        const std::vector<DriverInfo>& drivers);

    /// Returns true if this flag ID should be shown in the dashboard issue list
    static bool isActionableFlag(const std::wstring& flagId);

    /// Returns the weight for a given importance level (Virtual = 0)
    static int importanceWeight(DriverImportance importance);
};
