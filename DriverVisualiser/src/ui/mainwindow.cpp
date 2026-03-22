#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "AppTheme.h"
#include "CategoryGrouper.h"
#include "CategoryProcessor.h"
#include "DriverScanner.h"
#include "DriverImportanceEvaluator.h"
#include "ErrorLogReader.h"
#include "HealthScoreEvaluator.h"
#include "SystemInfoCollector.h"
#include "CategorySectionWidget.h"
#include "DashboardWidget.h"
#include "EventTimelineWidget.h"
#include "SearchFilterBar.h"
#include "ClassNameMapper.h"
#include "ReportGenerator.h"
#include "ReportNotification.h"

#include <QScreen>
#include <QGuiApplication>
#include <QStyleHints>
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

    // ── Theme ────────────────────────────────────────────────────────────────
    AppTheme::init();
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this](Qt::ColorScheme) {
        AppTheme::init();
        applyWindowTheme();
        if (!m_scanning && !m_allDrivers.empty())
            populateDriverList();
    });
    applyWindowTheme();

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
// Theme
// ============================================================================

void MainWindow::applyWindowTheme()
{
    const auto& c = AppTheme::colors();
    ui->scrollArea->setStyleSheet(QString(
        "QScrollArea { background-color: %1; border: none; }"
    ).arg(c.bgBase) + AppTheme::scrollbarStyleSheet());
    ui->scrollArea->viewport()->setStyleSheet(
        QString("background-color: %1;").arg(c.bgBase));
    ui->scrollAreaContents->setStyleSheet(
        QString("QWidget { background-color: %1; }").arg(c.bgBase));
    centralWidget()->setStyleSheet(
        QString("QWidget { background-color: %1; }").arg(c.bgBase));

    // Global tooltip styling — matches current theme so it's never a jarring
    // dark rectangle on light mode or vice versa
    qApp->setStyleSheet(QString(
        "QToolTip {"
        "   background-color: %1;"
        "   color: %2;"
        "   border: 1px solid %3;"
        "   padding: 4px 8px;"
        "   border-radius: 4px;"
        "   font-size: 11px;"
        "}"
    ).arg(c.bgOverlay, c.textPrimary, c.borderStrong));
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
        // Store the scan window used for error logs
        driver.errorLogWindowDays = logDays;
        
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
    m_allDrivers       = m_scanWatcher->result();
    m_lastScanLogDays  = m_selectedLogDays;  // Record the days used for this scan
    m_scanning         = false;

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
        m_loadingWidget->setStyleSheet(QString(
            "QWidget#loadingCard {"
            "   background-color: %1;"
            "   border: 1px solid %2;"
            "   border-radius: 16px;"
            "}"
        ).arg(AppTheme::colors().bgElevated, AppTheme::colors().borderNormal));
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
        m_loadingLabel->setStyleSheet(QString("color: %1; background: transparent; border: none;").arg(AppTheme::colors().textPrimary));
        m_loadingLabel->setAlignment(Qt::AlignCenter);
        cardLayout->addWidget(m_loadingLabel);

        QLabel* subLabel = new QLabel("Reading system drivers and event logs...");
        QFont sf = subLabel->font();
        sf.setPointSize(10);
        subLabel->setFont(sf);
        subLabel->setStyleSheet(QString("color: %1; background: transparent; border: none;").arg(AppTheme::colors().textSecondary));
        subLabel->setAlignment(Qt::AlignCenter);
        cardLayout->addWidget(subLabel);

        // Thin animated progress bar (indeterminate)
        QProgressBar* bar = new QProgressBar();
        bar->setRange(0, 0);
        bar->setFixedHeight(4);
        bar->setTextVisible(false);
        bar->setStyleSheet(QString(
            "QProgressBar {"
            "   background-color: %1;"
            "   border: none;"
            "   border-radius: 2px;"
            "}"
            "QProgressBar::chunk {"
            "   background-color: %2;"
            "   border-radius: 2px;"
            "}"
        ).arg(AppTheme::colors().bgButtonNeutral, AppTheme::colors().accent));
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

    connect(m_dashboard, &DashboardWidget::reportRequested, this, &MainWindow::onGenerateReportRequested);

    // =========================================================================
    // Event Timeline — header row (on bgBase) + chart card below
    // =========================================================================

    // Aggregate all error logs from all drivers and populate driver names
    std::vector<ErrorLogEntry> allErrorLogs;
    for (const auto& driver : m_allDrivers) {
        for (auto entry : driver.errorLog) {
            entry.driverName = driver.name;
            allErrorLogs.push_back(entry);
        }
    }

    // --- Chart widget ---
    m_timeline = new EventTimelineWidget();
    m_timeline->setDayRange(m_selectedLogDays);
    m_timeline->setEvents(allErrorLogs);

    // --- Header row (sits on bgBase, above the card) ---
    QWidget* timelineHeader = new QWidget();
    timelineHeader->setStyleSheet("background: transparent;");
    QHBoxLayout* headerLayout = new QHBoxLayout(timelineHeader);
    headerLayout->setContentsMargins(4, 0, 4, 0);
    headerLayout->setSpacing(0);

    // Left side: "Past 30 Days" title + total count
    auto* titleLabel = new QLabel(QString("Past %1 Days").arg(m_selectedLogDays));
    {
        QFont f = titleLabel->font();
        f.setPointSize(10);
        f.setWeight(QFont::DemiBold);
        titleLabel->setFont(f);
        titleLabel->setStyleSheet(QString("color: %1; background: transparent;")
            .arg(AppTheme::colors().textPrimary));
    }

    auto* separatorDot = new QLabel(" · ");
    separatorDot->setStyleSheet(QString("color: %1; background: transparent;")
        .arg(AppTheme::colors().textMuted));

    const auto totals = m_timeline->getTotals();
    auto* totalLabel = new QLabel(QString("%1 events").arg(totals.total()));
    {
        QFont f = totalLabel->font();
        f.setPointSize(10);
        totalLabel->setFont(f);
        totalLabel->setStyleSheet(QString("color: %1; background: transparent;")
            .arg(AppTheme::colors().textSecondary));
    }

    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(separatorDot);
    headerLayout->addWidget(totalLabel);
    headerLayout->addStretch();

    // Right side: legend entries — coloured square + label text + count
    auto makeLegendEntry = [&](const QString& color, const QString& labelText, int count) {
        QWidget* entry = new QWidget();
        entry->setStyleSheet("background: transparent;");
        QHBoxLayout* el = new QHBoxLayout(entry);
        el->setContentsMargins(0, 0, 0, 0);
        el->setSpacing(6);

        // Small rounded square matching bar shape
        QLabel* dot = new QLabel();
        dot->setFixedSize(10, 10);
        dot->setStyleSheet(QString(
            "background-color: %1;"
            "border-radius: 2px;"
        ).arg(color));

        // Label word in textSecondary, count number in severity colour
        QLabel* text = new QLabel(QString("%1 <span style='color: %2; font-weight: bold;'>%3</span>")
            .arg(labelText, color)
            .arg(count));
        text->setTextFormat(Qt::RichText);
        {
            QFont f = text->font();
            f.setPointSize(9);
            text->setFont(f);
            text->setStyleSheet(QString("color: %1; background: transparent;")
                .arg(AppTheme::colors().textSecondary));
        }

        el->addWidget(dot);
        el->addWidget(text);
        return entry;
    };

    headerLayout->addWidget(makeLegendEntry("#e74c3c", "Errors",   totals.critical));
    headerLayout->addSpacing(16);
    headerLayout->addWidget(makeLegendEntry("#f39c12", "Warnings", totals.warnings));
    headerLayout->addSpacing(16);
    headerLayout->addWidget(makeLegendEntry("#5dade2", "Info",     totals.info));
    headerLayout->addSpacing(16);

    // SWD legend — plain text, no square, explains the background noise concept
    auto* swdLegendLabel = new QLabel(
        QString("· <span style='color: %1;'>%2</span> software background (SWD, not shown on graph)")
            .arg(AppTheme::colors().textMuted)
            .arg(totals.swd));
    swdLegendLabel->setTextFormat(Qt::RichText);
    {
        QFont f = swdLegendLabel->font();
        f.setPointSize(9);
        swdLegendLabel->setFont(f);
        swdLegendLabel->setStyleSheet(QString("color: %1; background: transparent;")
            .arg(AppTheme::colors().textMuted));
    }
    headerLayout->addWidget(swdLegendLabel);

    contentLayout->addWidget(timelineHeader);
    contentLayout->addSpacing(6);
    contentLayout->addWidget(m_timeline);
    contentLayout->addSpacing(12);

    // Wire log window toggle -> update timeline day range + refresh header labels
    connect(m_dashboard, &DashboardWidget::logWindowChanged, this, [this, titleLabel, totalLabel](int days) {
        m_selectedLogDays = days;
        // Graph and header update on next rescan — not immediately
    });

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
        m_categorySections.push_back(section);
        contentLayout->addWidget(section);
    }

    contentLayout->addStretch();

    // =========================================================================
    // Footer Note
    // =========================================================================
    
    QLabel* footerLabel = new QLabel("Driver Visualiser - Keeping your drivers in check since 2026");
    QFont footerFont = footerLabel->font();
    footerFont.setPointSize(8);
    footerFont.setItalic(true);
    footerLabel->setFont(footerFont);
    footerLabel->setAlignment(Qt::AlignCenter);
    footerLabel->setStyleSheet(QString(
        "QLabel {"
        "   color: %1;"
        "   background: transparent;"
        "   padding: 16px 0px 8px 0px;"
        "}"
    ).arg(AppTheme::colors().textMuted));
    contentLayout->addWidget(footerLabel);

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

    m_dashboard        = nullptr;
    m_timeline         = nullptr;
    m_searchBar        = nullptr;
    m_contentLayout    = nullptr;
    m_loadingWidget    = nullptr;
    m_loadingLabel     = nullptr;
    m_categorySections.clear();
}

