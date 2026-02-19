#pragma once

#include <string>
#include <vector>

enum class HealthFlagSeverity {
    Critical,   // Major problem, significant penalty
    Warning,    // Concerning, moderate penalty
    Caution,    // Minor issue, small penalty
    Info        // No penalty, informational only
};

struct HealthFlag {
    std::wstring id;              // e.g., "DRIVER_BLOCKED"
    std::wstring description;     // e.g., "Driver was blocked by Windows"
    HealthFlagSeverity severity;
    int penalty;                  // Negative value (0 for Info flags)
};

struct HealthResult {
    int score;                         // Final score 0-100
    std::vector<HealthFlag> flags;     // All detected flags (including Info)
};
