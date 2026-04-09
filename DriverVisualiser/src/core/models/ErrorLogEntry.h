#pragma once

#include <string>
#include <chrono>
#include <optional>

/// ============================================================================
/// ErrorLogEntry
/// ============================================================================
/// Represents a single driver-related event from Windows Event Log.
/// Events are captured from System log and Configuration logs, then
/// classified and routed to appropriate UI components.
///
/// Event Sources:
///   - System log: Errors, warnings, and whitelisted lifecycle events
///   - Configuration log (Kernel-PnP): Lifecycle information events
///
/// Classification:
///   - DeviceMapped: Has DeviceInstanceId → shown in driver card + timeline
///   - SystemWide: No DeviceInstanceId → shown in timeline only
///   - SwdBackground: SWD\ software device → low-priority, collapsed UI section
/// ============================================================================
struct ErrorLogEntry {
    // ═══════════════════════════════════════════════════════════════════════
    // Core Event Data (Always Present)
    // ═══════════════════════════════════════════════════════════════════════

    std::chrono::system_clock::time_point timestamp;  ///< When the event occurred
    int eventId = 0;                                  ///< Event ID (provider-scoped, not globally unique)
    std::wstring level;                               ///< "Critical", "Error", "Warning", "Information"
    std::wstring provider;                            ///< Provider name (e.g., "Microsoft-Windows-Kernel-PnP", "nvlddmkm")
    std::wstring message;                             ///< Full event message text (extracted from XML or synthesized)

    // ═══════════════════════════════════════════════════════════════════════
    // Device Matching (Optional - for device-mapped events)
    // ═══════════════════════════════════════════════════════════════════════

    std::optional<std::wstring> deviceInstanceId;    ///< Device Instance ID for matching to driver cards

    // ── Device display name fields (priority: friendlyName > infName > deviceInstanceId) ──

    /// Human-readable device name resolved from Kernel-PnP XML or event data.
    /// Examples: "SteelSeries Aerox 5 Wireless", "HID Keyboard Device", "Intel Wi-Fi 6 AX200"
    /// This is what should be shown in the UI as the primary device label.
    std::optional<std::wstring> friendlyName;

    /// INF/driver filename from the DriverName XML field (e.g., "oem199.inf", "keyboard.inf").
    /// Used as secondary display fallback when friendlyName is absent.
    /// Also stored for lifecycle metadata (Event 400/410/430).
    std::optional<std::wstring> infName;

    // ═══════════════════════════════════════════════════════════════════════
    // Error Context (Optional - populated for error/warning events)
    // ═══════════════════════════════════════════════════════════════════════

    std::optional<uint32_t> statusCode;              ///< NTSTATUS or Win32 error code if available
    std::optional<std::wstring> failureReason;       ///< Extracted failure reason (e.g., HostProblem from UMDF crash)

    // ═══════════════════════════════════════════════════════════════════════
    // Lifecycle Metadata (Optional - Event 400, 420, 430, 431)
    // ═══════════════════════════════════════════════════════════════════════

    std::optional<std::wstring> driverVersion;       ///< Driver version (e.g., "32.0.15.6614")
    std::optional<std::wstring> driverProvider;      ///< Provider/manufacturer (e.g., "NVIDIA Corporation")
    std::optional<bool> isDriverUpdate;              ///< true = update, false = fresh install (from DeviceUpdated field in Event 400)

    // ═══════════════════════════════════════════════════════════════════════
    // Event Classification & UI Routing
    // ═══════════════════════════════════════════════════════════════════════

    /// Event category - determines where in UI the event appears
    enum class Category {
        DeviceMapped,     ///< Has DeviceInstanceId → show in driver card + timeline + DayEventsPopup main section
        SystemWide,       ///< No DeviceInstanceId → show in timeline + DayEventsPopup system section only
        SwdBackground     ///< SWD\ software device → low priority, shown in collapsed section
    };
    Category category = Category::DeviceMapped;

    /// UI visibility control - allows storage without display
    /// Example: Event 410 (device configured) is stored for completeness but hidden from UI
    bool isVisibleInUi = true;

    // ═══════════════════════════════════════════════════════════════════════
    // Display Helpers
    // ═══════════════════════════════════════════════════════════════════════

    /// Returns the best available display name for UI, applying priority:
    ///   1. friendlyName  — human-readable name (e.g., "SteelSeries Aerox 5 Wireless")
    ///   2. infName       — INF filename (e.g., "keyboard.inf")
    ///   3. deviceInstanceId — raw hardware ID as last resort
    /// Returns empty string for SystemWide events (no device association).
    std::wstring displayName() const
    {
        if (friendlyName.has_value() && !friendlyName.value().empty())
            return friendlyName.value();
        if (infName.has_value() && !infName.value().empty())
            return infName.value();
        if (deviceInstanceId.has_value() && !deviceInstanceId.value().empty())
            return deviceInstanceId.value();
        return L"";
    }
};