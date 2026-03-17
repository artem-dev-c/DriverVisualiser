#pragma once

#include "HealthFlag.h"
#include "DriverInfo.h"

/**
 * @class HealthScoreEvaluator
 * @brief Evaluates driver health based on various indicators.
 *
 * Analyzes driver properties and status to produce a health score (0-100)
 * and a list of detected flags categorized by severity.
 *
 * Scoring tiers:
 *   - Critical: Severe issues (blocked, failed) -> -40 penalty
 *   - Warning:  Moderate concerns (unsigned, outdated) -> -10 to -15 penalty
 *   - Caution:  Minor issues (missing info) -> -2 to -5 penalty
 *   - Info:     Informational only -> no penalty
 */
class HealthScoreEvaluator {
public:
    /// Evaluates driver health and returns score + flags
    static HealthResult evaluate(const DriverInfo& driver);

private:
    /// Tier 1: Critical issues (blocked drivers, problem codes)
    static void evaluateCriticalFlags(const DriverInfo& driver, HealthResult& result);

    /// Tier 2: Warning issues (unsigned, outdated, disconnected)
    static void evaluateWarningFlags(const DriverInfo& driver, HealthResult& result);

    /// Helper for evaluateWarningFlags: checks driver age
    static void evaluateOutdatedDriver(const DriverInfo& driver, HealthResult& result);

    /// Tier 3: Caution issues (missing metadata)
    static void evaluateCautionFlags(const DriverInfo& driver, HealthResult& result);

    /// Tier 4: Info flags (no penalty, informational)
    static void evaluateInfoFlags(const DriverInfo& driver, HealthResult& result);

    /// Tier 5: Error log frequency evaluation (penalize based on error rate)
    static void evaluateErrorLogFrequency(const DriverInfo& driver, HealthResult& result);

    // --- Helpers ---

    /// Checks if a flag already exists (for overlap prevention)
    static bool hasFlag(const HealthResult& result, const std::wstring& flagId);

    /// Adds a flag and applies its penalty
    static void addFlag(HealthResult& result, const std::wstring& id, const std::wstring& description,
                        HealthFlagSeverity severity, int penalty);

    /// Returns true if problem code indicates a real error (not user-disabled)
    static bool isCriticalProblemCode(uint32_t problemCode);

    /// Converts problem code to human-readable description
    static std::wstring getProblemCodeDescription(uint32_t problemCode);

    /// Validates if a driver date is real (not a placeholder)
    static bool isValidDriverDate(const std::optional<std::chrono::sys_days>& date);
};