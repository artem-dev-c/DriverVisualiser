#pragma once

#include <QString>
#include <vector>
#include "DriverInfo.h"
#include "SystemInfo.h"
#include "SystemHealthSummary.h"

/**
 * @interface IReportFormatter
 * @brief Interface for different report output formats (text, HTML, PDF, etc.)
 *
 * Implementations format system health data into specific output types.
 * The formatter receives pre-calculated health summary and driver data,
 * then generates a formatted report string.
 */
class IReportFormatter
{
public:
    virtual ~IReportFormatter() = default;

    /**
     * @brief Generate a formatted report string
     * @param drivers All scanned drivers with health scores
     * @param systemInfo System information (OS, build, architecture)
     * @param healthResult Pre-calculated health result and statistics
     * @param scanWindowDays Number of days scanned for error logs
     * @return Formatted report string ready for saving or displaying
     */
    virtual QString format(
        const std::vector<DriverInfo>& drivers,
        const SystemInfo& systemInfo,
        const SystemHealthResult& healthResult,
        int scanWindowDays
    ) const = 0;

    /**
     * @brief Get the file extension for this format (e.g., "txt", "html", "pdf")
     */
    virtual QString fileExtension() const = 0;

    /**
     * @brief Get the MIME type for this format (e.g., "text/plain", "text/html")
     */
    virtual QString mimeType() const = 0;
};