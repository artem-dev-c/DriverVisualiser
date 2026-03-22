#include "TextReportFormatter.h"
#include "CategoryGrouper.h"

#include <QDateTime>
#include <QUuid>
#include <algorithm>
#include <map>

// ============================================================================
// Canonical event ID descriptions
// Maps known Windows PnP / DriverFrameworks event IDs to fixed short strings.
// Used in statistics tables so descriptions are consistent regardless of the
// specific device name, version, or instance embedded in the raw event message.
// ============================================================================

static QString canonicalEventDescription(int eventId)
{
    switch (eventId) {
        case 200: return "Device enabled";
        case 201: return "Device disabled";
        case 210: return "Device started";
        case 211: return "Device stopped";
        case 219: return "Driver load failed";
        case 220: return "Driver started successfully";
        case 221: return "Driver unloaded";
        case 400: return "Device installed successfully";
        case 401: return "Device installation failed";
        case 410: return "Device configured successfully";
        case 411: return "Device configuration failed";
        case 420: return "Device requires system restart";
        case 430: return "Device removal started";
        case 431: return "Device removal completed";
        case 7000: return "Service failed to start";
        case 7001: return "Service dependency failed";
        case 7009: return "Service start timeout";
        case 7023: return "Service terminated with error";
        case 7026: return "Boot-start or system-start driver failed";
        case 7031: return "Service terminated unexpectedly";
        case 7034: return "Service terminated unexpectedly";
        case 7045: return "New service installed";
        default:   return QString();  // Unknown — caller falls back to raw message
    }
}

QString TextReportFormatter::format(
    const std::vector<DriverInfo>& drivers,
    const SystemInfo& systemInfo,
    const SystemHealthResult& healthResult,
    int scanWindowDays
) const
{
    QString report;

    report += formatHeader(systemInfo, scanWindowDays);
    report += "\n";
    report += formatHealthSummary(healthResult);
    report += "\n\n";
    report += formatExecutiveSummary(healthResult);
    report += "\n\n";
    report += formatCriticalIssues(drivers);
    report += "\n";
    report += formatWarnings(drivers);
    report += "\n";
    report += formatDriverInventory(drivers);
    report += "\n";
    report += formatEventLogSummary(drivers, scanWindowDays);
    report += "\n";
    report += formatDriverStatistics(drivers);
    report += "\n";
    report += formatRecommendations(drivers);
    report += "\n";
    report += formatFooter();

    return report;
}

QString TextReportFormatter::formatHeader(const SystemInfo& sysInfo, int scanWindowDays) const
{
    QString header;
    
    header += "DRIVER VISUALISER - SYSTEM HEALTH REPORT\n";
    header += QString("=").repeated(60) + "\n";
    
    QDateTime now = QDateTime::currentDateTime();
    header += QString("Generated:     %1\n").arg(now.toString("yyyy-MM-dd hh:mm:ss"));
    
    header += "\n";
    header += "SYSTEM INFORMATION\n";
    header += QString("-").repeated(60) + "\n";
    
    // Build full OS version string
    QString osVersion = QString::fromStdWString(sysInfo.osName);
    if (!sysInfo.displayVersion.empty() && sysInfo.displayVersion != L"Unknown") {
        osVersion += " " + QString::fromStdWString(sysInfo.displayVersion);
    }
    
    header += QString("OS:            %1\n").arg(osVersion);
    
    QString buildStr = QString("Build %1.%2")
        .arg(QString::fromStdWString(sysInfo.buildNumber), QString::fromStdWString(sysInfo.ubr));
    header += QString("Build:         %1\n").arg(buildStr);
    
    QString arch = QString::fromStdWString(sysInfo.architecture);
    header += QString("Architecture:  %1\n").arg(arch);
    
    QString scanWindow = QString("Last %1 days").arg(scanWindowDays);
    header += QString("Scan Window:   %1\n").arg(scanWindow);
    
    return header;
}

