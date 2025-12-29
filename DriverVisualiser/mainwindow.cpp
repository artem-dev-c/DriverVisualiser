#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "DriverScanner.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    populateDriverTable();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::populateDriverTable()
{
    DriverScanner scanner;
    std::vector<DriverInfo> drivers = scanner.fetchDrivers();

    ui->tableDrivers->setRowCount(drivers.size());

    for (int row = 0; row < drivers.size(); row++) {
        const auto& info = drivers[row];

        // Device Name | Manufacturer | Device Class | Provider
        ui->tableDrivers->setItem(row, 0, new QTableWidgetItem(QString::fromStdWString(info.name)));
        ui->tableDrivers->setItem(row, 1, new QTableWidgetItem(QString::fromStdWString(info.manufacturer)));
        ui->tableDrivers->setItem(row, 2, new QTableWidgetItem(QString::fromStdWString(info.deviceClass)));
        ui->tableDrivers->setItem(row, 3, new QTableWidgetItem(QString::fromStdWString(info.version)));

        ui->tableDrivers->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); // Auto-size columns
        ui->tableDrivers->setSortingEnabled(true); // Allow user to click headers to sort
    }
}
