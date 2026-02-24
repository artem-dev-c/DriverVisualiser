#pragma once

#include "IReportFormatter.h"

/**
 * @class HtmlReportFormatter
 * @brief Generates interactive HTML system health reports
 *
 * Produces a modern, dashboard-style HTML report with:
 * - Visual health score indicator (CSS donut chart)
 * - Color-coded driver categories
 * - Collapsible sections for driver inventory
 * - CSS-only bar charts for statistics
 * - Dark theme matching the application
 * - Fully self-contained (inline CSS/JS, no external dependencies)
 *
 * Output is optimized for:
 * - Viewing in any modern browser
 * - Printing to PDF
 * - Screenshots for documentation
 * - Professional presentation
 */
class HtmlReportFormatter : public IReportFormatter
{
public:
    QString format(
        const std::vector<DriverInfo>& drivers,
        const SystemInfo& systemInfo,
        const SystemHealthResult& healthResult,
        int scanWindowDays
    ) const override;

    QString fileExtension() const override { return "html"; }
    QString mimeType() const override { return "text/html"; }

private:
    /// Generate HTML header with title, meta tags, and embedded CSS
    QString formatHtmlHeader() const;

    /// Generate embedded CSS styles
    QString formatStyles() const;

    /// Generate embedded JavaScript for interactivity
    QString formatScripts() const;

    /// Generate dashboard header with health score and quick stats
    QString formatDashboardHeader(const SystemHealthResult& healthResult, 
                                   const SystemInfo& systemInfo,
                                   int scanWindowDays) const;

    /// Generate critical issues section
    QString formatCriticalSection(const std::vector<DriverInfo>& drivers) const;

    /// Generate warnings section
    QString formatWarningsSection(const std::vector<DriverInfo>& drivers) const;

    /// Generate recommendations call-out box
    QString formatRecommendations(const std::vector<DriverInfo>& drivers) const;

    /// Generate driver inventory (collapsible by category)
    QString formatDriverInventory(const std::vector<DriverInfo>& drivers) const;

    /// Generate statistics section with charts
    QString formatStatistics(const std::vector<DriverInfo>& drivers,
                             const SystemHealthResult& healthResult) const;

    /// Generate event log summary
    QString formatEventLog(const std::vector<DriverInfo>& drivers, int scanWindowDays) const;

    /// Generate HTML footer
    QString formatHtmlFooter() const;

    /// Helper: Format individual driver card
    QString formatDriverCard(const DriverInfo& driver) const;

    /// Helper: Get CSS class for health score
    QString getHealthClass(int healthScore) const;

    /// Helper: Get health label text
    QString getHealthLabel(int healthScore) const;

    /// Helper: Escape HTML special characters
    QString escapeHtml(const QString& text) const;
};