QString TextReportFormatter::formatHealthSummary(const SystemHealthResult& healthResult) const
{
    QString summary;
    
    // Determine health label
    QString healthLabel;
    if (healthResult.score >= 80) {
        healthLabel = "Good";
    } else if (healthResult.score >= 50) {
        healthLabel = "Needs Attention";
    } else {
        healthLabel = "Critical";
    }
    
    summary += "\nHEALTH SCORE\n";
    summary += QString("-").repeated(60) + "\n";
    summary += QString("Overall Score:    %1% (%2)\n").arg(healthResult.score).arg(healthLabel);
    summary += QString("Critical Drivers: %1\n").arg(healthResult.criticalDrivers);
    summary += QString("Warning Drivers:  %1\n").arg(healthResult.warningDrivers);
    summary += QString("Healthy Drivers:  %1\n").arg(healthResult.healthyDrivers);
    
    int totalDrivers = healthResult.criticalDrivers + healthResult.warningDrivers + healthResult.healthyDrivers;
    summary += QString("Total Drivers:    %1\n").arg(totalDrivers);
    
    return summary;
}

QString TextReportFormatter::formatExecutiveSummary(const SystemHealthResult& healthResult) const
{
    QString summary;
    
    summary += "EXECUTIVE SUMMARY\n";
    summary += separator(60, QChar(0x2500)) + "\n";  // ─
    
    // Overall status
    QString overallStatus;
    if (healthResult.score >= 80) {
        overallStatus = "GOOD";
    } else if (healthResult.score >= 50) {
        overallStatus = "NEEDS ATTENTION";
    } else {
        overallStatus = "CRITICAL";
    }
    
    summary += QString("Overall Status: %1\n\n").arg(overallStatus);
    
    // System description
    if (healthResult.criticalDrivers == 0 && healthResult.warningDrivers == 0) {
        summary += "The system is healthy with all drivers functioning normally.\n";
        summary += "No issues detected.\n";
    } else if (healthResult.criticalDrivers > 0) {
        summary += QString("The system has %1 critical driver issue%2 requiring attention.\n")
            .arg(healthResult.criticalDrivers)
            .arg(healthResult.criticalDrivers > 1 ? "s" : "");
        
        if (healthResult.warningDrivers > 0) {
            summary += QString("Additionally, %1 driver%2 showing warning signs.\n")
                .arg(healthResult.warningDrivers)
                .arg(healthResult.warningDrivers > 1 ? "s are" : " is");
        }
    } else if (healthResult.warningDrivers > 0) {
        summary += QString("The system is generally stable with %1 driver%2 showing minor issues.\n")
            .arg(healthResult.warningDrivers)
            .arg(healthResult.warningDrivers > 1 ? "s" : "");
    }
    
    return summary;
}

QString TextReportFormatter::formatCriticalIssues(const std::vector<DriverInfo>& drivers) const
{
    QString section;
    
    // Find critical drivers (health score < 70) - matches THRESHOLD_CRITICAL in SystemHealthSummary
    std::vector<const DriverInfo*> criticalDrivers;
    for (const auto& driver : drivers) {
        if (driver.healthScore < 70) {
            criticalDrivers.push_back(&driver);
        }
    }
    
    section += QString("CRITICAL ISSUES (%1)\n").arg(criticalDrivers.size());
    section += separator(60, QChar(0x2500)) + "\n";  // ─
    
    if (criticalDrivers.empty()) {
        section += "No critical issues detected.\n";
        return section;
    }
    
    // Sort by health score (worst first)
    std::sort(criticalDrivers.begin(), criticalDrivers.end(),
        [](const DriverInfo* a, const DriverInfo* b) {
            return a->healthScore < b->healthScore;
        });
    
    for (const auto* driver : criticalDrivers) {
        section += formatDriverDetails(*driver);
        section += "\n";
    }
    
    return section;
}

