#include "DriverCardWidget.h"
#include "DriverVersionFormatter.h"
#include "DriverStatusFormatter.h"
#include "DriverImportanceEvaluator.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
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

    // === Details row: Version, Manufacturer, Status ===
    QHBoxLayout* detailsLayout = new QHBoxLayout();
    detailsLayout->setSpacing(20);

    // Version
    QString versionText = "Version: " + DriverVersionFormatter::versionToString(driver.version);
    m_versionLabel = new QLabel(versionText);
    detailsLayout->addWidget(m_versionLabel);

    // Manufacturer
    QString manufacturerText = "Manufacturer: " + QString::fromStdWString(driver.manufacturer);
    m_manufacturerLabel = new QLabel(manufacturerText);
    detailsLayout->addWidget(m_manufacturerLabel);

    // Status (keep color coding for this one - it's meaningful)
    QString statusText = DriverStatusFormatter::statusToString(driver.status);
    m_statusLabel = new QLabel("Status: " + statusText);
    QString statusColor = getStatusColor(driver.status);
    m_statusLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(statusColor));
    detailsLayout->addWidget(m_statusLabel);

    // Health bar (compact, inline)
    int healthScore = driver.healthScore;
    
    QLabel* healthLabel = new QLabel("Health:");
    detailsLayout->addWidget(healthLabel);
    
    QWidget* healthBar = createHealthBar(healthScore);
    detailsLayout->addWidget(healthBar);

    detailsLayout->addStretch();

    mainLayout->addLayout(detailsLayout);
}

QWidget* DriverCardWidget::createHealthBar(int healthScore)
{
    m_healthBar = new QProgressBar();
    m_healthBar->setRange(0, 100);
    m_healthBar->setValue(healthScore);
    m_healthBar->setTextVisible(false);
    m_healthBar->setFixedHeight(10);
    m_healthBar->setFixedWidth(100);
    
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
        case DriverImportance::Optional:  return "#52e335"; // Blue
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