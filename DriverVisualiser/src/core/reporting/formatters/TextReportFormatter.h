#pragma once

#include "IReportFormatter.h"

/**
 * @class TextReportFormatter
 * @brief Generates plain text (.txt) system health reports
 *
 * Produces a structured, human-readable text report with:
 * - System metadata header
 * - Health score summary
 * - Executive summary
 * - Critical issues (detailed)
 * - Warnings (if any)
 * - Driver inventory by category
 * - Event log summary
 * - Driver statistics
 * - Recommendations
 *
 * Output is optimized for:
 * - Copy-paste into support tickets
 * - AI assistant analysis
 * - Quick sharing via email/Discord/forums
 * - Reading in any text editor
 */
class TextReportFormatter : public IReportFormatter
{
public:
    QString format(
        const std::vector<DriverInfo>& drivers,
        const SystemInfo& systemInfo,
        const SystemHealthResult& healthResult,
        int scanWindowDays
    ) const override;

    QString fileExtension() const override { return "txt"; }
    QString mimeType() const override { return "text/plain"; }

private:
    /// Generate the header section (title, metadata, system info)
    QString formatHeader(const SystemInfo& sysInfo, int scanWindowDays) const;

    /// Generate health score summary box
    QString formatHealthSummary(const SystemHealthResult& healthResult) const;

    /// Generate executive summary paragraph
    QString formatExecutiveSummary(const SystemHealthResult& healthResult) const;

    /// Generate critical issues section with full details
    QString formatCriticalIssues(const std::vector<DriverInfo>& drivers) const;

    /// Generate warnings section
    QString formatWarnings(const std::vector<DriverInfo>& drivers) const;

    /// Generate driver inventory grouped by category
    QString formatDriverInventory(const std::vector<DriverInfo>& drivers) const;

    /// Generate event log summary statistics
    QString formatEventLogSummary(const std::vector<DriverInfo>& drivers, int scanWindowDays) const;

    /// Generate driver statistics (counts, health distribution, installation age)
    QString formatDriverStatistics(const std::vector<DriverInfo>& drivers) const;

    /// Generate recommendations based on detected issues
    QString formatRecommendations(const std::vector<DriverInfo>& drivers) const;

    /// Generate footer with report ID and timestamp
    QString formatFooter() const;

    /// Helper: Create a visual ASCII bar chart for health distribution
    QString createHealthDistributionChart(const std::vector<DriverInfo>& drivers) const;

    /// Helper: Format driver details for critical/warning sections
    QString formatDriverDetails(const DriverInfo& driver) const;

    /// Helper: Create a separator line
    QString separator(int length = 60, QChar ch = QChar('-')) const;
};