QString TextReportFormatter::formatWarnings(const std::vector<DriverInfo>& drivers) const
{
    QString section;
    
    // Find warning drivers (70 <= health score < 90) - matches THRESHOLD_WARNING in SystemHealthSummary
    std::vector<const DriverInfo*> warningDrivers;
    for (const auto& driver : drivers) {
        if (driver.healthScore >= 70 && driver.healthScore < 90) {
            warningDrivers.push_back(&driver);
        }
    }
    
    section += QString("WARNINGS (%1)\n").arg(warningDrivers.size());
    section += separator(60, QChar(0x2500)) + "\n";  // ─
    
    if (warningDrivers.empty()) {
        section += "No warnings detected.\n";
        return section;
    }
    
    // Sort by health score (worst first)
    std::sort(warningDrivers.begin(), warningDrivers.end(),
        [](const DriverInfo* a, const DriverInfo* b) {
            return a->healthScore < b->healthScore;
        });
    
    for (const auto* driver : warningDrivers) {
        section += formatDriverDetails(*driver);
        section += "\n";
    }
    
    return section;
}

QString TextReportFormatter::formatDriverDetails(const DriverInfo& driver) const
{
    QString details;
    
    // Status icon
    QString icon;
    if (driver.healthScore < 70) {
        icon = "[!]";
    } else if (driver.healthScore < 90) {
        icon = "[⚠]";
    } else {
        icon = "[✓]";
    }
    
    // Driver name
    QString name = QString::fromStdWString(driver.name);
    details += QString("%1 %2\n").arg(icon, name);
    
    // Category
    QString category = QString::fromStdWString(driver.deviceClassName);
    if (!category.isEmpty() && category != "Unknown") {
        details += QString("    Category:       %1\n").arg(category);
    }
    
    // Status - directly from driver.status enum
    QString statusText;
    switch (driver.status) {
        case DriverStatus::Ok: statusText = "OK"; break;
        case DriverStatus::Disabled: statusText = "Disabled"; break;
        case DriverStatus::NotStarted: statusText = "Not Started"; break;
        case DriverStatus::Error: statusText = "Error"; break;
        default: statusText = "Unknown"; break;
    }
    if (!statusText.isEmpty() && statusText != "Unknown") {
        details += QString("    Status:         %1\n").arg(statusText);
    }
    
    // Manufacturer
    QString manufacturer = QString::fromStdWString(driver.manufacturer);
    if (!manufacturer.isEmpty() && manufacturer != "Unknown") {
        details += QString("    Manufacturer:   %1\n").arg(manufacturer);
    }
    
    // Version - directly from driver.version
    if (driver.version.hasVersion) {
        QString versionText = QString("%1.%2.%3.%4")
            .arg(driver.version.major)
            .arg(driver.version.minor)
            .arg(driver.version.build)
            .arg(driver.version.revision);
        details += QString("    Version:        %1\n").arg(versionText);
    }
    
    // Date - prefer install date (more reliable), fall back to driver date
    std::optional<std::chrono::sys_days> dateToShow = driver.installDate;
    QString dateLabel = "Installed";
    if (!dateToShow.has_value()) {
        dateToShow = driver.driverDate;
        dateLabel = "Driver Date";
    }
    
    if (dateToShow.has_value()) {
        auto ymd = std::chrono::year_month_day{dateToShow.value()};
        QString dateText = QString("%1-%2-%3")
            .arg(static_cast<int>(ymd.year()))
            .arg(static_cast<unsigned>(ymd.month()), 2, 10, QChar('0'))
            .arg(static_cast<unsigned>(ymd.day()), 2, 10, QChar('0'));
        details += QString("    %1: %2\n").arg(dateLabel, dateText);
    }
    
    // Health score
    details += QString("    Health Score:   %1%\n").arg(driver.healthScore);
    
    // Error count from errorLog vector
    if (!driver.errorLog.empty()) {
        details += QString("    Error Count:    %1\n").arg(driver.errorLog.size());
    }
    
    // Flags
    if (!driver.healthFlags.empty()) {
        details += QString("    Flags:          ");
        QStringList flagNames;
        for (const auto& flag : driver.healthFlags) {
            flagNames.append(QString::fromStdWString(flag.id));
        }
        details += flagNames.join(", ") + "\n";
    }
    
    // Description (if critical or warning)
    if (driver.healthScore < 90) {
        details += "\n    Description:\n";
        if (driver.healthScore < 70) {
            details += "    This driver has critical issues and may be causing system\n";
            details += "    instability or device malfunction.\n";
        } else {
            details += "    This driver is showing warning signs and should be monitored.\n";
        }
    }
    
    return details;
}

