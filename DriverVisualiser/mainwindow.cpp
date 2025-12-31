#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "DriverStatusFormatter.h"
#include "DriverVersionFormatter.h"
#include "DriverInstallDateFormatter.h"
#include "DriverImportanceFormatter.h"
#include "DriverImportanceEvaluator.h"
#include "DeviceGrouper.h"
#include "DeviceInfo.h"
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

    auto devices = DeviceGrouper::groupByDevice(drivers);

    for (const auto& [id, device] : devices) {

        QTreeWidgetItem* deviceItem = new QTreeWidgetItem(ui->treeDrivers);
        deviceItem->setText(0, QString::fromStdWString(device.name));
        deviceItem->setText(1, QString::fromStdWString(device.manufacturer));
        deviceItem->setText(6, QString::fromStdWString(device.instanceId));
        deviceItem->setFirstColumnSpanned(true);
        deviceItem->setExpanded(false);

        for (const auto& driver : device.drivers) {
            QTreeWidgetItem* driverItem = new QTreeWidgetItem(deviceItem);

            driverItem->setText(0, QString::fromStdWString(driver.name));
            driverItem->setText(1, QString::fromStdWString(driver.manufacturer));
            driverItem->setText(2, DriverStatusFormatter::statusToString(driver.status));
            driverItem->setText(3, DriverVersionFormatter::versionToString(driver.version));
            driverItem->setText(4, DriverInstallDateFormatter::dateToString(driver.installDate));
            driverItem->setText(5, DriverImportanceFormatter::importanceToString(
                                    DriverImportanceEvaluator::evaluate(driver)));
            driverItem->setText(6, QString::fromStdWString(driver.instanceId));
        }
    }

    ui->treeDrivers->expandAll();

}
