#pragma once

#include <QMainWindow>
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <vector>
#include "DriverInfo.h"
#include "SystemInfo.h"
#include "DashboardWidget.h"
#include "CategorySectionWidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class CategorySectionWidget;
class SearchFilterBar;
class DashboardWidget;
class EventTimelineWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void populateDriverList();
    void clearDriverList();
    void applyFilters();
    void applyWindowTheme();

    /// Show/hide the loading overlay
    void showLoadingState(bool loading);

    /// Called on background thread — returns scanned drivers
    std::vector<DriverInfo> performScan(int logDays);

    /// Called on main thread when scan completes
    void onScanComplete();

    /// Called when user clicks Generate Report button
    void onGenerateReportRequested(ReportFormat format);

    Ui::MainWindow *ui;

    // --- Widgets ---
    DashboardWidget*     m_dashboard        = nullptr;
    EventTimelineWidget* m_timeline         = nullptr;
    SearchFilterBar*     m_searchBar        = nullptr;
    QVBoxLayout*         m_contentLayout    = nullptr;
    QWidget*             m_loadingWidget    = nullptr;
    QLabel*              m_loadingLabel     = nullptr;

    /// Live category section widgets — kept alive for fast show/hide filtering
    std::vector<CategorySectionWidget*> m_categorySections;

    // --- Async scan ---
    QFutureWatcher<std::vector<DriverInfo>>* m_scanWatcher = nullptr;
    bool m_scanning = false;

    // --- Data ---
    std::vector<DriverInfo> m_allDrivers;
    std::vector<ErrorLogEntry> m_allEvents;  ///< All events for timeline (includes SystemWide)
    SystemInfo              m_systemInfo;
    int                     m_selectedLogDays  = 30;  ///< Currently selected log window (may differ from last scan)
    int                     m_lastScanLogDays  = 30;  ///< Log days actually used in the most recent scan
};