QString TextReportFormatter::formatDriverInventory(const std::vector<DriverInfo>& drivers) const
{
    QString section;
    
    section += "DRIVER INVENTORY BY CATEGORY\n";
    section += separator(60, QChar(0x2500)) + "\n";  // ─
    
    // Group by category - returns a map<wstring, DeviceCategory>
    auto categoriesMap = CategoryGrouper::groupByCategory(drivers);
    
    // Convert map to vector for sorting
    std::vector<DeviceCategory> sortedCategories;
    sortedCategories.reserve(categoriesMap.size());
    for (const auto& pair : categoriesMap) {
        sortedCategories.push_back(pair.second);  // Extract the DeviceCategory from the pair
    }
    
    // Sort categories by: critical count desc, then warning count desc, then name
    std::sort(sortedCategories.begin(), sortedCategories.end(),
        [](const DeviceCategory& a, const DeviceCategory& b) {
            // Count critical and warnings in each category
            int aCritical = 0, aWarning = 0;
            int bCritical = 0, bWarning = 0;
            
            for (const auto& driver : a.drivers) {
                if (driver.healthScore < 70) aCritical++;
                else if (driver.healthScore < 90) aWarning++;
            }
            
            for (const auto& driver : b.drivers) {
                if (driver.healthScore < 70) bCritical++;
                else if (driver.healthScore < 90) bWarning++;
            }
            
            if (aCritical != bCritical) return aCritical > bCritical;
            if (aWarning != bWarning) return aWarning > bWarning;
            return a.className < b.className;
        });
    
    // List each category
    for (const auto& category : sortedCategories) {
        QString categoryName = QString::fromStdWString(category.displayName);
        
        // Count issues in this category
        int criticalCount = 0;
        int warningCount = 0;
        for (const auto& driver : category.drivers) {
            if (driver.healthScore < 70) criticalCount++;
            else if (driver.healthScore < 90) warningCount++;
        }
        
        // Category header
        QString statusSuffix;
        if (criticalCount > 0) {
            statusSuffix = QString(", %1 critical").arg(criticalCount);
        } else if (warningCount > 0) {
            statusSuffix = QString(", %1 warning").arg(warningCount);
        }
        
        section += QString("\n%1 (%2 driver%3%4)\n")
            .arg(categoryName)
            .arg(category.drivers.size())
            .arg(category.drivers.size() > 1 ? "s" : "")
            .arg(statusSuffix);
        
        // List up to 5 drivers (critical/warning first, then healthy)
        std::vector<const DriverInfo*> sortedDrivers;
        for (const auto& driver : category.drivers) {
            sortedDrivers.push_back(&driver);
        }
        
        std::sort(sortedDrivers.begin(), sortedDrivers.end(),
            [](const DriverInfo* a, const DriverInfo* b) {
                return a->healthScore < b->healthScore;
            });
        
        int displayCount = std::min(5, static_cast<int>(sortedDrivers.size()));
        for (int i = 0; i < displayCount; i++) {
            const auto* driver = sortedDrivers[i];
            QString name = QString::fromStdWString(driver->name);
            QString icon;
            QString healthText;
            
            if (driver->healthScore < 70) {
                icon = "  × ";
                healthText = QString("[CRITICAL] Health: %1%").arg(driver->healthScore);
            } else if (driver->healthScore < 90) {
                icon = "  ⚠ ";
                healthText = QString("[WARNING]  Health: %1%").arg(driver->healthScore);
            } else {
                icon = "  ✓ ";
                healthText = QString("[HEALTHY]  Health: %1%").arg(driver->healthScore);
            }
            
            section += QString("%1%2%3\n").arg(icon, name.leftJustified(35, ' '), healthText);
        }
        
        if (category.drivers.size() > 5) {
            section += QString("  ... and %1 more\n").arg(category.drivers.size() - 5);
        }
    }
    
    return section;
}

