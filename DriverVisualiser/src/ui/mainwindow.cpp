#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "PhysicalDeviceGrouper.h"
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

    auto physicalDevices = PhysicalDeviceGrouper::groupByContainerId(drivers);

    for (const auto& [containerId, device] : physicalDevices) {
        QTreeWidgetItem* deviceItem = new QTreeWidgetItem(ui->treeDrivers);

        QString deviceName = QString::fromStdWString(device.displayName);
        deviceName += QString(" (%1 driver%2)")
                          .arg(device.drivers.size())
                          .arg(device.drivers.size() == 1 ? "" : "s");

        deviceItem->setText(0, deviceName);
        deviceItem->setText(1, QString::fromStdWString(device.manufacturer));
        deviceItem->setText(2, DriverStatusFormatter::statusToString(device.worstStatus));

        QFont font = deviceItem->font(0);
        font.setBold(true);
        deviceItem->setFont(0, font);

        deviceItem->setExpanded(true);

        for (const auto& driver : device.drivers) {
            QTreeWidgetItem* driverItem = new QTreeWidgetItem(deviceItem);

            QString driverName = QString::fromStdWString(driver.name);
            if (!driver.deviceClassName.empty()) {
                driverName += QString(" [%1]").arg(QString::fromStdWString(driver.deviceClassName));
            }

            driverItem->setText(0, driverName);
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
