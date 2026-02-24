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
#include "ClassNameMapper.h"
#include "ReportGenerator.h"
#include "ReportNotification.h"

#include <QScreen>
#include <QGuiApplication>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QMessageBox>
#include <algorithm>

// ============================================================================
// Construction / Destruction
// ============================================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();
        int defaultWidth  = screenGeometry.width() * 3 / 4;
        int defaultHeight = screenGeometry.height() * 3 / 4;
        resize(defaultWidth, defaultHeight);
        move((screenGeometry.width()  - defaultWidth)  / 2,
             (screenGeometry.height() - defaultHeight) / 2);
    }

    // Watcher: fires onScanComplete() on the main thread when background scan finishes
    m_scanWatcher = new QFutureWatcher<std::vector<DriverInfo>>(this);
    connect(m_scanWatcher, &QFutureWatcher<std::vector<DriverInfo>>::finished,
            this, &MainWindow::onScanComplete);

    m_systemInfo = SystemInfoCollector::collect();

    // Initialize class name mapper from JSON (must happen before first scan)
    // Non-fatal: falls back to raw class names if file is missing
    {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring dir(exePath);
        dir = dir.substr(0, dir.rfind(L'\\'));
        ClassNameMapper::initialize(dir + L"\\device_class_mappings.json");
    }

    // Show loading UI immediately, kick off async scan
    showLoadingState(true);
    auto future = QtConcurrent::run([this]() {
        return performScan(m_selectedLogDays);
    });
    m_scanWatcher->setFuture(future);
}

MainWindow::~MainWindow()
{
    // Wait for any in-flight scan to finish before destroying
    if (m_scanWatcher->isRunning()) {
        m_scanWatcher->waitForFinished();
    }
    delete ui;
}

// ============================================================================
// Async Scan
// ============================================================================

std::vector<DriverInfo> MainWindow::performScan(int logDays)
{
    // Runs on background thread — NO Qt UI calls allowed here
    DriverScanner scanner;
    auto drivers = scanner.fetchDrivers();

    ErrorLogReader::populateErrorLogs(drivers, logDays);

    for (auto& driver : drivers) {
        if (driver.importanceLevel == DriverImportance::Unknown) {
            driver.importanceLevel = DriverImportanceEvaluator::evaluate(driver);
        }
        HealthResult healthResult = HealthScoreEvaluator::evaluate(driver);
        driver.healthScore = healthResult.score;
        driver.healthFlags = healthResult.flags;
    }

    return drivers;
}

void MainWindow::onScanComplete()
{
    // Back on main thread — safe to update UI
    m_allDrivers = m_scanWatcher->result();
    m_scanning   = false;

    showLoadingState(false);
    populateDriverList();
}

// ============================================================================
// Loading State
// ============================================================================

void MainWindow::showLoadingState(bool loading)
{
    m_scanning = loading;

    QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(ui->scrollAreaContents->layout());
    if (!mainLayout) {
        mainLayout = new QVBoxLayout(ui->scrollAreaContents);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
    }

    if (loading) {
        // Clear any existing content
        QLayoutItem* item;
        while ((item = mainLayout->takeAt(0)) != nullptr) {
            if (item->widget()) delete item->widget();
            delete item;
        }

        // Build a vertically + horizontally centred loading card
        // Vertical centering: use a full-height container with stretch above and below
        m_loadingWidget = new QWidget();
        m_loadingWidget->setObjectName("loadingCard");
        m_loadingWidget->setStyleSheet(
            "QWidget#loadingCard {"
            "   background-color: #2b2b2b;"
            "   border: 1px solid #3d3d3d;"
            "   border-radius: 16px;"
            "}"
        );
        m_loadingWidget->setFixedSize(520, 200);

        QVBoxLayout* cardLayout = new QVBoxLayout(m_loadingWidget);
        cardLayout->setAlignment(Qt::AlignCenter);
        cardLayout->setContentsMargins(40, 32, 40, 32);
        cardLayout->setSpacing(14);

        // Animated dots label
        m_loadingLabel = new QLabel("Scanning drivers");
        QFont f = m_loadingLabel->font();
        f.setPointSize(16);
        f.setBold(true);
        m_loadingLabel->setFont(f);
        m_loadingLabel->setStyleSheet("color: #e0e0e0; background: transparent; border: none;");
        m_loadingLabel->setAlignment(Qt::AlignCenter);
        cardLayout->addWidget(m_loadingLabel);

        QLabel* subLabel = new QLabel("Reading system drivers and event logs...");
        QFont sf = subLabel->font();
        sf.setPointSize(10);
        subLabel->setFont(sf);
        subLabel->setStyleSheet("color: #666666; background: transparent; border: none;");
        subLabel->setAlignment(Qt::AlignCenter);
        cardLayout->addWidget(subLabel);

        // Thin animated progress bar (indeterminate)
        QProgressBar* bar = new QProgressBar();
        bar->setRange(0, 0);
        bar->setFixedHeight(4);
        bar->setTextVisible(false);
        bar->setStyleSheet(
            "QProgressBar {"
            "   background-color: #3a3a3a;"
            "   border: none;"
            "   border-radius: 2px;"
            "}"
            "QProgressBar::chunk {"
            "   background-color: #5dade2;"
            "   border-radius: 2px;"
            "}"
        );
        cardLayout->addWidget(bar);

        // Animate dots
        QTimer* dotTimer = new QTimer(m_loadingWidget);
        int* dotCount = new int(0);
        connect(dotTimer, &QTimer::timeout, this, [this, dotCount]() {
            if (!m_loadingLabel) return;
            *dotCount = (*dotCount + 1) % 4;
            m_loadingLabel->setText("Scanning drivers" + QString(".").repeated(*dotCount));
        });
        dotTimer->start(500);

        // Centre horizontally and vertically using nested stretches
        QWidget* outerWrapper = new QWidget();
        outerWrapper->setStyleSheet("QWidget { background-color: transparent; }");
        QVBoxLayout* outerV = new QVBoxLayout(outerWrapper);
        outerV->setContentsMargins(0, 0, 0, 0);
        outerV->addStretch();

        QHBoxLayout* hCentre = new QHBoxLayout();
        hCentre->addStretch();
        hCentre->addWidget(m_loadingWidget);
        hCentre->addStretch();
        outerV->addLayout(hCentre);
        outerV->addStretch();

        mainLayout->addWidget(outerWrapper, 1);  // 1 = expand to fill scroll area

    } else {
        // Loading widget is inside outerWrapper — clearDriverList() will handle cleanup
        m_loadingWidget = nullptr;
        m_loadingLabel  = nullptr;
    }
}

