#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "CategoryGrouper.h"
#include "CategoryProcessor.h"
#include "DriverScanner.h"
#include "DriverImportanceEvaluator.h"
#include "ErrorLogReader.h"
#include "HealthScoreEvaluator.h"
#include "SystemInfoCollector.h"
#include "CategorySectionWidget.h"
#include "DashboardWidget.h"
#include "SearchFilterBar.h"

#include <QScreen>
#include <QGuiApplication>
#include <QApplication>
#include <algorithm>

// ============================================================================
// Construction / Destruction
// ============================================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Set default window size: 50% width, 75% height, centered
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();
        int defaultWidth  = screenGeometry.width()  / 2;
        int defaultHeight = screenGeometry.height() * 3 / 4;
        resize(defaultWidth, defaultHeight);
        move((screenGeometry.width()  - defaultWidth)  / 2,
             (screenGeometry.height() - defaultHeight) / 2);
    }

    // Collect system info once (doesn't change during session)
    m_systemInfo = SystemInfoCollector::collect();

    // Initial scan with default 7-day log window
    populateDriverList();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ============================================================================
// Scan + Populate
// ============================================================================

void MainWindow::runScan(int logDays)
{
    // === Step 1: Scan drivers ===
    DriverScanner scanner;
    auto drivers = scanner.fetchDrivers();

    // === Step 2: Populate error logs with chosen day window ===
    ErrorLogReader::populateErrorLogs(drivers, logDays);

    // === Step 3: Evaluate importance and health for each driver ===
    for (auto& driver : drivers) {
        if (driver.importanceLevel == DriverImportance::Unknown) {
            driver.importanceLevel = DriverImportanceEvaluator::evaluate(driver);
        }

        HealthResult healthResult = HealthScoreEvaluator::evaluate(driver);
        driver.healthScore  = healthResult.score;
        driver.healthFlags  = healthResult.flags;
    }

    m_allDrivers = drivers;
}

void MainWindow::populateDriverList()
{
    clearDriverList();

    // Run full scan using the persisted log window selection.
    // We read m_selectedLogDays (not m_dashboard) because clearDriverList()
    // deletes m_dashboard before we get here, losing the selection.
    runScan(m_selectedLogDays);

    // Get/create the main scroll area layout
    QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(ui->scrollAreaContents->layout());
    if (!mainLayout) {
        mainLayout = new QVBoxLayout(ui->scrollAreaContents);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
    }

    // =========================================================================
    // Centered wrapper (70% width) — used for all content
    // =========================================================================

    QWidget* outerWrapper = new QWidget();
    outerWrapper->setStyleSheet("QWidget { background-color: transparent; }");

    QHBoxLayout* outerWrapperLayout = new QHBoxLayout(outerWrapper);
    outerWrapperLayout->setContentsMargins(0, 0, 0, 0);
    outerWrapperLayout->setSpacing(0);
    outerWrapperLayout->addStretch(15);

    // Content container (70% of scroll area width)
    QWidget* contentWidget = new QWidget();
    contentWidget->setStyleSheet("QWidget { background-color: transparent; }");

    QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(20, 20, 20, 20);
    contentLayout->setSpacing(8);

    // =========================================================================
    // Dashboard (top of content area)
    // =========================================================================

    m_dashboard = new DashboardWidget();
    m_dashboard->populate(m_allDrivers, m_systemInfo);
    contentLayout->addWidget(m_dashboard);
    contentLayout->addSpacing(12);

    // Connect rescan signal
    connect(m_dashboard, &DashboardWidget::scanRequested, this, [this]() {
        populateDriverList();
    });

    // Persist the log window selection so it survives the next rescan
    // (m_dashboard is deleted and recreated on each rescan, so we store
    // the value in MainWindow::m_selectedLogDays instead)
    connect(m_dashboard, &DashboardWidget::logWindowChanged, this, [this](int days) {
        m_selectedLogDays = days;
    });

    // =========================================================================
    // Search / Filter Bar
    // =========================================================================

    SearchFilterBar* searchBar = new SearchFilterBar();

    // Wrap in same centered row as content
    QWidget* searchWrapper = new QWidget();
    searchWrapper->setStyleSheet("QWidget { background-color: transparent; }");
    QVBoxLayout* searchWrapperLayout = new QVBoxLayout(searchWrapper);
    searchWrapperLayout->setContentsMargins(0, 0, 0, 0);
    searchWrapperLayout->addWidget(searchBar);

    contentLayout->addWidget(searchWrapper);
    contentLayout->addSpacing(8);

    // =========================================================================
    // Category sections
    // =========================================================================

    // Extract unique manufacturers for filter popup
    std::set<std::wstring> manufacturers;
    for (const auto& driver : m_allDrivers) {
        if (!driver.manufacturer.empty() && driver.manufacturer != L"Unknown") {
            manufacturers.insert(driver.manufacturer);
        }
    }
    searchBar->setManufacturers(manufacturers);

    // Process categories → sorted category widgets
    auto rawCategories        = CategoryGrouper::groupByCategory(m_allDrivers);
    auto processedCategories  = CategoryProcessor::process(rawCategories);

    for (const auto& category : processedCategories) {
        CategorySectionWidget* section = new CategorySectionWidget(category, m_selectedLogDays);
        contentLayout->addWidget(section);
    }

    contentLayout->addStretch();

    // Assemble centered wrapper
    outerWrapperLayout->addWidget(contentWidget, 70);
    outerWrapperLayout->addStretch(15);

    mainLayout->addWidget(outerWrapper);

    // Store references needed for applyFilters()
    m_searchBar    = searchBar;
    m_contentLayout = contentLayout;

    // Connect search / filter signals → live filtering
    connect(searchBar, &SearchFilterBar::searchChanged,  this, [this]() { applyFilters(); });
    connect(searchBar, &SearchFilterBar::filtersChanged, this, [this]() { applyFilters(); });
}

