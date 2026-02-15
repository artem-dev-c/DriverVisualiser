#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "CategoryGrouper.h"
#include "CategoryProcessor.h"
#include "DriverScanner.h"
#include "DriverImportanceEvaluator.h"
#include "ErrorLogReader.h"
#include "HealthScoreEvaluator.h"
#include "CategorySectionWidget.h"
#include "SearchFilterBar.h"
#include <QScreen>
#include <QGuiApplication>
#include <algorithm>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    
    // Set default window size to 50% of screen width
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();
        int defaultWidth = screenGeometry.width() / 2;
        int defaultHeight = screenGeometry.height() * 3 / 4;  // 75% height
        resize(defaultWidth, defaultHeight);
        
        // Center the window
        move((screenGeometry.width() - defaultWidth) / 2,
             (screenGeometry.height() - defaultHeight) / 2);
    }
    
    populateDriverList();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::clearDriverList()
{
    // Clear all widgets from the layout
    QLayout* layout = ui->scrollAreaContents->layout();
    if (!layout) return;

    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}

void MainWindow::populateDriverList()
{
    clearDriverList();

    // Get or create the main layout
    QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(ui->scrollAreaContents->layout());
    if (!mainLayout) {
        mainLayout = new QVBoxLayout(ui->scrollAreaContents);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
    }

    // === Search/Filter Bar at Top ===
    SearchFilterBar* searchBar = new SearchFilterBar();
    
    // Wrap in centered container (70% width like sections)
    QWidget* searchWrapper = new QWidget();
    searchWrapper->setStyleSheet("QWidget { background-color: transparent; }");
    QHBoxLayout* searchWrapperLayout = new QHBoxLayout(searchWrapper);
    searchWrapperLayout->setContentsMargins(0, 0, 0, 0);
    searchWrapperLayout->setSpacing(0);
    searchWrapperLayout->addStretch(15);
    searchWrapperLayout->addWidget(searchBar, 70);
    searchWrapperLayout->addStretch(15);
    
    mainLayout->addWidget(searchWrapper);
    mainLayout->addSpacing(12);  // Space after search bar

    // Create centered wrapper widget with max width
    QWidget* wrapperWidget = new QWidget();
    wrapperWidget->setStyleSheet("QWidget { background-color: transparent; }");
    
    QHBoxLayout* wrapperLayout = new QHBoxLayout(wrapperWidget);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);
    wrapperLayout->setSpacing(0);
    
    // Left spacer (15%)
    wrapperLayout->addStretch(15);
    
    // Content container - 70% width (unified for sections and cards)
    QWidget* contentWidget = new QWidget();
    contentWidget->setStyleSheet("QWidget { background-color: transparent; }");
    
    QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(20, 20, 20, 20);
    contentLayout->setSpacing(8);  // Spacing between sections

    // Scan for drivers
    DriverScanner scanner;
    auto drivers = scanner.fetchDrivers();
    
    // Populate error logs
    ErrorLogReader::populateErrorLogs(drivers);
    
    // Evaluate importance and health
    for (auto& driver : drivers) {
        if (driver.importanceLevel == DriverImportance::Unknown) {
            driver.importanceLevel = DriverImportanceEvaluator::evaluate(driver);
        }
        
        HealthResult healthResult = HealthScoreEvaluator::evaluate(driver);
        driver.healthScore = healthResult.score;
        driver.healthFlags = healthResult.flags;
    }

    // Store all drivers for filtering
    m_allDrivers = drivers;
    
    // Extract unique manufacturers for filter
    std::set<std::wstring> manufacturers;
    for (const auto& driver : drivers) {
        if (!driver.manufacturer.empty() && driver.manufacturer != L"Unknown") {
            manufacturers.insert(driver.manufacturer);
        }
    }
    searchBar->setManufacturers(manufacturers);

    // Process categories
    auto rawCategories = CategoryGrouper::groupByCategory(drivers);
    auto processedCategories = CategoryProcessor::process(rawCategories);

    // Create section widgets
    for (const auto& category : processedCategories) {
        CategorySectionWidget* section = new CategorySectionWidget(category);
        contentLayout->addWidget(section);
    }

    contentLayout->addStretch();
    
    // Add content to wrapper (70% center)
    wrapperLayout->addWidget(contentWidget, 70);
    
    // Right spacer (15%)
    wrapperLayout->addStretch(15);
    
    // Add wrapper to main layout
    mainLayout->addWidget(wrapperWidget);
    
    // Store references for filtering
    m_searchBar = searchBar;
    m_contentLayout = contentLayout;
    
    // Connect search/filter signals
    connect(searchBar, &SearchFilterBar::searchChanged, this, [this]() {
        applyFilters();
    });
    
    connect(searchBar, &SearchFilterBar::filtersChanged, this, [this]() {
        applyFilters();
    });
}

