#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "CategoryGrouper.h"
#include "CategoryProcessor.h"
#include "DriverScanner.h"
#include "DriverImportanceEvaluator.h"
#include "ErrorLogReader.h"
#include "HealthScoreEvaluator.h"
#include "CategorySectionWidget.h"
#include <QScreen>
#include <QGuiApplication>

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
}