QString TextReportFormatter::formatEventLogSummary(const std::vector<DriverInfo>& drivers, int scanWindowDays) const
{
    QString section;

    section += QString("EVENT LOG SUMMARY (Past %1 days)\n").arg(scanWindowDays);
    section += separator(60, QChar(0x2500)) + "\n";

    // Collect all events
    struct Entry {
        std::chrono::system_clock::time_point timestamp;
        std::wstring driverName;
        int eventId;
        std::wstring level;
        std::wstring message;
    };
    std::vector<Entry> all;
    for (const auto& driver : drivers) {
        for (const auto& e : driver.errorLog) {
            all.push_back({ e.timestamp, driver.name, e.eventId, e.level, e.message });
        }
    }

    if (all.empty()) {
        section += "No driver-related events detected in the scan window.\n";
        return section;
    }

    // ── Stats table grouped by eventId ───────────────────────────────────────
    // Key: eventId → { level, representative message, count }
    struct Stat { std::wstring level; std::wstring message; int count = 0; };
    std::map<int, Stat> stats;
    int criticalCount = 0, warningCount = 0, infoCount = 0;

    for (const auto& e : all) {
        auto& s = stats[e.eventId];
        s.count++;
        if (s.count == 1) { s.level = e.level; s.message = e.message; }
        if (e.level == L"Critical" || e.level == L"Error") criticalCount++;
        else if (e.level == L"Warning")                     warningCount++;
        else                                                infoCount++;
    }

    section += QString("Total: %1 events  (%2 critical/error, %3 warnings, %4 info)\n\n")
        .arg(all.size()).arg(criticalCount).arg(warningCount).arg(infoCount);

    // ── Stats table grouped by eventId ───────────────────────────────────────
    // Sort by count descending
    std::vector<std::pair<int, Stat>> sorted(stats.begin(), stats.end());
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) { return a.second.count > b.second.count; });

    // Fixed column widths
    static constexpr int COL_ID   = 12;
    static constexpr int COL_SEV  = 12;
    static constexpr int COL_CNT  =  6;

    section += QString("  %1%2%3  %4\n")
        .arg(QString("Event ID").leftJustified(COL_ID))
        .arg(QString("Severity").leftJustified(COL_SEV))
        .arg(QString("Count").rightJustified(COL_CNT))
        .arg("Description");
    section += "  " + separator(56, QChar(QChar(0x2500))) + "\n";

    for (const auto& [id, stat] : sorted) {
        QString evId  = QString("Event %1").arg(id).leftJustified(COL_ID);
        QString level = QString::fromStdWString(stat.level).leftJustified(COL_SEV);
        QString cnt   = QString("%1x").arg(stat.count).rightJustified(COL_CNT);
        QString msg   = canonicalEventDescription(id);
        if (msg.isEmpty()) {
            msg = QString::fromStdWString(stat.message);
            if (msg.length() > 50) msg = msg.left(47) + "...";
        }
        section += QString("  %1%2%3  %4\n").arg(evId, level, cnt, msg);
    }
    section += "\n";

    // ── Critical and Warning event list (newest first, max 30) ───────────────
    std::vector<Entry> actionable;
    for (const auto& e : all) {
        if (e.level == L"Critical" || e.level == L"Error" || e.level == L"Warning")
            actionable.push_back(e);
    }
    std::sort(actionable.begin(), actionable.end(),
        [](const Entry& a, const Entry& b) { return a.timestamp > b.timestamp; });

    if (!actionable.empty()) {
        section += QString("Critical & Warning Events (%1):\n").arg(actionable.size());
        section += "  " + separator(56, QChar(0x2500)) + "\n";

        int shown = std::min(30, static_cast<int>(actionable.size()));
        for (int i = 0; i < shown; ++i) {
            const auto& e = actionable[i];
            auto timeT = std::chrono::system_clock::to_time_t(e.timestamp);
            QDateTime dt = QDateTime::fromSecsSinceEpoch(timeT);
            QString ts  = dt.toString("yyyy-MM-dd hh:mm:ss");
            QString lvl = QString::fromStdWString(e.level).leftJustified(10);
            QString drv = QString::fromStdWString(e.driverName);
            QString msg = QString::fromStdWString(e.message);
            if (msg.length() > 60) msg = msg.left(57) + "...";
            section += QString("  [%1]  %2  Event %3\n    %4\n    %5\n")
                .arg(ts, lvl)
                .arg(e.eventId)
                .arg(drv, msg);
        }
        if (static_cast<int>(actionable.size()) > 30)
            section += QString("  ... and %1 more\n").arg(actionable.size() - 30);
    }

    return section;
}

