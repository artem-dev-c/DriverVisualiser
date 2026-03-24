#include "ReportGenerator.h"
#include "IReportFormatter.h"
#include "TextReportFormatter.h"
#include "HtmlReportFormatter.h"
#include "SystemHealthSummary.h"

ReportGenerator::ReportGenerator()
{
}

ReportGenerator::~ReportGenerator()
{
}

QString ReportGenerator::generateTextReport(
    const std::vector<DriverInfo>& drivers,
    const SystemInfo& systemInfo,
    const std::vector<ErrorLogEntry>& allEvents,
    int scanWindowDays
) const
{
    TextReportFormatter formatter;
    return generateReport(drivers, systemInfo, allEvents, scanWindowDays, formatter);
}

QString ReportGenerator::generateHtmlReport(
    const std::vector<DriverInfo>& drivers,
    const SystemInfo& systemInfo,
    const std::vector<ErrorLogEntry>& allEvents,
    int scanWindowDays
) const
{
    HtmlReportFormatter formatter;
    return generateReport(drivers, systemInfo, allEvents, scanWindowDays, formatter);
}

QString ReportGenerator::generateReport(
    const std::vector<DriverInfo>& drivers,
    const SystemInfo& systemInfo,
    const std::vector<ErrorLogEntry>& allEvents,
    int scanWindowDays,
    const IReportFormatter& formatter
) const
{
    // Calculate health summary with SystemWide event data
    SystemHealthResult healthResult = SystemHealthSummary::compute(
        drivers,
        allEvents,
        scanWindowDays
    );
    
    // Delegate formatting to the provided formatter
    return formatter.format(drivers, systemInfo, healthResult, scanWindowDays);
}