// ============================================================================
// Populate UI (called after scan completes)
// ============================================================================

void MainWindow::populateDriverList()
{
    clearDriverList();

    QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(ui->scrollAreaContents->layout());
    if (!mainLayout) {
        mainLayout = new QVBoxLayout(ui->scrollAreaContents);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
    }

    QWidget* outerWrapper = new QWidget();
    outerWrapper->setStyleSheet("QWidget { background-color: transparent; }");

    QHBoxLayout* outerWrapperLayout = new QHBoxLayout(outerWrapper);
    outerWrapperLayout->setContentsMargins(0, 0, 0, 0);
    outerWrapperLayout->setSpacing(0);
    outerWrapperLayout->addStretch(15);

    QWidget* contentWidget = new QWidget();
    contentWidget->setStyleSheet("QWidget { background-color: transparent; }");

    QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(20, 20, 20, 20);
    contentLayout->setSpacing(8);

    // =========================================================================
    // Dashboard
    // =========================================================================

    m_dashboard = new DashboardWidget();
    // Restore persisted log window selection BEFORE populate() so buttons show correctly
    m_dashboard->setSelectedLogDays(m_selectedLogDays);
    m_dashboard->populate(m_allDrivers, m_systemInfo);
    // Fix height after populate so filtering can't cause the dashboard to expand
    m_dashboard->setFixedHeight(m_dashboard->sizeHint().height());
    contentLayout->addWidget(m_dashboard);
    contentLayout->addSpacing(12);

    connect(m_dashboard, &DashboardWidget::scanRequested, this, [this]() {
        if (m_scanning) return;  // Ignore if already scanning
        m_scanning = true;

        // Disable rescan button visually
        if (m_dashboard) m_dashboard->setScanButtonEnabled(false);

        // Show loading overlay, then kick off async scan
        showLoadingState(true);
        auto future = QtConcurrent::run([this]() {
            return performScan(m_selectedLogDays);
        });
        m_scanWatcher->setFuture(future);
    });

    connect(m_dashboard, &DashboardWidget::logWindowChanged, this, [this](int days) {
        m_selectedLogDays = days;
    });

    connect(m_dashboard, &DashboardWidget::reportRequested, this, &MainWindow::onGenerateReportRequested);

    // =========================================================================
    // Search / Filter Bar
    // =========================================================================

    SearchFilterBar* searchBar = new SearchFilterBar();
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

    std::set<std::wstring> manufacturers;
    for (const auto& driver : m_allDrivers) {
        if (!driver.manufacturer.empty() && driver.manufacturer != L"Unknown")
            manufacturers.insert(driver.manufacturer);
    }
    searchBar->setManufacturers(manufacturers);

    auto rawCategories       = CategoryGrouper::groupByCategory(m_allDrivers);
    auto processedCategories = CategoryProcessor::process(rawCategories);

    for (const auto& category : processedCategories) {
        CategorySectionWidget* section = new CategorySectionWidget(category, m_selectedLogDays);
        contentLayout->addWidget(section);
    }

    contentLayout->addStretch();

    outerWrapperLayout->addWidget(contentWidget, 70);
    outerWrapperLayout->addStretch(15);
    mainLayout->addWidget(outerWrapper);

    m_searchBar     = searchBar;
    m_contentLayout = contentLayout;

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
        if (item->widget()) delete item->widget();
        delete item;
    }

    m_dashboard     = nullptr;
    m_searchBar     = nullptr;
    m_contentLayout = nullptr;
    m_loadingWidget = nullptr;
    m_loadingLabel  = nullptr;
}

