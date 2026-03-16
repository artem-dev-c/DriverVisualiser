#pragma once

#include <QIcon>
#include <QString>
#include <QMap>

/// ============================================================================
/// IconProvider
/// ============================================================================
/// Centralized provider for Tabler SVG icons.
/// Loads icons from Qt resources and provides them as QIcon objects with
/// optional color replacement for theme integration.
///
/// Usage:
///   QIcon icon = IconProvider::icon(IconProvider::ChevronDown);
///   button->setIcon(icon);
///
/// Or with custom color:
///   QIcon icon = IconProvider::icon(IconProvider::Copy, "#5dade2");
///   button->setIcon(icon);
/// ============================================================================

class IconProvider
{
public:
    /// Available icon types from Tabler icon set
    enum IconType {
        ChevronDown,        ///< Dropdown/expand indicator
        ChevronRight,       ///< Collapse/navigate indicator
        Copy,               ///< Copy to clipboard action
        FileDownload,       ///< Download file action
        FileTypeHtml,       ///< HTML file type indicator
        FileTypeTxt,        ///< Text file type indicator
        Folder,             ///< Folder/directory action
        RefreshDot,         ///< Refresh/rescan action
        Settings,           ///< Settings/configuration action
        SquareRoundedCheck, ///< Success/checkmark indicator
        InfoCircle          ///< Information/details indicator
    };

    /// Returns a QIcon for the specified icon type.
    /// Icons are cached after first load.
    /// @param type The icon type to load
    /// @param color Optional color override (default: uses "currentColor" from SVG)
    /// @param size Icon size in pixels (default: 24)
    static QIcon icon(IconType type, const QString& color = QString(), int size = 24);

    /// Returns the resource path for an icon type
    static QString iconPath(IconType type);

private:
    /// Internal cache: (type, color, size) -> QIcon
    static QMap<QString, QIcon> s_cache;

    /// Loads SVG from resources and optionally replaces colors
    static QIcon loadIcon(const QString& path, const QString& color, int size);

    /// Creates a cache key for the icon
    static QString cacheKey(IconType type, const QString& color, int size);
};