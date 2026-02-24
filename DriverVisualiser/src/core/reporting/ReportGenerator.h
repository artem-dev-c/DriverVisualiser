#pragma once

#include <QString>
#include <memory>
#include <vector>
#include "DriverInfo.h"
#include "SystemInfo.h"
#include "SystemHealthSummary.h"

class IReportFormatter;

/**
 * @class ReportGenerator
 * @brief Main orchestrator for system health report generation
 *
 * This class:
 * 1. Takes scanned driver data and system info
 * 2. Calculates health summary statistics
 * 3. Delegates formatting to IReportFormatter implementations
 * 4. Returns formatted report strings ready for saving
 *
 * Usage:
 *   ReportGenerator generator;
 *   QString report = generator.generateTextReport(drivers, systemInfo, scanDays);
 *   // Save report to file or copy to clipboard
 *
 * Future: Support multiple formats (HTML, PDF, JSON) via different formatters
 */
class ReportGenerator
{
public:
    ReportGenerator();
    ~ReportGenerator();

    /**
     * @brief Generate a plain text system health report
     * @param drivers All scanned drivers with health scores
     * @param systemInfo System information (OS, build, architecture)
     * @param scanWindowDays Number of days scanned for error logs (default: 7)
     * @return Formatted text report ready to save or display
     */
    QString generateTextReport(
        const std::vector<DriverInfo>& drivers,
        const SystemInfo& systemInfo,
        int scanWindowDays = 7
    ) const;

    /**
     * @brief Generate an interactive HTML system health report
     * @param drivers All scanned drivers with health scores
     * @param systemInfo System information (OS, build, architecture)
     * @param scanWindowDays Number of days scanned for error logs (default: 7)
     * @return Formatted HTML report ready to save
     */
    QString generateHtmlReport(
        const std::vector<DriverInfo>& drivers,
        const SystemInfo& systemInfo,
        int scanWindowDays = 7
    ) const;

    /**
     * @brief Generate report using a custom formatter
     * @param drivers All scanned drivers
     * @param systemInfo System information
     * @param scanWindowDays Scan window in days
     * @param formatter Custom formatter implementation
     * @return Formatted report string
     */
    QString generateReport(
        const std::vector<DriverInfo>& drivers,
        const SystemInfo& systemInfo,
        int scanWindowDays,
        const IReportFormatter& formatter
    ) const;
};