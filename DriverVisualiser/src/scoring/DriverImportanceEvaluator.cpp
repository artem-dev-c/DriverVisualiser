#include "DriverImportanceEvaluator.h"

DriverImportance DriverImportanceEvaluator::evaluate(const DriverInfo& driver)
{
    const std::wstring& hwId = driver.hardwareId;
    
    // Tier 0: Virtual/Software devices (0/3)
    if (hwId.find(L"SWD\\") == 0 || 
        hwId.find(L"SW\\") == 0 || 
        hwId.find(L"ROOT\\") == 0 || 
        hwId.find(L"UMB\\") == 0) {
        return DriverImportance::Virtual;
    }
    
    // Tier 1: Critical system devices (3/3)
    if (hwId.find(L"ACPI\\") == 0 || 
        hwId.find(L"PCI\\") == 0 || 
        hwId.find(L"PCIROOT\\") == 0 || 
        hwId.find(L"IDE\\") == 0 || 
        hwId.find(L"SCSI\\") == 0) {
        return DriverImportance::Critical;
    }
    
    // Tier 2: Important hardware (2/3)
    if (hwId.find(L"USB\\") == 0 || 
        hwId.find(L"HDAUDIO\\") == 0 || 
        hwId.find(L"DISPLAY\\") == 0 || 
        hwId.find(L"STORAGE\\") == 0 || 
        hwId.find(L"SD\\") == 0 || 
        hwId.find(L"BTH\\") == 0 || 
        hwId.find(L"PCMCIA\\") == 0) {
        return DriverImportance::Important;
    }
    
    // Tier 3: Peripherals (1/3)
    if (hwId.find(L"HID\\") == 0 || 
        hwId.find(L"PRINTENUM\\") == 0) {
        return DriverImportance::Optional;
    }
    
    // Default: Optional
    return DriverImportance::Optional;
}