// ============================================================================
// Clear
// ============================================================================

void MainWindow::clearDriverList()
{
    QLayout* layout = ui->scrollAreaContents->layout();
    if (!layout) return;

    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    // Reset pointers — widgets were just deleted above
    m_dashboard     = nullptr;
    m_searchBar     = nullptr;
    m_contentLayout = nullptr;
}

// ============================================================================
// Filter
// ============================================================================

void MainWindow::applyFilters()
{
    if (!m_searchBar || !m_contentLayout) return;

    QString searchText = m_searchBar->getSearchText().toLower();
    FilterPopupWidget::FilterState filters = m_searchBar->getFilterState();

    // Build filtered driver list
    std::vector<DriverInfo> filtered;

    for (const auto& driver : m_allDrivers) {

        // === Text search ===
        if (!searchText.isEmpty()) {
            bool matches = false;
            if (QString::fromStdWString(driver.name).toLower().contains(searchText))            matches = true;
            if (QString::fromStdWString(driver.manufacturer).toLower().contains(searchText))    matches = true;
            if (QString::fromStdWString(driver.provider).toLower().contains(searchText))        matches = true;
            if (QString::fromStdWString(driver.deviceClassName).toLower().contains(searchText)) matches = true;
            if (!matches) continue;
        }

        // === Health filter (OR within group; all unchecked = show all) ===
        bool hasHealthFilter = filters.showCritical || filters.showWarnings || filters.showHealthy;
        if (hasHealthFilter) {
            bool healthMatch = false;
            if (filters.showCritical && driver.healthScore < 70)                              healthMatch = true;
            if (filters.showWarnings && driver.healthScore >= 70 && driver.healthScore < 90)  healthMatch = true;
            if (filters.showHealthy  && driver.healthScore >= 90)                             healthMatch = true;
            if (!healthMatch) continue;
        }

        // === Importance filter (OR within group; all unchecked = show all) ===
        bool hasImportanceFilter = filters.showCriticalDevices || filters.showImportantDevices ||
                                   filters.showOptionalDevices  || filters.showVirtualDevices;
        if (hasImportanceFilter) {
            bool importanceMatch = false;
            if (filters.showCriticalDevices  && driver.importanceLevel == DriverImportance::Critical)  importanceMatch = true;
            if (filters.showImportantDevices && driver.importanceLevel == DriverImportance::Important) importanceMatch = true;
            if (filters.showOptionalDevices  && driver.importanceLevel == DriverImportance::Optional)  importanceMatch = true;
            if (filters.showVirtualDevices   && driver.importanceLevel == DriverImportance::Virtual)   importanceMatch = true;
            if (!importanceMatch) continue;
        }

        // === Manufacturer filter (empty = show all) ===
        if (!filters.selectedManufacturers.empty()) {
            if (filters.selectedManufacturers.count(driver.manufacturer) == 0) continue;
        }

        filtered.push_back(driver);
    }

    // Clear only the category section widgets below the search bar
    // (dashboard + search bar are at indices 0 and 1 in contentLayout — skip them)
    // We do this by removing everything from the bottom, stopping before search bar
    //
    // Layout order in contentLayout:
    //   [0] DashboardWidget
    //   [1] spacing
    //   [2] SearchFilterBar wrapper
    //   [3] spacing
    //   [4..N] CategorySectionWidgets + stretch
    //
    // We need to preserve 0-3 and replace 4+
    // The cleanest way: remove from the end until we hit the search bar index

    // Count items to keep (dashboard + its spacing + searchbar + its spacing = 4 items)
    constexpr int ITEMS_TO_KEEP = 4;
    int currentCount = m_contentLayout->count();

    for (int i = currentCount - 1; i >= ITEMS_TO_KEEP; --i) {
        QLayoutItem* item = m_contentLayout->takeAt(i);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    // Populate with filtered results
    if (filtered.empty()) {
        QLabel* noResults = new QLabel("No drivers match your search/filter criteria");
        noResults->setStyleSheet(
            "QLabel {"
            "   color: #888888;"
            "   font-size: 14px;"
            "   padding: 40px;"
            "}"
        );
        noResults->setAlignment(Qt::AlignCenter);
        m_contentLayout->addWidget(noResults);
    } else {
        auto rawCategories       = CategoryGrouper::groupByCategory(filtered);
        auto processedCategories = CategoryProcessor::process(rawCategories);

        for (const auto& category : processedCategories) {
            CategorySectionWidget* section = new CategorySectionWidget(category, m_selectedLogDays);
            m_contentLayout->addWidget(section);
        }
    }

    m_contentLayout->addStretch();
}