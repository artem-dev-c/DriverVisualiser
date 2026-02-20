#include "ClassNameMapper.h"
#include <fstream>
#include <sstream>
#include <string>

// ============================================================================
// Static Member Definitions
// ============================================================================

std::unordered_map<std::wstring, std::wstring> ClassNameMapper::s_mappings;
bool ClassNameMapper::s_initialized = false;

// ============================================================================
// Public API
// ============================================================================

bool ClassNameMapper::initialize(const std::wstring& jsonPath)
{
    s_mappings.clear();
    s_initialized = false;

    // MinGW's ifstream only accepts narrow paths — convert wstring to string first
    std::string narrowPath(jsonPath.begin(), jsonPath.end());
    std::ifstream file(narrowPath);
    if (!file.is_open()) {
        return false;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string narrow = ss.str();

    if (narrow.empty()) {
        return false;
    }

    // Convert narrow to wstring for parsing (JSON is ASCII-safe)
    std::wstring content(narrow.begin(), narrow.end());

    s_initialized = parseJson(content);
    return s_initialized;
}

std::wstring ClassNameMapper::getDisplayName(const std::wstring& rawClassName)
{
    if (rawClassName.empty()) {
        return L"Unknown";
    }

    if (!s_initialized || s_mappings.empty()) {
        return rawClassName;
    }

    auto it = s_mappings.find(rawClassName);
    if (it != s_mappings.end()) {
        return it->second;
    }

    return rawClassName;
}

bool ClassNameMapper::isInitialized()
{
    return s_initialized;
}

// ============================================================================
// JSON Parsing
// ============================================================================
// Simple line-by-line parser for our specific JSON structure.
// Avoids pulling in nlohmann/json or similar as a dependency.
// Format expected: "key": "value" pairs inside "deviceClasses": { }
// ============================================================================

bool ClassNameMapper::parseJson(const std::wstring& content)
{
    const std::wstring sectionKey = L"\"deviceClasses\"";
    size_t sectionPos = content.find(sectionKey);
    if (sectionPos == std::wstring::npos) {
        return false;
    }

    size_t braceOpen = content.find(L'{', sectionPos + sectionKey.size());
    if (braceOpen == std::wstring::npos) {
        return false;
    }

    size_t braceClose = content.find(L'}', braceOpen + 1);
    if (braceClose == std::wstring::npos) {
        return false;
    }

    std::wstring block = content.substr(braceOpen + 1, braceClose - braceOpen - 1);
    std::wistringstream stream(block);
    std::wstring line;

    while (std::getline(stream, line)) {
        auto trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(L" \t\r\n"));
        if (trimmed.empty() || trimmed[0] == L'/' || trimmed[0] == L'}') {
            continue;
        }

        size_t colonPos = trimmed.find(L':');
        if (colonPos == std::wstring::npos) {
            continue;
        }

        std::wstring key   = trimJsonString(trimmed.substr(0, colonPos));
        std::wstring value = trimJsonString(trimmed.substr(colonPos + 1));

        if (!key.empty() && !value.empty()) {
            s_mappings[key] = value;
        }
    }

    return !s_mappings.empty();
}

std::wstring ClassNameMapper::trimJsonString(const std::wstring& s)
{
    size_t start = s.find(L'"');
    if (start == std::wstring::npos) {
        return L"";
    }
    start++;

    size_t end = s.find(L'"', start);
    if (end == std::wstring::npos) {
        return L"";
    }

    return s.substr(start, end - start);
}