#pragma once

#include <QMainWindow>
#include <QVBoxLayout>
#include <vector>
#include "DriverInfo.h"
#include "SystemInfo.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class CategorySectionWidget;
class SearchFilterBar;
class DashboardWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    void populateDriverList();
    void clearDriverList();
    void applyFilters();

    /// Run a full scan (drivers + event log) and refresh the entire UI
    /// @param logDays  Event log lookback window in days (7, 30, or 90)
    void runScan(int logDays);

    // --- Widgets ---
    DashboardWidget* m_dashboard    = nullptr;
    SearchFilterBar* m_searchBar    = nullptr;
    QVBoxLayout*     m_contentLayout = nullptr;

    // --- Data ---
    std::vector<DriverInfo> m_allDrivers;
    SystemInfo              m_systemInfo;
    int                     m_selectedLogDays = 7;  ///< Persisted log window (survives rescan)
};