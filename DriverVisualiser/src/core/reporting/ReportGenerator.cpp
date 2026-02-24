#include "ReportGenerator.h"
#include "IReportFormatter.h"
#include "TextReportFormatter.h"
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
    int scanWindowDays
) const
{
    TextReportFormatter formatter;
    return generateReport(drivers, systemInfo, scanWindowDays, formatter);
}

QString ReportGenerator::generateReport(
    const std::vector<DriverInfo>& drivers,
    const SystemInfo& systemInfo,
    int scanWindowDays,
    const IReportFormatter& formatter
) const
{
    // Calculate health summary from driver data using existing SystemHealthSummary class
    SystemHealthResult healthResult = SystemHealthSummary::compute(drivers);
    
    // Delegate formatting to the provided formatter
    return formatter.format(drivers, systemInfo, healthResult, scanWindowDays);
}