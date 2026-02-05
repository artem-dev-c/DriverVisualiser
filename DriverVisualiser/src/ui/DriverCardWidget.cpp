#include "DriverCardWidget.h"
#include "DriverVersionFormatter.h"
#include "DriverStatusFormatter.h"
#include "DriverImportanceEvaluator.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QApplication>
#include <QPalette>

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

    // === Details row: Version, Manufacturer, Status, Fault Score ===
    QHBoxLayout* detailsLayout = new QHBoxLayout();
    detailsLayout->setSpacing(20);

    // Use system palette for secondary text color
    QPalette pal = QApplication::palette();
    
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

    // Fault Score
    QString faultText = QString("Fault Score: %1").arg(driver.faultScore);
    m_faultScoreLabel = new QLabel(faultText);
    detailsLayout->addWidget(m_faultScoreLabel);

    detailsLayout->addStretch(); // Push everything to the left

    mainLayout->addLayout(detailsLayout);
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
        case DriverImportance::Virtual:   filledCount = 0; break;  // NEW
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
            // Gray square for empty/virtual
            square->setStyleSheet(
                "background-color: #ccc;"  // Light gray
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
        case DriverImportance::Optional:  return "#54e536"; // Green
        case DriverImportance::Virtual:   return "#95a5a6"; // Gray - NEW
        default:                          return "#95a5a6"; // Gray
    }
}

QString DriverCardWidget::getStatusColor(DriverStatus status)
{
    switch (status) {
        case DriverStatus::Ok:         return "#27ae60"; // Green
        case DriverStatus::Error:      return "#e74c3c"; // Red
        case DriverStatus::NotStarted: return "#f39c12"; // Orange
        case DriverStatus::Disabled:   return "#95a5a6"; // Gray
        default:                       return "#666666"; // Dark gray
    }
}