// ============================================================================
// Filter
// ============================================================================

void MainWindow::applyFilters()
{
    if (!m_searchBar || m_categorySections.empty()) return;

    const QString searchText = m_searchBar->getSearchText().toLower();
    const FilterPopupWidget::FilterState filters = m_searchBar->getFilterState();

    // Delegate filtering to each section widget — it shows/hides individual
    // driver cards and hides itself if no cards match. No widget destruction,
    // no widget construction — just visibility changes, so filtering is instant.
    bool anyVisible = false;
    for (auto* section : m_categorySections) {
        if (section->applyFilter(searchText, filters))
            anyVisible = true;
    }

    // Show/hide the "no results" label (last item in layout before stretch)
    // We manage this separately since it's not a section widget.
    // Find it by object name set during populateDriverList.
    if (m_contentLayout) {
        QWidget* noResults = m_contentLayout->parentWidget()
            ? m_contentLayout->parentWidget()->findChild<QLabel*>("noResultsLabel")
            : nullptr;

        if (!anyVisible) {
            if (!noResults) {
                auto* lbl = new QLabel("No drivers match your search/filter criteria");
                lbl->setObjectName("noResultsLabel");
                lbl->setStyleSheet(QString(
                    "QLabel { color: %1; font-size: 14px; padding: 40px; }"
                ).arg(AppTheme::colors().textSecondary));
                lbl->setAlignment(Qt::AlignCenter);
                // Insert before the stretch (second-to-last item)
                int stretchIdx = m_contentLayout->count() - 1;
                m_contentLayout->insertWidget(stretchIdx, lbl);
            }
            if (noResults) noResults->setVisible(true);
        } else {
            if (noResults) noResults->setVisible(false);
        }
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
        reportText = generator.generateHtmlReport(m_allDrivers, m_systemInfo, m_lastScanLogDays);
    } else {
        reportText = generator.generateTextReport(m_allDrivers, m_systemInfo, m_lastScanLogDays);
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