#include "HtmlReportFormatter.h"
#include "CategoryGrouper.h"
#include <QDateTime>
#include <algorithm>
#include <map>

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
        default:   return QString();
    }
}

QString HtmlReportFormatter::format(
    const std::vector<DriverInfo>& drivers,
    const SystemInfo& systemInfo,
    const SystemHealthResult& healthResult,
    int scanWindowDays
) const
{
    QString html;
    
    html += formatHtmlHeader();
    html += formatStyles();
    html += "</head><body>\n";
    
    html += "<div class=\"container\">\n";
    html += formatDashboardHeader(healthResult, systemInfo, scanWindowDays);
    html += formatCriticalSection(drivers);
    html += formatWarningsSection(drivers);
    html += formatRecommendations(drivers);
    html += formatStatistics(drivers, healthResult);
    html += formatEventLog(drivers, scanWindowDays);
    html += formatDriverInventory(drivers);
    html += "</div>\n";
    
    html += formatScripts();
    html += formatHtmlFooter();
    
    return html;
}

QString HtmlReportFormatter::formatHtmlHeader() const
{
    QDateTime now = QDateTime::currentDateTime();
    
    QString header = R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta name="generator" content="Driver Visualiser">
    <meta name="date" content=")";
    
    header += now.toString("yyyy-MM-dd") + "\">\n";
    header += "    <title>System Health Report - " + now.toString("yyyy-MM-dd HH:mm") + "</title>\n";
    
    return header;
}

