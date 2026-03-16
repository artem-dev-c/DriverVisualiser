#include "IconProvider.h"
#include <QFile>
#include <QSvgRenderer>
#include <QPainter>
#include <QPixmap>

// ── Static cache ──────────────────────────────────────────────────────────────

QMap<QString, QIcon> IconProvider::s_cache;

// ── Icon path mapping ─────────────────────────────────────────────────────────

QString IconProvider::iconPath(IconType type)
{
    switch (type) {
        case ChevronDown:        return ":/icons/chevron-down.svg";
        case ChevronRight:       return ":/icons/chevron-right.svg";
        case Copy:               return ":/icons/copy.svg";
        case FileDownload:       return ":/icons/file-download.svg";
        case FileTypeHtml:       return ":/icons/file-type-html.svg";
        case FileTypeTxt:        return ":/icons/file-type-txt.svg";
        case Folder:             return ":/icons/folder.svg";
        case RefreshDot:         return ":/icons/refresh-dot.svg";
        case Settings:           return ":/icons/settings.svg";
        case SquareRoundedCheck: return ":/icons/square-rounded-check.svg";
        case InfoCircle:         return ":/icons/info-circle.svg";
        default:                 return QString();
    }
}

// ── Cache key generation ──────────────────────────────────────────────────────

QString IconProvider::cacheKey(IconType type, const QString& color, int size)
{
    return QString("%1_%2_%3").arg(static_cast<int>(type)).arg(color).arg(size);
}

// ── Icon loading ──────────────────────────────────────────────────────────────

QIcon IconProvider::icon(IconType type, const QString& color, int size)
{
    QString key = cacheKey(type, color, size);
    
    // Return cached icon if available
    if (s_cache.contains(key)) {
        return s_cache[key];
    }
    
    // Load and cache the icon
    QString path = iconPath(type);
    QIcon loadedIcon = loadIcon(path, color, size);
    s_cache[key] = loadedIcon;
    
    return loadedIcon;
}

QIcon IconProvider::loadIcon(const QString& path, const QString& color, int size)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QIcon();  // Return empty icon on failure
    }
    
    QByteArray svgData = file.readAll();
    file.close();
    
    // Replace color if specified
    if (!color.isEmpty()) {
        // Replace both fill="currentColor" and stroke="currentColor"
        svgData.replace("currentColor", color.toUtf8());
    }
    
    // Create high-quality pixmaps at multiple resolutions for crisp rendering
    QIcon icon;
    
    // Generate at 1x, 1.5x, and 2x for different DPI settings
    QList<int> sizes = {size, qRound(size * 1.5), size * 2};
    
    for (int pixmapSize : sizes) {
        QSvgRenderer renderer(svgData);
        
        // Use higher resolution for smoother rendering
        QPixmap pixmap(pixmapSize, pixmapSize);
        pixmap.fill(Qt::transparent);
        
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        renderer.render(&painter);
        painter.end();
        
        // Set device pixel ratio for proper scaling
        pixmap.setDevicePixelRatio(static_cast<qreal>(pixmapSize) / size);
        icon.addPixmap(pixmap);
    }
    
    return icon;
}