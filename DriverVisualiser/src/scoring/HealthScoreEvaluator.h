#pragma once

#include "HealthFlag.h"
#include "DriverInfo.h"

class HealthScoreEvaluator {
public:
    /// Evaluates driver health and returns score + flags
    static HealthResult evaluate(const DriverInfo& driver);

private:
    /// Check Tier 1: Critical flags
    static void evaluateCriticalFlags(const DriverInfo& driver, HealthResult& result);
    
    /// Check Tier 2: Warning flags
    static void evaluateWarningFlags(const DriverInfo& driver, HealthResult& result);
    
    /// Check Tier 3: Caution flags
    static void evaluateCautionFlags(const DriverInfo& driver, HealthResult& result);
    
    /// Check Tier 4: Info flags (no penalty)
    static void evaluateInfoFlags(const DriverInfo& driver, HealthResult& result);
    
    /// Helper: Check if a flag ID already exists (for overlap prevention)
    static bool hasFlag(const HealthResult& result, const std::wstring& flagId);
    
    /// Helper: Add a flag to result
    static void addFlag(HealthResult& result, 
                        const std::wstring& id,
                        const std::wstring& description,
                        HealthFlagSeverity severity,
                        int penalty);
    
    /// Helper: Check if problem code is a real error (not user-disabled)
    static bool isCriticalProblemCode(uint32_t problemCode);
    
    /// Helper: Get human-readable description for problem code
    static std::wstring getProblemCodeDescription(uint32_t problemCode);
};