QString HtmlReportFormatter::formatStyles() const
{
    return R"(    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #1a1a1a;
            color: #e0e0e0;
            line-height: 1.6;
            padding: 20px;
        }
        
        .container {
            max-width: 1200px;
            margin: 0 auto;
        }
        
        /* Dashboard Header */
        .dashboard-header {
            background: linear-gradient(135deg, #2b2b2b 0%, #1e1e1e 100%);
            border-radius: 12px;
            padding: 30px;
            margin-bottom: 30px;
            border: 1px solid #3a3a3a;
        }
        
        .header-top {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 20px;
        }
        
        .header-title h1 {
            font-size: 24px;
            color: #5dade2;
            margin-bottom: 5px;
        }
        
        .header-title .subtitle {
            font-size: 14px;
            color: #888;
        }
        
        .system-info {
            text-align: right;
            font-size: 13px;
            color: #999;
        }
        
        .health-score-container {
            display: flex;
            gap: 40px;
            align-items: center;
        }
        
        /* CSS Donut Chart */
        .score-circle {
            position: relative;
            width: 120px;
            height: 120px;
        }
        
        .score-ring {
            width: 100%;
            height: 100%;
            border-radius: 50%;
            background: conic-gradient(
                #27ae60 0deg,
                #27ae60 calc(var(--score) * 3.6deg),
                #3a3a3a calc(var(--score) * 3.6deg)
            );
            display: flex;
            align-items: center;
            justify-content: center;
        }
        
        .score-inner {
            width: 80px;
            height: 80px;
            background: #2b2b2b;
            border-radius: 50%;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
        }
        
        .score-value {
            font-size: 28px;
            font-weight: bold;
            color: #fff;
        }
        
        .score-label {
            font-size: 11px;
            color: #888;
            text-transform: uppercase;
        }
        
        .health-stats {
            flex: 1;
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 15px;
        }
        
        .stat-card {
            background: #1e1e1e;
            padding: 15px;
            border-radius: 8px;
            border-left: 4px solid #555;
        }
        
        .stat-card.critical { border-left-color: #e74c3c; }
        .stat-card.warning { border-left-color: #f39c12; }
        .stat-card.healthy { border-left-color: #27ae60; }
        
        .stat-number {
            font-size: 32px;
            font-weight: bold;
            margin-bottom: 5px;
        }
        
        .stat-card.critical .stat-number { color: #e74c3c; }
        .stat-card.warning .stat-number { color: #f39c12; }
        .stat-card.healthy .stat-number { color: #27ae60; }
        
        .stat-label {
            font-size: 13px;
            color: #999;
        }
        
        /* Sections */
        .section {
            background: #2b2b2b;
            border-radius: 12px;
            padding: 25px;
            margin-bottom: 20px;
            border: 1px solid #3a3a3a;
        }
        
        .section-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 20px;
            padding-bottom: 15px;
            border-bottom: 2px solid #3a3a3a;
        }
        
        .section-title {
            font-size: 18px;
            font-weight: 600;
            color: #5dade2;
        }
        
        .section-count {
            background: #3a3a3a;
            padding: 4px 12px;
            border-radius: 12px;
            font-size: 13px;
            color: #ccc;
        }
        
        /* Driver Cards */
        .driver-card {
            background: #1e1e1e;
            border-radius: 8px;
            padding: 15px;
            margin-bottom: 12px;
            border-left: 4px solid #555;
        }
        
        .driver-card.critical { border-left-color: #e74c3c; }
        .driver-card.warning { border-left-color: #f39c12; }
        .driver-card.healthy { border-left-color: #27ae60; }
        
        .driver-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 10px;
        }
        
        .driver-name {
            font-size: 15px;
            font-weight: 600;
            color: #fff;
        }
        
        .driver-health {
            font-size: 14px;
            font-weight: bold;
            padding: 4px 10px;
            border-radius: 4px;
            background: #3a3a3a;
        }
        
        .driver-card.critical .driver-health { background: #e74c3c; color: #fff; }
        .driver-card.warning .driver-health { background: #f39c12; color: #fff; }
        .driver-card.healthy .driver-health { background: #27ae60; color: #fff; }
        
        .driver-details {
            font-size: 13px;
            color: #aaa;
            line-height: 1.8;
        }
        
        .driver-details span {
            margin-right: 15px;
        }
        
        .driver-flags {
            margin-top: 8px;
            font-size: 12px;
            color: #f39c12;
        }
        
        /* Recommendations - removed blue highlight */
        
        .recommendation-list {
            list-style: none;
        }
        
        .recommendation-list li {
            padding: 12px 0;
            border-bottom: 1px solid rgba(255,255,255,0.1);
        }
        
        .recommendation-list li:last-child {
            border-bottom: none;
        }
        
        .recommendation-title {
            font-weight: 600;
            color: #5dade2;
            margin-bottom: 5px;
        }
        
        .recommendation-desc {
            font-size: 13px;
            color: #aaa;
            margin-left: 20px;
        }
        
        /* Statistics Charts */
        .stats-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 20px;
        }
        
        @media (max-width: 900px) {
            .stats-grid {
                grid-template-columns: 1fr;
            }
        }
        
        .chart-container {
            background: #1e1e1e;
            padding: 20px;
            border-radius: 8px;
        }
        
        .chart-title {
            font-size: 14px;
            font-weight: 600;
            color: #ccc;
            margin-bottom: 15px;
        }
        
        /* CSS Bar Chart */
        .bar-chart {
            display: flex;
            flex-direction: column;
            gap: 10px;
        }
        
        .bar-row {
            display: flex;
            align-items: center;
            gap: 10px;
        }
        
        .bar-label {
            min-width: 80px;
            font-size: 12px;
            color: #999;
        }
        
        .bar-track {
            flex: 1;
            background: #3a3a3a;
            height: 24px;
            border-radius: 4px;
            position: relative;
            overflow: hidden;
        }
        
        .bar-fill {
            height: 100%;
            background: linear-gradient(90deg, #5dade2, #3498db);
            border-radius: 4px;
            transition: width 1s ease;
        }
        
        .bar-value {
            position: absolute;
            right: 8px;
            top: 50%;
            transform: translateY(-50%);
            font-size: 11px;
            color: #fff;
            font-weight: 600;
        }
        
        /* Collapsible Sections */
        .collapsible-header {
            cursor: pointer;
            user-select: none;
        }
        
        .collapsible-header:hover {
            opacity: 0.8;
        }
        
        .collapsible-content {
            max-height: 0;
            overflow: hidden;
            transition: max-height 0.3s ease;
        }
        
        .collapsible-content.active {
            max-height: 10000px;
        }
        
        /* Category Grid */
        .category-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
            gap: 15px;
        }
        
        .category-card {
            background: #1e1e1e;
            padding: 15px;
            border-radius: 8px;
            cursor: pointer;
            transition: background 0.2s;
        }
        
        .category-card:hover {
            background: #252525;
        }
        
        .category-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        
        .category-name {
            font-weight: 600;
            color: #fff;
        }
        
        .category-count {
            font-size: 12px;
            color: #888;
        }
        
        .category-issues {
            font-size: 12px;
            margin-top: 5px;
        }
        
        .category-issues.critical { color: #e74c3c; }
        .category-issues.warning { color: #f39c12; }
        
        .category-sample {
            margin-top: 8px;
            font-size: 12px;
        }
        
        .sample-driver {
            color: #27ae60;
            padding: 2px 0;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
        }
        
        .sample-more {
            color: #888;
            font-style: italic;
            margin-top: 4px;
        }
        
        /* Error Log Entries */
        .error-log-list {
            display: flex;
            flex-direction: column;
            gap: 12px;
        }
        
        .error-entry {
            background: #1e1e1e;
            border-left: 4px solid #555;
            padding: 12px;
            border-radius: 6px;
        }
        
        .error-entry.error { border-left-color: #e74c3c; }
        .error-entry.warning { border-left-color: #f39c12; }
        .error-entry.info { border-left-color: #5dade2; }
        
        .error-header {
            display: flex;
            gap: 15px;
            align-items: center;
            margin-bottom: 8px;
            font-size: 12px;
        }
        
        .error-time {
            color: #888;
            font-family: 'Courier New', monospace;
        }
        
        .error-level {
            padding: 2px 8px;
            border-radius: 3px;
            font-weight: 600;
            font-size: 11px;
        }
        
        .error-entry.error .error-level {
            background: #e74c3c;
            color: #fff;
        }
        
        .error-entry.warning .error-level {
            background: #f39c12;
            color: #fff;
        }
        
        .error-entry.info .error-level {
            background: #5dade2;
            color: #fff;
        }
        
        .error-id {
            color: #999;
        }
        
        .error-driver {
            font-size: 13px;
            color: #5dade2;
            margin-bottom: 6px;
            font-weight: 500;
        }
        
        .error-message {
            font-size: 13px;
            color: #ccc;
            line-height: 1.5;
        }
        
        /* Utility */
        .empty-state {
            text-align: center;
            padding: 40px;
            color: #666;
        }

        /* Event stats table */
        .event-stats { margin-bottom: 20px; }
        .event-stats-title { font-weight: 600; margin-bottom: 8px; color: #a0b0c0; font-size: 12px; text-transform: uppercase; letter-spacing: 0.05em; }
        .event-stats-table { width: 100%; border-collapse: collapse; font-size: 13px; }
        .event-stats-table th { text-align: left; padding: 6px 10px; border-bottom: 1px solid #334155; color: #a0b0c0; font-weight: 600; font-size: 11px; text-transform: uppercase; }
        .event-stats-table td { padding: 6px 10px; border-bottom: 1px solid #1e293b; }
        .stat-id { font-family: monospace; color: #cbd5e1; }
        .stat-count { font-weight: bold; text-align: center; }
        .stat-msg { color: #94a3b8; }
        .stat-level.error { color: #e74c3c; font-weight: bold; }
        .stat-level.warning { color: #f39c12; font-weight: bold; }
        .stat-level.info { color: #5dade2; }
        .event-log-subtitle { font-weight: 600; margin: 16px 0 8px; color: #a0b0c0; font-size: 12px; text-transform: uppercase; letter-spacing: 0.05em; }

        @media print {
            body { background: white; color: black; }
            .section { page-break-inside: avoid; }
        }
    </style>
)";
}

QString HtmlReportFormatter::formatScripts() const
{
    return R"(    <script>
        // Simple collapsible sections
        document.addEventListener('DOMContentLoaded', function() {
            const headers = document.querySelectorAll('.collapsible-header');
            headers.forEach(header => {
                header.addEventListener('click', function() {
                    const content = this.nextElementSibling;
                    content.classList.toggle('active');
                });
            });
            
            // Animate bars on load
            setTimeout(() => {
                document.querySelectorAll('.bar-fill').forEach(bar => {
                    bar.style.width = bar.dataset.width;
                });
            }, 100);
        });
    </script>
)";
}

QString HtmlReportFormatter::formatDashboardHeader(
    const SystemHealthResult& healthResult,
    const SystemInfo& systemInfo,
    int scanWindowDays
) const
{
    QString html;
    QDateTime now = QDateTime::currentDateTime();
    
    // Build OS string
    QString osVersion = QString::fromStdWString(systemInfo.osName);
    if (!systemInfo.displayVersion.empty()) {
        osVersion += " " + QString::fromStdWString(systemInfo.displayVersion);
    }
    
    QString healthLabel = getHealthLabel(healthResult.score);
    
    html += "<div class=\"dashboard-header\">\n";
    html += "  <div class=\"header-top\">\n";
    html += "    <div class=\"header-title\">\n";
    html += "      <h1>System Health Report</h1>\n";
    html += "      <div class=\"subtitle\">" + healthLabel + "</div>\n";
    html += "    </div>\n";
    html += "    <div class=\"system-info\">\n";
    html += "      <div>" + escapeHtml(osVersion) + "</div>\n";
    html += "      <div>Build " + QString::fromStdWString(systemInfo.buildNumber) + "." + QString::fromStdWString(systemInfo.ubr) + "</div>\n";
    html += "      <div>Generated: " + now.toString("yyyy-MM-dd HH:mm") + "</div>\n";
    html += "    </div>\n";
    html += "  </div>\n";
    
    html += "  <div class=\"health-score-container\">\n";
    html += "    <div class=\"score-circle\">\n";
    html += "      <div class=\"score-ring\" style=\"--score: " + QString::number(healthResult.score) + "\">\n";
    html += "        <div class=\"score-inner\">\n";
    html += "          <div class=\"score-value\">" + QString::number(healthResult.score) + "%</div>\n";
    html += "          <div class=\"score-label\">Health</div>\n";
    html += "        </div>\n";
    html += "      </div>\n";
    html += "    </div>\n";
    
    html += "    <div class=\"health-stats\">\n";
    html += "      <div class=\"stat-card critical\">\n";
    html += "        <div class=\"stat-number\">" + QString::number(healthResult.criticalDrivers) + "</div>\n";
    html += "        <div class=\"stat-label\">Critical</div>\n";
    html += "      </div>\n";
    html += "      <div class=\"stat-card warning\">\n";
    html += "        <div class=\"stat-number\">" + QString::number(healthResult.warningDrivers) + "</div>\n";
    html += "        <div class=\"stat-label\">Warnings</div>\n";
    html += "      </div>\n";
    html += "      <div class=\"stat-card healthy\">\n";
    html += "        <div class=\"stat-number\">" + QString::number(healthResult.healthyDrivers) + "</div>\n";
    html += "        <div class=\"stat-label\">Healthy</div>\n";
    html += "      </div>\n";
    html += "    </div>\n";
    html += "  </div>\n";
    html += "</div>\n";
    
    return html;
}

QString HtmlReportFormatter::formatCriticalSection(const std::vector<DriverInfo>& drivers) const
{
    // Find critical drivers
    std::vector<const DriverInfo*> criticalDrivers;
    for (const auto& driver : drivers) {
        if (driver.healthScore < 70) {
            criticalDrivers.push_back(&driver);
        }
    }
    
    QString html;
    html += "<div class=\"section\">\n";
    html += "  <div class=\"section-header\">\n";
    html += "    <div class=\"section-title\">Critical Issues</div>\n";
    html += "    <div class=\"section-count\">" + QString::number(criticalDrivers.size()) + "</div>\n";
    html += "  </div>\n";
    
    if (criticalDrivers.empty()) {
        html += "  <div class=\"empty-state\">No critical issues detected</div>\n";
    } else {
        // Sort by health score (worst first)
        std::sort(criticalDrivers.begin(), criticalDrivers.end(),
            [](const DriverInfo* a, const DriverInfo* b) {
                return a->healthScore < b->healthScore;
            });
        
        html += "  <div class=\"driver-list\">\n";
        for (const auto* driver : criticalDrivers) {
            html += formatDriverCard(*driver);
        }
        html += "  </div>\n";
    }
    
    html += "</div>\n";
    return html;
}

QString HtmlReportFormatter::formatWarningsSection(const std::vector<DriverInfo>& drivers) const
{
    // Find warning drivers
    std::vector<const DriverInfo*> warningDrivers;
    for (const auto& driver : drivers) {
        if (driver.healthScore >= 70 && driver.healthScore < 90) {
            warningDrivers.push_back(&driver);
        }
    }
    
    QString html;
    html += "<div class=\"section\">\n";
    html += "  <div class=\"section-header\">\n";
    html += "    <div class=\"section-title\">Warnings</div>\n";
    html += "    <div class=\"section-count\">" + QString::number(warningDrivers.size()) + "</div>\n";
    html += "  </div>\n";
    
    if (warningDrivers.empty()) {
        html += "  <div class=\"empty-state\">No warnings detected</div>\n";
    } else {
        std::sort(warningDrivers.begin(), warningDrivers.end(),
            [](const DriverInfo* a, const DriverInfo* b) {
                return a->healthScore < b->healthScore;
            });
        
        html += "  <div class=\"driver-list\">\n";
        for (const auto* driver : warningDrivers) {
            html += formatDriverCard(*driver);
        }
        html += "  </div>\n";
    }
    
    html += "</div>\n";
    return html;
}

QString HtmlReportFormatter::formatRecommendations(const std::vector<DriverInfo>& drivers) const
{
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
    
    QString html;
    html += "<div class=\"section\">\n";
    html += "  <div class=\"section-header\">\n";
    html += "    <div class=\"section-title\">Recommended Actions</div>\n";
    html += "  </div>\n";
    html += "  <ul class=\"recommendation-list\">\n";
    
    int recommendationNum = 1;
    
    if (criticalCount > 0) {
        html += "    <li>\n";
        html += "      <div class=\"recommendation-title\">" + QString::number(recommendationNum++) + ". Address Critical Driver Issues (" + QString::number(criticalCount) + ")</div>\n";
        html += "      <div class=\"recommendation-desc\">• Review drivers with health scores below 70%</div>\n";
        html += "      <div class=\"recommendation-desc\">• Check Device Manager for error details</div>\n";
        html += "      <div class=\"recommendation-desc\">• Update drivers from manufacturer websites</div>\n";
        html += "      <div class=\"recommendation-desc\">• Consider rolling back recent driver updates if issues started recently</div>\n";
        html += "    </li>\n";
    }
    
    if (warningCount > 0) {
        html += "    <li>\n";
        html += "      <div class=\"recommendation-title\">" + QString::number(recommendationNum++) + ". Monitor Warning Drivers (" + QString::number(warningCount) + ")</div>\n";
        html += "      <div class=\"recommendation-desc\">• Keep an eye on drivers with health scores 70-89%</div>\n";
        html += "      <div class=\"recommendation-desc\">• Check for available driver updates</div>\n";
        html += "      <div class=\"recommendation-desc\">• Review error logs for patterns</div>\n";
        html += "    </li>\n";
    }
    
    if (actionableEventCount > 10) {
        html += "    <li>\n";
        html += "      <div class=\"recommendation-title\">" + QString::number(recommendationNum++) + ". Review Event Logs</div>\n";
        html += "      <div class=\"recommendation-desc\">• " + QString::number(actionableEventCount)
             + " critical/warning events detected (" + QString::number(errorCount)
             + " errors, " + QString::number(eventWarningCount) + " warnings)</div>\n";
        html += "      <div class=\"recommendation-desc\">• Check the Error Logs section below for details</div>\n";
        html += "      <div class=\"recommendation-desc\">• Look for patterns or repeated issues</div>\n";
        html += "    </li>\n";
    }
    
    html += "    <li>\n";
    html += "      <div class=\"recommendation-title\">" + QString::number(recommendationNum++) + ". General System Maintenance</div>\n";
    html += "      <div class=\"recommendation-desc\">• Run Windows Update regularly</div>\n";
    html += "      <div class=\"recommendation-desc\">• Keep manufacturer drivers up to date</div>\n";
    html += "      <div class=\"recommendation-desc\">• Rescan periodically to track changes</div>\n";
    html += "      <div class=\"recommendation-desc\">• Create system restore points before major updates</div>\n";
    html += "    </li>\n";
    
    html += "  </ul>\n";
    html += "</div>\n";
    
    return html;
}

QString HtmlReportFormatter::formatStatistics(
    const std::vector<DriverInfo>& drivers,
    const SystemHealthResult& healthResult
) const
{
    // Count by importance
    int criticalImp = 0, importantImp = 0, optionalImp = 0, virtualImp = 0;
    for (const auto& driver : drivers) {
        switch (driver.importanceLevel) {
            case DriverImportance::Critical: criticalImp++; break;
            case DriverImportance::Important: importantImp++; break;
            case DriverImportance::Optional: optionalImp++; break;
            case DriverImportance::Virtual: virtualImp++; break;
            default: optionalImp++; break;
        }
    }
    
    // Count by health ranges
    int perfect = 0, excellent = 0, good = 0, critical = 0;
    for (const auto& driver : drivers) {
        if (driver.healthScore == 100) perfect++;
        else if (driver.healthScore >= 90) excellent++;
        else if (driver.healthScore >= 70) good++;
        else critical++;
    }
    
    // Count by installation age
    int lessThanMonth = 0, oneToSixMonths = 0, sixToTwelveMonths = 0, overOneYear = 0, unknownAge = 0;
    auto now = std::chrono::system_clock::now();
    auto nowDays = std::chrono::floor<std::chrono::days>(now);
    
    for (const auto& driver : drivers) {
        // Prefer install date, fall back to driver date
        std::optional<std::chrono::sys_days> dateToUse = driver.installDate;
        if (!dateToUse.has_value()) {
            dateToUse = driver.driverDate;
        }
        
        if (!dateToUse.has_value()) {
            unknownAge++;
            continue;
        }
        
        auto daysOld = std::chrono::duration_cast<std::chrono::days>(
            nowDays - dateToUse.value()
        ).count();
        
        if (daysOld < 30) lessThanMonth++;
        else if (daysOld < 180) oneToSixMonths++;
        else if (daysOld < 365) sixToTwelveMonths++;
        else overOneYear++;
    }
    
    int total = drivers.size();
    
    QString html;
    html += "<div class=\"section\">\n";
    html += "  <div class=\"section-header\">\n";
    html += "    <div class=\"section-title\">Statistics</div>\n";
    html += "  </div>\n";
    html += "  <div class=\"stats-grid\">\n";
    
    // Importance breakdown
    html += "    <div class=\"chart-container\">\n";
    html += "      <div class=\"chart-title\">Driver Importance</div>\n";
    html += "      <div class=\"bar-chart\">\n";
    
    auto makeBar = [total](const QString& label, int count) -> QString {
        double pct = total > 0 ? (count * 100.0 / total) : 0;
        QString bar;
        bar += "        <div class=\"bar-row\">\n";
        bar += "          <div class=\"bar-label\">" + label + "</div>\n";
        bar += "          <div class=\"bar-track\">\n";
        bar += "            <div class=\"bar-fill\" data-width=\"" + QString::number(pct, 'f', 1) + "%\" style=\"width: 0\"></div>\n";
        bar += "            <div class=\"bar-value\">" + QString::number(count) + "</div>\n";
        bar += "          </div>\n";
        bar += "        </div>\n";
        return bar;
    };
    
    html += makeBar("Critical", criticalImp);
    html += makeBar("Important", importantImp);
    html += makeBar("Optional", optionalImp);
    html += makeBar("Virtual", virtualImp);
    
    html += "      </div>\n";
    html += "    </div>\n";
    
    // Health distribution
    html += "    <div class=\"chart-container\">\n";
    html += "      <div class=\"chart-title\">Health Distribution</div>\n";
    html += "      <div class=\"bar-chart\">\n";
    
    html += makeBar("100%", perfect);
    html += makeBar("90-99%", excellent);
    html += makeBar("70-89%", good);
    html += makeBar("<70%", critical);
    
    html += "      </div>\n";
    html += "    </div>\n";
    
    // Installation age breakdown
    html += "    <div class=\"chart-container\">\n";
    html += "      <div class=\"chart-title\">Installation Age</div>\n";
    html += "      <div class=\"bar-chart\">\n";
    
    html += makeBar("< 1 month", lessThanMonth);
    html += makeBar("1-6 months", oneToSixMonths);
    html += makeBar("6-12 months", sixToTwelveMonths);
    html += makeBar("> 1 year", overOneYear);
    if (unknownAge > 0) {
        html += makeBar("Unknown", unknownAge);
    }
    
    html += "      </div>\n";
    html += "    </div>\n";
    
    html += "  </div>\n";
    html += "</div>\n";
    
    return html;
}

QString HtmlReportFormatter::formatEventLog(const std::vector<DriverInfo>& drivers, int scanWindowDays) const
{
    struct ErrorEntry {
        std::chrono::system_clock::time_point timestamp;
        std::wstring driverName;
        int eventId;
        std::wstring level;
        std::wstring message;
    };

    std::vector<ErrorEntry> allErrors;
    for (const auto& driver : drivers) {
        for (const auto& error : driver.errorLog) {
            allErrors.push_back({ error.timestamp, driver.name, error.eventId, error.level, error.message });
        }
    }

    // Sort by timestamp newest first
    std::sort(allErrors.begin(), allErrors.end(),
        [](const ErrorEntry& a, const ErrorEntry& b) { return a.timestamp > b.timestamp; });

    // ── Build stats grouped by eventId ────────────────────────────────────────
    struct Stat { std::wstring level; std::wstring message; int count = 0; };
    std::map<int, Stat> statsMap;
    int criticalCount = 0, warningCount = 0, infoCount = 0;

    for (const auto& e : allErrors) {
        auto& s = statsMap[e.eventId];
        s.count++;
        if (s.count == 1) { s.level = e.level; s.message = e.message; }
        if (e.level == L"Critical" || e.level == L"Error") criticalCount++;
        else if (e.level == L"Warning")                     warningCount++;
        else                                                infoCount++;
    }

    // Sort stats by count descending
    std::vector<std::pair<int, Stat>> sortedStats(statsMap.begin(), statsMap.end());
    std::sort(sortedStats.begin(), sortedStats.end(),
        [](const auto& a, const auto& b) { return a.second.count > b.second.count; });

    // ── Actionable events only (Critical/Warning) ─────────────────────────────
    std::vector<ErrorEntry> actionable;
    for (const auto& e : allErrors) {
        if (e.level == L"Critical" || e.level == L"Error" || e.level == L"Warning")
            actionable.push_back(e);
    }

    QString html;
    html += "<div class=\"section\">\n";
    html += "  <div class=\"section-header collapsible-header\">\n";
    html += "    <div class=\"section-title\">Event Log (Past " + QString::number(scanWindowDays) + " days)</div>\n";
    html += "    <div class=\"section-count\">" + QString::number(allErrors.size()) + " total &nbsp;|&nbsp; "
         + QString::number(criticalCount) + " critical &nbsp;|&nbsp; "
         + QString::number(warningCount)  + " warnings &nbsp;|&nbsp; "
         + QString::number(infoCount)     + " info</div>\n";
    html += "  </div>\n";
    html += "  <div class=\"collapsible-content\">\n";

    if (allErrors.empty()) {
        html += "    <div class=\"empty-state\">No driver-related events detected in the scan window</div>\n";
    } else {
        // Stats table
        html += "    <div class=\"event-stats\">\n";
        html += "      <div class=\"event-stats-title\">Event Frequency by ID</div>\n";
        html += "      <table class=\"event-stats-table\">\n";
        html += "        <thead><tr><th>Event ID</th><th>Severity</th><th>Count</th><th>Description</th></tr></thead>\n";
        html += "        <tbody>\n";
        for (const auto& [id, stat] : sortedStats) {
            QString levelClass = "info";
            if (stat.level == L"Critical" || stat.level == L"Error") levelClass = "error";
            else if (stat.level == L"Warning") levelClass = "warning";
            QString msg = canonicalEventDescription(id);
            if (msg.isEmpty()) {
                msg = escapeHtml(QString::fromStdWString(stat.message));
                if (msg.length() > 80) msg = msg.left(77) + "...";
            }
            html += "        <tr class=\"stat-row " + levelClass + "\">\n";
            html += "          <td class=\"stat-id\">Event " + QString::number(id) + "</td>\n";
            html += "          <td class=\"stat-level " + levelClass + "\">" + escapeHtml(QString::fromStdWString(stat.level)) + "</td>\n";
            html += "          <td class=\"stat-count\">" + QString::number(stat.count) + "×</td>\n";
            html += "          <td class=\"stat-msg\">" + msg + "</td>\n";
            html += "        </tr>\n";
        }
        html += "        </tbody>\n";
        html += "      </table>\n";
        html += "    </div>\n";

        // Critical/Warning raw list
        html += "    <div class=\"event-log-list\">\n";
        html += "      <div class=\"event-log-subtitle\">Critical &amp; Warning Events ("
             + QString::number(actionable.size()) + ")</div>\n";

        if (actionable.empty()) {
            html += "      <div class=\"empty-state\">No critical or warning events in this scan window</div>\n";
        } else {
            int shown = std::min(50, static_cast<int>(actionable.size()));
            for (int i = 0; i < shown; ++i) {
                const auto& error = actionable[i];
                auto timeT = std::chrono::system_clock::to_time_t(error.timestamp);
                std::tm tm;
                localtime_s(&tm, &timeT);
                char timeBuf[64];
                std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tm);
                QString timeStr = QString::fromUtf8(timeBuf);

                QString levelClass = (error.level == L"Critical" || error.level == L"Error") ? "error" : "warning";

                html += "      <div class=\"error-entry " + levelClass + "\">\n";
                html += "        <div class=\"error-header\">\n";
                html += "          <span class=\"error-time\">" + timeStr + "</span>\n";
                html += "          <span class=\"error-level\">" + escapeHtml(QString::fromStdWString(error.level)) + "</span>\n";
                html += "          <span class=\"error-id\">Event ID: " + QString::number(error.eventId) + "</span>\n";
                html += "        </div>\n";
                html += "        <div class=\"error-driver\">Driver: " + escapeHtml(QString::fromStdWString(error.driverName)) + "</div>\n";
                html += "        <div class=\"error-message\">" + escapeHtml(QString::fromStdWString(error.message)) + "</div>\n";
                html += "      </div>\n";
            }
            if (static_cast<int>(actionable.size()) > 50)
                html += "      <div class=\"empty-state\">... and " + QString::number(actionable.size() - 50) + " more</div>\n";
        }
        html += "    </div>\n";
    }

    html += "  </div>\n</div>\n";
    return html;
}

QString HtmlReportFormatter::formatDriverInventory(const std::vector<DriverInfo>& drivers) const
{
    auto categoriesMap = CategoryGrouper::groupByCategory(drivers);
    
    QString html;
    html += "<div class=\"section\">\n";
    html += "  <div class=\"section-header collapsible-header\">\n";
    html += "    <div class=\"section-title\">Driver Inventory by Category</div>\n";
    html += "    <div class=\"section-count\">" + QString::number(categoriesMap.size()) + " categories</div>\n";
    html += "  </div>\n";
    html += "  <div class=\"collapsible-content\">\n";
    html += "    <div class=\"category-grid\">\n";
    
    for (const auto& pair : categoriesMap) {
        const auto& category = pair.second;
        QString categoryName = QString::fromStdWString(category.displayName);
        int driverCount = category.drivers.size();
        
        // Count issues
        int criticalCount = 0, warningCount = 0;
        for (const auto& driver : category.drivers) {
            if (driver.healthScore < 70) criticalCount++;
            else if (driver.healthScore < 90) warningCount++;
        }
        
        html += "      <div class=\"category-card\">\n";
        html += "        <div class=\"category-header\">\n";
        html += "          <div class=\"category-name\">" + escapeHtml(categoryName) + "</div>\n";
        html += "          <div class=\"category-count\">" + QString::number(driverCount) + " driver" + (driverCount > 1 ? "s" : "") + "</div>\n";
        html += "        </div>\n";
        
        // Show issue counts or sample healthy drivers
        if (criticalCount > 0) {
            html += "        <div class=\"category-issues critical\">" + QString::number(criticalCount) + " critical</div>\n";
        } else if (warningCount > 0) {
            html += "        <div class=\"category-issues warning\">" + QString::number(warningCount) + " warning</div>\n";
        } else {
            // Show sample of healthy drivers (up to 3)
            html += "        <div class=\"category-sample\">\n";
            int displayCount = std::min(3, driverCount);
            for (int i = 0; i < displayCount; i++) {
                QString driverName = QString::fromStdWString(category.drivers[i].name);
                html += "          <div class=\"sample-driver\">✓ " + escapeHtml(driverName) + "</div>\n";
            }
            if (driverCount > 3) {
                html += "          <div class=\"sample-more\">... and " + QString::number(driverCount - 3) + " more</div>\n";
            }
            html += "        </div>\n";
        }
        
        html += "      </div>\n";
    }
    
    html += "    </div>\n";
    html += "  </div>\n";
    html += "</div>\n";
    
    return html;
}

QString HtmlReportFormatter::formatDriverCard(const DriverInfo& driver) const
{
    QString healthClass = getHealthClass(driver.healthScore);
    QString name = QString::fromStdWString(driver.name);
    QString category = QString::fromStdWString(driver.deviceClassName);
    QString manufacturer = QString::fromStdWString(driver.manufacturer);
    
    // Status
    QString statusText;
    switch (driver.status) {
        case DriverStatus::Ok: statusText = "OK"; break;
        case DriverStatus::Disabled: statusText = "Disabled"; break;
        case DriverStatus::NotStarted: statusText = "Not Started"; break;
        case DriverStatus::Error: statusText = "Error"; break;
        default: statusText = "Unknown"; break;
    }
    
    // Version
    QString versionText = "Unknown";
    if (driver.version.hasVersion) {
        versionText = QString("%1.%2.%3.%4")
            .arg(driver.version.major)
            .arg(driver.version.minor)
            .arg(driver.version.build)
            .arg(driver.version.revision);
    }
    
    QString html;
    html += "  <div class=\"driver-card " + healthClass + "\">\n";
    html += "    <div class=\"driver-header\">\n";
    html += "      <div class=\"driver-name\">" + escapeHtml(name) + "</div>\n";
    html += "      <div class=\"driver-health\">" + QString::number(driver.healthScore) + "%</div>\n";
    html += "    </div>\n";
    html += "    <div class=\"driver-details\">\n";
    html += "      <span>Category: " + escapeHtml(category) + "</span>\n";
    html += "      <span>Status: " + statusText + "</span>\n";
    if (manufacturer != "Unknown") {
        html += "      <span>Manufacturer: " + escapeHtml(manufacturer) + "</span>\n";
    }
    html += "      <span>Version: " + versionText + "</span>\n";
    html += "    </div>\n";
    
    // Flags
    if (!driver.healthFlags.empty()) {
        QStringList flagNames;
        for (const auto& flag : driver.healthFlags) {
            flagNames.append(QString::fromStdWString(flag.id));
        }
        html += "    <div class=\"driver-flags\">Flags: " + escapeHtml(flagNames.join(", ")) + "</div>\n";
    }
    
    html += "  </div>\n";
    return html;
}

QString HtmlReportFormatter::formatHtmlFooter() const
{
    QDateTime now = QDateTime::currentDateTime();
    QString reportId = now.toString("yyyyMMdd-HHmmss");
    
    QString footer = "<div style=\"text-align: center; padding: 40px 20px; color: #666; font-size: 12px;\">\n";
    footer += "  Generated by Driver Visualiser | Report ID: " + reportId + "\n";
    footer += "</div>\n";
    footer += "</body>\n</html>\n";
    
    return footer;
}

QString HtmlReportFormatter::getHealthClass(int healthScore) const
{
    if (healthScore < 70) return "critical";
    if (healthScore < 90) return "warning";
    return "healthy";
}

QString HtmlReportFormatter::getHealthLabel(int healthScore) const
{
    if (healthScore >= 90) return "System is Healthy";
    if (healthScore >= 70) return "System Needs Attention";
    return "Critical Issues Detected";
}

QString HtmlReportFormatter::escapeHtml(const QString& text) const
{
    QString escaped = text;
    escaped.replace("&", "&amp;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");
    escaped.replace("\"", "&quot;");
    escaped.replace("'", "&#39;");
    return escaped;
}