QString TextReportFormatter::formatDriverStatistics(const std::vector<DriverInfo>& drivers) const
{
    QString section;
    
    section += "DRIVER STATISTICS\n";
    section += separator(60, QChar(0x2500)) + "\n";  // ─
    
    int totalDrivers = drivers.size();
    section += QString("Total Drivers:        %1\n").arg(totalDrivers);
    
    // Count by importance level
    int criticalImportance = 0;
    int importantImportance = 0;
    int optionalImportance = 0;
    int virtualImportance = 0;
    
    for (const auto& driver : drivers) {
        switch (driver.importanceLevel) {
            case DriverImportance::Critical:
                criticalImportance++;
                break;
            case DriverImportance::Important:
                importantImportance++;
                break;
            case DriverImportance::Optional:
                optionalImportance++;
                break;
            case DriverImportance::Virtual:
                virtualImportance++;
                break;
            default:
                optionalImportance++; // Count unknown as optional
                break;
        }
    }
    
    section += QString("  - Critical:         %1\n").arg(criticalImportance);
    section += QString("  - Important:        %1\n").arg(importantImportance);
    section += QString("  - Optional:         %1\n").arg(optionalImportance);
    section += QString("  - Virtual:          %1\n\n").arg(virtualImportance);
    
    // Health distribution
    section += "Health Distribution:\n";
    section += createHealthDistributionChart(drivers);
    section += "\n";
    
    // Installation age breakdown
    section += "Installation Age:\n";
    
    int lessThanMonth = 0;
    int oneToSixMonths = 0;
    int sixToTwelveMonths = 0;
    int overOneYear = 0;
    int unknown = 0;
    
    auto now = std::chrono::system_clock::now();
    auto nowDays = std::chrono::floor<std::chrono::days>(now);
    
    for (const auto& driver : drivers) {
        // Prefer install date (more reliable), fall back to driver date
        std::optional<std::chrono::sys_days> dateToUse = driver.installDate;
        if (!dateToUse.has_value()) {
            dateToUse = driver.driverDate;
        }
        
        if (!dateToUse.has_value()) {
            unknown++;
            continue;
        }
        
        auto daysOld = std::chrono::duration_cast<std::chrono::days>(
            nowDays - dateToUse.value()
        ).count();
        
        if (daysOld < 30) {
            lessThanMonth++;
        } else if (daysOld < 180) {
            oneToSixMonths++;
        } else if (daysOld < 365) {
            sixToTwelveMonths++;
        } else {
            overOneYear++;
        }
    }
    
    section += QString("  < 1 month:         %1\n").arg(lessThanMonth);
    section += QString("  1-6 months:        %1\n").arg(oneToSixMonths);
    section += QString("  6-12 months:       %1\n").arg(sixToTwelveMonths);
    section += QString("  > 1 year:          %1\n").arg(overOneYear);
    if (unknown > 0) {
        section += QString("  Unknown:           %1\n").arg(unknown);
    }
    
    return section;
}

