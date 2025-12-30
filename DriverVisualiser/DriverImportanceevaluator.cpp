#include "DriverImportanceEvaluator.h"

// Early implementation of driver importance evaluation logic

DriverImportance DriverImportanceEvaluator::evaluate(const DriverInfo& d)
{
    
    if (d.deviceClassName == L"System" ||
        d.deviceClassName == L"Processor" ||
        d.deviceClassName == L"Motherboard")
        return DriverImportance::Critical;

    
    if (d.status == DriverStatus::Error)
        return DriverImportance::Critical;

    
    if (d.provider == L"Microsoft")
        return DriverImportance::Important;

    
    return DriverImportance::Optional;
}

QString DriverImportanceEvaluator::importanceToString(DriverImportance level) {
    switch(level) {
        case DriverImportance::Critical:  return "Critical";
        case DriverImportance::Important: return "Important";
        case DriverImportance::Optional:  return "Optional";
        default:                          return "Unknown";
    }
}
