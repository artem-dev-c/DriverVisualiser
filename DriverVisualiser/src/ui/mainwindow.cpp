#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "CategoryGrouper.h"
#include "DriverScanner.h"
#include "CategorySectionWidget.h"
#include "DriverCardWidget.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
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

    // Get the layout from the scroll area contents
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(ui->scrollAreaContents->layout());
    if (!layout) {
        layout = new QVBoxLayout(ui->scrollAreaContents);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
    }

    // Scan for drivers
    DriverScanner scanner;
    auto drivers = scanner.fetchDrivers();

    // Group by category
    auto categories = CategoryGrouper::groupByCategory(drivers);

    // Create sections for each category
    for (const auto& [className, category] : categories) {
        QString categoryName = QString::fromStdWString(category.displayName);
        
        // Create collapsible section
        CategorySectionWidget* section = new CategorySectionWidget(
            categoryName, 
            static_cast<int>(category.drivers.size())
        );

        // Add driver cards to the section
        for (const auto& driver : category.drivers) {
            DriverCardWidget* card = new DriverCardWidget(driver);
            section->addDriverCard(card);
        }

        layout->addWidget(section);
    }

    // Add stretch at the end to push everything to the top
    layout->addStretch();
}