QString TextReportFormatter::createHealthDistributionChart(const std::vector<DriverInfo>& drivers) const
{
    QString chart;
    
    // Count drivers in each health range
    int perfect = 0;      // 100
    int excellent = 0;    // 80-99
    int good = 0;         // 50-79
    int critical = 0;     // < 50
    
    for (const auto& driver : drivers) {
        if (driver.healthScore == 100) {
            perfect++;
        } else if (driver.healthScore >= 80) {
            excellent++;
        } else if (driver.healthScore >= 50) {
            good++;
        } else {
            critical++;
        }
    }
    
    int total = drivers.size();
    
    // Create ASCII bar chart
    auto createBar = [total](int count) -> QString {
        if (total == 0) return "";
        int barLength = (count * 20) / total;  // Max 20 chars
        return QString("█").repeated(barLength);
    };
    
    auto formatPercentage = [total](int count) -> QString {
        if (total == 0) return "0.0%";
        double pct = (count * 100.0) / total;
        return QString::number(pct, 'f', 1) + "%";
    };
    
    chart += QString("  100%       (%1 drivers)  %2 %3\n")
        .arg(perfect, 3)
        .arg(createBar(perfect).leftJustified(20, ' '))
        .arg(formatPercentage(perfect));
    
    chart += QString("  80-99%     (%1 drivers)  %2 %3\n")
        .arg(excellent, 3)
        .arg(createBar(excellent).leftJustified(20, ' '))
        .arg(formatPercentage(excellent));
    
    chart += QString("  50-79%     (%1 drivers)  %2 %3\n")
        .arg(good, 3)
        .arg(createBar(good).leftJustified(20, ' '))
        .arg(formatPercentage(good));
    
    chart += QString("  <50%       (%1 drivers)  %2 %3\n")
        .arg(critical, 3)
        .arg(createBar(critical).leftJustified(20, ' '))
        .arg(formatPercentage(critical));
    
    return chart;
}

QString TextReportFormatter::formatRecommendations(const std::vector<DriverInfo>& drivers) const
{
    QString section;
    
    section += "RECOMMENDATIONS\n";
    section += separator(60, QChar(0x2500)) + "\n";  // ─
    
    // Count issues
    int criticalCount = 0;
    int warningCount = 0;
    int errorCount = 0;
    int eventWarningCount = 0;
    for (const auto& driver : drivers) {
        if (driver.healthScore < 70) criticalCount++;
        else if (driver.healthScore < 90) warningCount++;
        for (const auto& entry : driver.errorLog) {
            if (entry.level == L"Critical" || entry.level == L"Error")
                errorCount++;
            else if (entry.level == L"Warning")
                eventWarningCount++;
        }
    }
    int actionableEventCount = errorCount + eventWarningCount;
    
    if (criticalCount == 0 && warningCount == 0 && errorCount == 0) {
        section += "✓ System appears stable and healthy\n";
        section += "  • No immediate action required\n";
        section += "  • Continue regular monitoring\n";
        section += "  • Keep drivers updated\n";
        return section;
    }
    
    // Generate recommendations based on issues
    int recommendationNum = 1;
    
    if (criticalCount > 0) {
        section += QString("%1. Address Critical Driver Issues (%2)\n").arg(recommendationNum++).arg(criticalCount);
        section += "   • Review drivers with health scores below 70%\n";
        section += "   • Check Device Manager for error details\n";
        section += "   • Update drivers from manufacturer websites\n";
        section += "   • Consider rolling back recent driver updates if issues started recently\n\n";
    }
    
    if (warningCount > 0) {
        section += QString("%1. Monitor Warning Drivers (%2)\n").arg(recommendationNum++).arg(warningCount);
        section += "   • Keep an eye on drivers with health scores 70-89%\n";
        section += "   • Check for available driver updates\n";
        section += "   • Review error logs for patterns\n\n";
    }
    
    if (actionableEventCount > 10) {
        section += QString("%1. Investigate Event Log Errors\n").arg(recommendationNum++);
        section += QString("   • %1 critical/warning events detected (%2 errors, %3 warnings)\n")
            .arg(actionableEventCount).arg(errorCount).arg(eventWarningCount);
        section += "   • Review Windows Event Viewer for details\n";
        section += "   • Look for patterns (time of day, specific operations)\n\n";
    }
    
    // General recommendations
    section += QString("%1. General System Maintenance\n").arg(recommendationNum++);
    section += "   • Run Windows Update regularly\n";
    section += "   • Keep manufacturer drivers up to date\n";
    section += "   • Rescan periodically to track changes\n";
    section += "   • Create system restore points before major updates\n";
    
    return section;
}

QString TextReportFormatter::formatFooter() const
{
    QString footer;
    
    footer += QString("=").repeated(60) + "\n";
    footer += "Generated by Driver Visualiser\n";
    
    // Simple timestamp-based ID
    QString reportId = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    footer += QString("Report ID: %1\n").arg(reportId);
    
    return footer;
}

QString TextReportFormatter::separator(int length, QChar ch) const
{
    return QString(ch).repeated(length);
}