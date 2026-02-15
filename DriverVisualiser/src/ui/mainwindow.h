#pragma once

#include <QMainWindow>
#include <QVBoxLayout>
#include <vector>
#include "DriverInfo.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class CategorySectionWidget;
class SearchFilterBar;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    
    void populateDriverList();
    void clearDriverList();
    void applyFilters();
    
    // Search/filter state
    SearchFilterBar* m_searchBar = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;
    std::vector<DriverInfo> m_allDrivers;
};