#include "DriverImportanceFormatter.h"



QString DriverImportanceFormatter::importanceToString(DriverImportance level) {
    switch(level) {
        case DriverImportance::Critical:  return "Critical";
        case DriverImportance::Important: return "Important";
        case DriverImportance::Optional:  return "Optional";
        default:                          return "Unknown";
    }
}
