#pragma once

#include "IconProvider.h"
#include <QString>

/// ============================================================================
/// CategoryIconMapper
/// ============================================================================
/// Maps device class names to appropriate category icons.
/// Provides a centralized mapping from Windows device classes to visual icons.
/// ============================================================================

class CategoryIconMapper
{
public:
    /// Get the appropriate icon for a device category
    /// @param className The internal Windows class name (e.g., "Display", "Net")
    /// @return IconType enum value for the appropriate icon
    static IconProvider::IconType iconForCategory(const QString& className);
};
