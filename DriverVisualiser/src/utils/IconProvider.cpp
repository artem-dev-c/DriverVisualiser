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
        // UI Icons
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
        
        // Category Icons - Hardware
        case DeviceDesktop:      return ":/icons/device-desktop.svg";
        case Network:            return ":/icons/network.svg";
        case Volume:             return ":/icons/volume.svg";
        case DeviceFloppy:       return ":/icons/device-floppy.svg";
        case Usb:                return ":/icons/usb.svg";
        case Cpu:                return ":/icons/cpu.svg";
        case Keyboard:           return ":/icons/keyboard.svg";
        case Mouse:              return ":/icons/mouse.svg";
        case Camera:             return ":/icons/camera.svg";
        case Printer:            return ":/icons/printer.svg";
        case Bluetooth:          return ":/icons/bluetooth.svg";
        case Battery:            return ":/icons/battery-1.svg";
        case Fingerprint:        return ":/icons/fingerprint.svg";
        case CreditCard:         return ":/icons/credit-card.svg";
        case Radar:              return ":/icons/radar.svg";
        case Gps:                return ":/icons/gps.svg";
        case Router:             return ":/icons/router.svg";
        case AntennaBars:        return ":/icons/antenna-bars-5.svg";
        
        // Category Icons - System
        case DeviceLaptop:       return ":/icons/device-laptop.svg";
        case FileCode:           return ":/icons/file-code.svg";
        case Components:         return ":/icons/components.svg";
        case ShieldLock:         return ":/icons/shield-lock.svg";
        case Plug:               return ":/icons/plug.svg";
        case Link:               return ":/icons/link.svg";
        case Box:                return ":/icons/box.svg";
        case Stack:              return ":/icons/stack.svg";
        
        // Category Icons - Storage & Media
        case Disc:               return ":/icons/disc.svg";
        case Archive:            return ":/icons/archive.svg";
        case Folders:            return ":/icons/folders.svg";
        
        // Category Icons - Network
        case Users:              return ":/icons/users.svg";
        case Cloud:              return ":/icons/cloud.svg";
        case Code:               return ":/icons/code.svg";
        
        // Category Icons - Filters & Special
        case Filter:             return ":/icons/filter.svg";
        case LockAccess:         return ":/icons/lock-access.svg";
        
        // Category Icons - Portable & Fallback
        case DeviceMobile:       return ":/icons/device-mobile.svg";
        case QuestionMark:       return ":/icons/question-mark.svg";
        case AlertTriangle:      return ":/icons/alert-triangle.svg";
        case Clock:              return ":/icons/clock.svg";
        
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