void MainWindow::applyFilters()
{
    if (!m_searchBar || !m_contentLayout) return;
    
    QString searchText = m_searchBar->getSearchText().toLower();
    FilterPopupWidget::FilterState filters = m_searchBar->getFilterState();
    
    // Filter drivers
    std::vector<DriverInfo> filtered;
    
    for (const auto& driver : m_allDrivers) {
        // === Search filter ===
        if (!searchText.isEmpty()) {
            bool matches = false;
            
            // Search in: name, manufacturer, provider, device class
            if (QString::fromStdWString(driver.name).toLower().contains(searchText)) matches = true;
            if (QString::fromStdWString(driver.manufacturer).toLower().contains(searchText)) matches = true;
            if (QString::fromStdWString(driver.provider).toLower().contains(searchText)) matches = true;
            if (QString::fromStdWString(driver.deviceClassName).toLower().contains(searchText)) matches = true;
            
            if (!matches) continue;
        }
        
        // === Health filter (OR within group) ===
        // If ALL unchecked, show all (don't filter)
        bool hasHealthFilter = filters.showCritical || filters.showWarnings || filters.showHealthy;
        if (hasHealthFilter) {
            bool healthMatch = false;
            if (filters.showCritical && driver.healthScore < 70) healthMatch = true;
            if (filters.showWarnings && driver.healthScore >= 70 && driver.healthScore < 90) healthMatch = true;
            if (filters.showHealthy && driver.healthScore >= 90) healthMatch = true;
            if (!healthMatch) continue;
        }
        
        // === Importance filter (OR within group) ===
        // If ALL unchecked, show all (don't filter)
        bool hasImportanceFilter = filters.showCriticalDevices || filters.showImportantDevices || 
                                    filters.showOptionalDevices || filters.showVirtualDevices;
        if (hasImportanceFilter) {
            bool importanceMatch = false;
            if (filters.showCriticalDevices && driver.importanceLevel == DriverImportance::Critical) importanceMatch = true;
            if (filters.showImportantDevices && driver.importanceLevel == DriverImportance::Important) importanceMatch = true;
            if (filters.showOptionalDevices && driver.importanceLevel == DriverImportance::Optional) importanceMatch = true;
            if (filters.showVirtualDevices && driver.importanceLevel == DriverImportance::Virtual) importanceMatch = true;
            if (!importanceMatch) continue;
        }
        
        // === Manufacturer filter (OR within group) ===
        // If empty set, show all (don't filter)
        if (!filters.selectedManufacturers.empty()) {
            if (filters.selectedManufacturers.count(driver.manufacturer) == 0) {
                continue;
            }
        }
        
        // All filters passed
        filtered.push_back(driver);
    }
    
    // Clear current sections
    QLayoutItem* item;
    while ((item = m_contentLayout->takeAt(0)) != nullptr) {
        // Don't delete the stretch at the end
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
    
    if (filtered.empty()) {
        // Show "No results" message
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
        // Process filtered categories
        auto rawCategories = CategoryGrouper::groupByCategory(filtered);
        auto processedCategories = CategoryProcessor::process(rawCategories);

        // Create section widgets
        for (const auto& category : processedCategories) {
            CategorySectionWidget* section = new CategorySectionWidget(category);
            m_contentLayout->addWidget(section);
        }
    }
    
    m_contentLayout->addStretch();
}