// ============================================================================
// Filter
// ============================================================================

void MainWindow::applyFilters()
{
    if (!m_searchBar || !m_contentLayout) return;

    QString searchText = m_searchBar->getSearchText().toLower();
    FilterPopupWidget::FilterState filters = m_searchBar->getFilterState();

    std::vector<DriverInfo> filtered;

    for (const auto& driver : m_allDrivers) {

        if (!searchText.isEmpty()) {
            bool matches = false;
            if (QString::fromStdWString(driver.name).toLower().contains(searchText))            matches = true;
            if (QString::fromStdWString(driver.manufacturer).toLower().contains(searchText))    matches = true;
            if (QString::fromStdWString(driver.provider).toLower().contains(searchText))        matches = true;
            if (QString::fromStdWString(driver.deviceClassName).toLower().contains(searchText)) matches = true;
            if (!matches) continue;
        }

        bool hasHealthFilter = filters.showCritical || filters.showWarnings || filters.showHealthy;
        if (hasHealthFilter) {
            bool healthMatch = false;
            if (filters.showCritical && driver.healthScore < 70)                             healthMatch = true;
            if (filters.showWarnings && driver.healthScore >= 70 && driver.healthScore < 90) healthMatch = true;
            if (filters.showHealthy  && driver.healthScore >= 90)                            healthMatch = true;
            if (!healthMatch) continue;
        }

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

        if (!filters.selectedManufacturers.empty()) {
            if (filters.selectedManufacturers.count(driver.manufacturer) == 0) continue;
        }

        filtered.push_back(driver);
    }

    // Suppress repaints on both the scroll area and its viewport
    // to eliminate flash during widget removal + rebuild
    auto* scrollArea = ui->scrollAreaContents->parentWidget();
    if (scrollArea) scrollArea->setUpdatesEnabled(false);
    ui->scrollAreaContents->setUpdatesEnabled(false);

    constexpr int ITEMS_TO_KEEP = 4;
    int currentCount = m_contentLayout->count();
    for (int i = currentCount - 1; i >= ITEMS_TO_KEEP; --i) {
        QLayoutItem* item = m_contentLayout->takeAt(i);
        if (item->widget()) {
            item->widget()->hide();  // Hide before delete to avoid intermediate repaint
            delete item->widget();
        }
        delete item;
    }

    if (filtered.empty()) {
        QLabel* noResults = new QLabel("No drivers match your search/filter criteria");
        noResults->setStyleSheet(
            "QLabel { color: #888888; font-size: 14px; padding: 40px; }"
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

    // Re-enable painting — use update() to do a single clean repaint
    ui->scrollAreaContents->setUpdatesEnabled(true);
    if (scrollArea) {
        scrollArea->setUpdatesEnabled(true);
        scrollArea->update();
    }
}

// ============================================================================
// Report Generation
// ============================================================================

void MainWindow::onGenerateReportRequested(ReportFormat format)
{
    // Don't allow report generation during scan
    if (m_scanning) {
        QMessageBox::information(
            this,
            "Report Generation",
            "Please wait for the current scan to complete before generating a report."
        );
        return;
    }

    // Make sure we have driver data
    if (m_allDrivers.empty()) {
        QMessageBox::information(
            this,
            "Report Generation",
            "No driver data available. Please scan drivers first."
        );
        return;
    }

    // 1. Create report directory if it doesn't exist
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString reportDir = documentsPath + "/DriverVisualiser/Reports";
    
    QDir dir;
    if (!dir.mkpath(reportDir)) {
        QMessageBox::critical(
            this,
            "Report Generation Failed",
            "Could not create report directory:\n" + reportDir
        );
        return;
    }

    // 2. Generate filename with timestamp and format extension
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString extension = (format == ReportFormat::Html) ? ".html" : ".txt";
    QString fileName = QString("SystemReport_%1%2").arg(timestamp, extension);
    QString filePath = reportDir + "/" + fileName;

    // 3. Generate report content based on format
    ReportGenerator generator;
    QString reportText;
    
    if (format == ReportFormat::Html) {
        reportText = generator.generateHtmlReport(m_allDrivers, m_systemInfo, m_selectedLogDays);
    } else {
        reportText = generator.generateTextReport(m_allDrivers, m_systemInfo, m_selectedLogDays);
    }

    // 4. Save to file
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(
            this,
            "Report Generation Failed",
            "Could not write report file:\n" + filePath + "\n\nError: " + file.errorString()
        );
        return;
    }

    QTextStream out(&file);
    // Qt6 uses UTF-8 by default, no need to set codec
    out << reportText;
    file.close();

    // 5. Show success notification with actions
    // For HTML, hide copy button (no one wants to copy HTML source code)
    bool isHtml = (format == ReportFormat::Html);
    ReportNotification* notification = new ReportNotification(filePath, reportText, this, isHtml);
    notification->show(3500);  // Show for 3.5 seconds
}