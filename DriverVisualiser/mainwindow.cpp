#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "DriverGrouper.h"
#include "DriverStatusFormatter.h"
#include "DriverVersionFormatter.h"
#include "DriverInstallDateFormatter.h"
#include "DriverImportanceEvaluator.h"
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
        "Driver",
        "Manufacturer",
        "Status",
        "Version",
        "Install date",
        "Importance",
        "Instance ID"
    });


    DriverScanner scanner;
    DriverGrouper grouper;

    auto groupedDrivers = grouper.groupByClass(scanner.fetchDrivers());

    for (const auto& [deviceClass, driverList] : groupedDrivers) {

        QTreeWidgetItem* classItem = new QTreeWidgetItem(ui->treeDrivers);
        classItem->setText(0, QString::fromStdWString(deviceClass));
        classItem->setFirstColumnSpanned(true);
        classItem->setExpanded(false);

        for (const auto& driver : driverList) {
            QTreeWidgetItem* driverItem = new QTreeWidgetItem(classItem);

            DriverImportance level = DriverImportanceEvaluator::evaluate(driver);

            driverItem->setText(0, QString::fromStdWString(driver.name));
            driverItem->setText(1, QString::fromStdWString(driver.manufacturer));
            driverItem->setText(2, DriverStatusFormatter::statusToString(driver.status));
            driverItem->setText(3, DriverVersionFormatter::versionToString(driver.version));
            driverItem->setText(4, DriverInstallDateFormatter::dateToString(driver.installDate));
            driverItem->setText(5, DriverImportanceEvaluator::importanceToString(level));
            driverItem->setText(6, QString::fromStdWString(driver.instanceId));
        }

    }

    ui->treeDrivers->expandAll();
}

