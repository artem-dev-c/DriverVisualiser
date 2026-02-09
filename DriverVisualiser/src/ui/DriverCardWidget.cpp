#include "DriverCardWidget.h"
#include "DriverVersionFormatter.h"
#include "DriverStatusFormatter.h"
#include "DriverImportanceEvaluator.h"
#include "DriverDateFormatter.h"
#include "FlagIndicatorWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QApplication>
#include <QPalette>
#include <QProgressBar>

DriverCardWidget::DriverCardWidget(const DriverInfo& driver, QWidget* parent)
    : QFrame(parent)
{
    setupUi(driver);
}

void DriverCardWidget::setupUi(const DriverInfo& driver)
{
    // Use native styling - just a subtle frame
    setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
    setAutoFillBackground(true);
    
    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 8, 12, 8);
    mainLayout->setSpacing(6);

    // === Top row: Name + Importance Indicator ===
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(12);

    // Device name (large, bold)
    QString deviceName;
    if (!driver.provider.empty() && driver.provider != L"Unknown") {
        deviceName = QString::fromStdWString(driver.provider);
    } else {
        deviceName = QString::fromStdWString(driver.name);
    }

    m_nameLabel = new QLabel(deviceName);
    QFont nameFont = m_nameLabel->font();
    nameFont.setPointSize(10);
    nameFont.setBold(true);
    m_nameLabel->setFont(nameFont);
    headerLayout->addWidget(m_nameLabel, 1); // Stretch to fill

    // Importance indicator (3 squares)
    DriverImportance importance = DriverImportanceEvaluator::evaluate(driver);
    QWidget* importanceWidget = createImportanceIndicator(importance);
    headerLayout->addWidget(importanceWidget);

    mainLayout->addLayout(headerLayout);

    // === Content area: Details (left) + Flag Indicator (right) ===
    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(15);

    // Left side: Two rows of details
    QVBoxLayout* detailsLayout = new QVBoxLayout();
    detailsLayout->setSpacing(4);

    // Row 1: Version, Manufacturer
    QHBoxLayout* row1Layout = new QHBoxLayout();
    row1Layout->setSpacing(20);

    QString versionText = "Version: " + DriverVersionFormatter::versionToString(driver.version);
    m_versionLabel = new QLabel(versionText);
    row1Layout->addWidget(m_versionLabel);

    QString manufacturerText = "Manufacturer: " + QString::fromStdWString(driver.manufacturer);
    m_manufacturerLabel = new QLabel(manufacturerText);
    row1Layout->addWidget(m_manufacturerLabel);

    row1Layout->addStretch();
    detailsLayout->addLayout(row1Layout);

    // Row 2: Status, Driver Date, Installed, Health
    QHBoxLayout* row2Layout = new QHBoxLayout();
    row2Layout->setSpacing(15);

    // Status (color coded)
    QString statusText = DriverStatusFormatter::statusToString(driver.status);
    m_statusLabel = new QLabel("Status: " + statusText);
    QString statusColor = getStatusColor(driver.status);
    m_statusLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(statusColor));
    row2Layout->addWidget(m_statusLabel);

    // Driver date
    QString driverDateText = DriverDateFormatter::isDriverDateValid(driver.driverDate) 
        ? DriverDateFormatter::dateToString(driver.driverDate) 
        : "Unknown";
    m_driverDateLabel = new QLabel("Driver Date: " + driverDateText);
    row2Layout->addWidget(m_driverDateLabel);

    // Install date
    QString installDateText = DriverDateFormatter::dateToString(driver.installDate);
    m_installDateLabel = new QLabel("Installed: " + installDateText);
    row2Layout->addWidget(m_installDateLabel);

    // Health bar
    QLabel* healthLabel = new QLabel("Health:");
    row2Layout->addWidget(healthLabel);
    
    QWidget* healthBar = createHealthBar(driver.healthScore);
    row2Layout->addWidget(healthBar);
    
    // Health percentage
    m_healthPercentLabel = new QLabel(QString("%1%").arg(driver.healthScore));
    m_healthPercentLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(getHealthColor(driver.healthScore)));
    row2Layout->addWidget(m_healthPercentLabel);

    row2Layout->addStretch();
    detailsLayout->addLayout(row2Layout);

    contentLayout->addLayout(detailsLayout, 1);

    // Right side: Flag indicator (spans both rows visually)
    m_flagIndicator = new FlagIndicatorWidget(driver.healthFlags);
    contentLayout->addWidget(m_flagIndicator);

    mainLayout->addLayout(contentLayout);
}

QWidget* DriverCardWidget::createHealthBar(int healthScore)
{
    m_healthBar = new QProgressBar();
    m_healthBar->setRange(0, 100);
    m_healthBar->setValue(healthScore);
    m_healthBar->setTextVisible(false);
    m_healthBar->setFixedHeight(6);
    m_healthBar->setFixedWidth(60);
    
    QString color = getHealthColor(healthScore);
    
    m_healthBar->setStyleSheet(QString(
        "QProgressBar {"
        "   border: 1px solid #ccc;"
        "   border-radius: 3px;"
        "   background-color: #e0e0e0;"
        "}"
        "QProgressBar::chunk {"
        "   background-color: %1;"
        "   border-radius: 2px;"
        "}"
    ).arg(color));

    return m_healthBar;
}

QWidget* DriverCardWidget::createImportanceIndicator(DriverImportance importance)
{
    QWidget* container = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    int filledCount = 0;
    switch (importance) {
        case DriverImportance::Critical:  filledCount = 3; break;
        case DriverImportance::Important: filledCount = 2; break;
        case DriverImportance::Optional:  filledCount = 1; break;
        default:                          filledCount = 0; break;
    }

    QString activeColor = getImportanceColor(importance);

    for (int i = 0; i < 3; ++i) {
        QLabel* square = new QLabel();
        square->setFixedSize(16, 16);
        
        if (i < filledCount) {
            square->setStyleSheet(QString(
                "background-color: %1;"
                "border-radius: 3px;"
            ).arg(activeColor));
        } else {
            square->setStyleSheet(
                "background-color: #ddd;"
                "border-radius: 3px;"
            );
        }
        
        layout->addWidget(square);
    }

    return container;
}

QString DriverCardWidget::getImportanceColor(DriverImportance importance)
{
    switch (importance) {
        case DriverImportance::Critical:  return "#e74c3c"; // Red
        case DriverImportance::Important: return "#f39c12"; // Orange
        case DriverImportance::Optional:  return "#5af14a"; // Green
        default:                          return "#95a5a6"; // Gray
    }
}

QString DriverCardWidget::getHealthColor(int healthScore)
{
    if (healthScore >= 80) {
        return "#27ae60"; // Green
    } else if (healthScore >= 50) {
        return "#f39c12"; // Orange
    } else {
        return "#e74c3c"; // Red
    }
}

QString DriverCardWidget::getStatusColor(DriverStatus status)
{
    switch (status) {
        case DriverStatus::Ok:         return "#27ae60";
        case DriverStatus::Error:      return "#e74c3c";
        case DriverStatus::NotStarted: return "#f39c12";
        case DriverStatus::Disabled:   return "#95a5a6";
        default:                       return "#666666";
    }
}