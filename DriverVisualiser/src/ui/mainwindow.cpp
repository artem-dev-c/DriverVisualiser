#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "CategoryGrouper.h"              // Changed
#include "DriverScanner.h"
#include "DriverStatusFormatter.h"
#include "DriverVersionFormatter.h"
#include "DriverInstallDateFormatter.h"
#include "DriverImportanceEvaluator.h"
#include "DriverImportanceFormatter.h"
#include <QTreeWidgetItem>
#include <QHeaderView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    populateDriverTree();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::populateDriverTree()
{
    ui->treeDrivers->clear();

    ui->treeDrivers->setColumnCount(7);
    ui->treeDrivers->setHeaderLabels({
        "Name",
        "Manufacturer",
        "Status",
        "Version",
        "Install date",
        "Importance",
        "Instance ID"
    });

    DriverScanner scanner;
    auto drivers = scanner.fetchDrivers();

    // Group by category (Device Manager style)
    auto categories = CategoryGrouper::groupByCategory(drivers);

    for (const auto& [className, category] : categories) {
        // Category header (bold)
        QTreeWidgetItem* categoryItem = new QTreeWidgetItem(ui->treeDrivers);
        
        QString categoryName = QString::fromStdWString(category.displayName);
        categoryName += QString(" (%1)").arg(category.drivers.size());
        
        categoryItem->setText(0, categoryName);
        
        QFont font = categoryItem->font(0);
        font.setBold(true);
        categoryItem->setFont(0, font);
        
        categoryItem->setExpanded(false);

        // Individual drivers under category
        for (const auto& driver : category.drivers) {
            QTreeWidgetItem* driverItem = new QTreeWidgetItem(categoryItem);

            // Smart name selection based on device type
            QString deviceName;
            
            if (!driver.provider.empty() && driver.provider != L"Unknown") {
                // Provider has useful info (disks, some devices)
                deviceName = QString::fromStdWString(driver.provider);
            } else {
                // Fall back to name (GPUs, most devices)
                deviceName = QString::fromStdWString(driver.name);
            }
            
            driverItem->setText(0, deviceName);
            driverItem->setText(1, QString::fromStdWString(driver.manufacturer));
            driverItem->setText(2, DriverStatusFormatter::statusToString(driver.status));
            driverItem->setText(3, DriverVersionFormatter::versionToString(driver.version));
            driverItem->setText(4, DriverInstallDateFormatter::dateToString(driver.installDate));
            driverItem->setText(5, DriverImportanceFormatter::importanceToString(
                                    DriverImportanceEvaluator::evaluate(driver)));
            driverItem->setText(6, QString::fromStdWString(driver.instanceId));
        }
    }

    for (int i = 0; i < ui->treeDrivers->columnCount(); ++i) {
        ui->treeDrivers->resizeColumnToContents(i);
    }
}