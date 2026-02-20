#pragma once

#include <string>
#include <unordered_map>

/**
 * @class ClassNameMapper
 * @brief Maps raw Windows device class names to clean display names.
 *
 * Loads mappings from device_class_mappings.json at startup.
 * Raw class names are always preserved in DriverInfo/DeviceCategory::className.
 * This mapper is used ONLY at the presentation layer:
 *   - Section header labels
 *   - Detected issues dashboard flag
 *
 * NOT used in:
 *   - Troubleshoot reports (raw name preferred)
 *   - JSON/data exports
 *   - Info popup (raw name preferred)
 *   - Internal logs
 */
class ClassNameMapper {
public:
    /**
     * @brief Load mappings from JSON file. Call once at app startup.
     * @param jsonPath  Absolute path to device_class_mappings.json
     * @return true if loaded successfully, false if file missing or malformed
     */
    static bool initialize(const std::wstring& jsonPath);

    /**
     * @brief Get display name for a raw device class name.
     * @param rawClassName  e.g. L"HIDClass", L"Net", L"SCSIAdapter"
     * @return Mapped display name, or rawClassName as-is if not found (graceful fallback)
     */
    static std::wstring getDisplayName(const std::wstring& rawClassName);

    /**
     * @brief Check if the mapper has been successfully initialized.
     */
    static bool isInitialized();

private:
    static std::unordered_map<std::wstring, std::wstring> s_mappings;
    static bool s_initialized;

    /// Parse the JSON file manually (no external JSON dependency)
    static bool parseJson(const std::wstring& jsonContent);

    /// Trim whitespace and quotes from a JSON string value
    static std::wstring trimJsonString(const std::wstring& s);
};
