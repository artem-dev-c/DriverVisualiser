#include "CategoryIconMapper.h"

IconProvider::IconType CategoryIconMapper::iconForCategory(const QString& className)
{
    // Display & Graphics
    if (className == "Display") return IconProvider::DeviceDesktop;
    if (className == "Monitor") return IconProvider::DeviceDesktop;
    
    // Network
    if (className == "Net") return IconProvider::Network;
    if (className == "NetClient") return IconProvider::Users;
    if (className == "NetService") return IconProvider::Cloud;
    if (className == "NetTrans") return IconProvider::Code;
    
    // Audio & Media
    if (className == "Media" || className == "MEDIA") return IconProvider::Volume;
    if (className == "Sound") return IconProvider::Volume;
    if (className == "AudioEndpoint") return IconProvider::Volume;
    if (className == "AudioProcessingObject") return IconProvider::Volume;
    
    // Storage
    if (className == "DiskDrive") return IconProvider::DeviceFloppy;
    if (className == "CDROM") return IconProvider::Disc;
    if (className == "SCSIAdapter") return IconProvider::DeviceFloppy;
    if (className == "HDC") return IconProvider::DeviceFloppy;
    if (className == "FloppyDisk") return IconProvider::DeviceFloppy;
    if (className == "TapeDrive") return IconProvider::Archive;
    if (className == "Volume") return IconProvider::Folders;
    if (className == "VolumeSnapshot") return IconProvider::Folders;
    if (className == "MediumChanger") return IconProvider::Disc;
    
    // USB
    if (className == "USB") return IconProvider::Usb;
    if (className == "USBDevice") return IconProvider::Usb;
    if (className == "USBPRINT") return IconProvider::Usb;
    if (className == "UCM") return IconProvider::Usb;
    
    // Input Devices
    if (className == "Keyboard") return IconProvider::Keyboard;
    if (className == "Mouse") return IconProvider::Mouse;
    if (className == "HIDClass") return IconProvider::Mouse;
    
    // Processors & System
    if (className == "Processor") return IconProvider::Cpu;
    if (className == "Computer") return IconProvider::DeviceLaptop;
    if (className == "System") return IconProvider::DeviceLaptop;
    
    // Imaging
    if (className == "Camera") return IconProvider::Camera;
    if (className == "Image") return IconProvider::Camera;
    
    // Printing
    if (className == "Printer") return IconProvider::Printer;
    if (className == "PnpPrinters") return IconProvider::Printer;
    if (className == "PrintQueue") return IconProvider::Printer;
    if (className == "Dot4Print") return IconProvider::Printer;
    
    // Wireless & Connectivity
    if (className == "Bluetooth") return IconProvider::Bluetooth;
    if (className == "BluetootheLE") return IconProvider::Bluetooth;
    if (className == "Infrared") return IconProvider::AntennaBars;
    if (className == "Modem") return IconProvider::Router;
    
    // Power & Battery
    if (className == "Battery") return IconProvider::Battery;
    
    // Security & Biometric
    if (className == "Biometric") return IconProvider::Fingerprint;
    if (className == "SecurityDevices") return IconProvider::ShieldLock;
    if (className == "SmartCard") return IconProvider::CreditCard;
    if (className == "SmartCardReader") return IconProvider::CreditCard;
    
    // Sensors & Location
    if (className == "Sensor") return IconProvider::Radar;
    if (className == "GPS") return IconProvider::Gps;
    if (className == "Proximity") return IconProvider::Radar;
    
    // Ports & Connectors
    if (className == "Ports") return IconProvider::Plug;
    if (className == "PCIEBusConnector") return IconProvider::Link;
    if (className == "PCMCIA") return IconProvider::CreditCard;
    if (className == "1394") return IconProvider::Link;
    if (className == "Enum1394") return IconProvider::Link;
    if (className == "SBP2") return IconProvider::Link;
    if (className == "61883") return IconProvider::Link;
    if (className == "AVC") return IconProvider::Link;
    if (className == "Dot4") return IconProvider::Link;
    if (className == "LPTENUM") return IconProvider::Plug;
    if (className == "MultiPortSerial") return IconProvider::Plug;
    
    // File System Filters
    if (className.startsWith("FsFilter-")) return IconProvider::Filter;
    if (className == "FsFilter-AntiVirus") return IconProvider::Filter;
    if (className == "FsFilter-Encryption") return IconProvider::LockAccess;
    
    // Software & Firmware
    if (className == "Firmware") return IconProvider::FileCode;
    if (className == "SoftwareComponent") return IconProvider::Components;
    if (className == "SoftwareDevice") return IconProvider::Components;
    
    // Multifunction & Misc
    if (className == "Multifunction") return IconProvider::Stack;
    if (className == "Extension") return IconProvider::Components;
    
    // Portable Devices
    if (className == "WPD") return IconProvider::DeviceMobile;
    if (className == "WPDBUSENUM") return IconProvider::DeviceMobile;
    if (className == "WindowsCE") return IconProvider::DeviceMobile;
    if (className == "WCEUSBSHClass") return IconProvider::DeviceMobile;
    
    // Storage Technologies
    if (className == "MemoryTechnology") return IconProvider::DeviceFloppy;
    if (className == "MTD") return IconProvider::DeviceFloppy;
    if (className == "EhStorClas") return IconProvider::ShieldLock;
    
    // Fallback Cases
    if (className == "Unknown") return IconProvider::QuestionMark;
    if (className == "NoDriver") return IconProvider::AlertTriangle;
    if (className == "LegacyDriver") return IconProvider::Clock;
    
    // Default fallback - use laptop/device icon
    return IconProvider::DeviceLaptop;
}
