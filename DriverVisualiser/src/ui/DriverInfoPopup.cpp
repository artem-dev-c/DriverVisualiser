#include "DriverInfoPopup.h"
#include "DriverVersionFormatter.h"
#include "DriverStatusFormatter.h"
#include "DriverDateFormatter.h"
#include "SystemInfoCollector.h"

#include <QApplication>
#include <QScreen>
#include <QClipboard>
#include <QLabel>
#include <QHBoxLayout>
#include <QDateTime>
#include <QScrollArea>
#include <QTimer>
#include <QKeyEvent>
#include <QFocusEvent>

DriverInfoPopup::DriverInfoPopup(const DriverInfo& driver, QWidget* parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
    , m_driver(driver)
    , m_systemInfo(SystemInfoCollector::collect())
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    
    setupUi(driver);
}

void DriverInfoPopup::setupUi(const DriverInfo& driver)
{
    setFixedWidth(520);
    setMaximumHeight(480);
    
    // Main popup styling (same as FlagPopupWidget)
    setStyleSheet(
        "DriverInfoPopup {"
        "   background-color: #2b2b2b;"
        "   border: 1px solid #555555;"
        "   border-radius: 6px;"
        "}"
    );
    
    // Main layout with scroll area
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(1, 1, 1, 1);  // Small margin for border
    mainLayout->setSpacing(0);
    
    // Scroll area for content
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(
        "QScrollArea {"
        "   background-color: transparent;"
        "   border: none;"
        "   border-radius: 5px;"
        "}"
        "QScrollArea > QWidget > QWidget {"
        "   background-color: transparent;"
        "}"
    );
    
    QWidget* contentWidget = new QWidget();
    contentWidget->setStyleSheet("background-color: transparent;");
    QVBoxLayout* layout = new QVBoxLayout(contentWidget);
    layout->setContentsMargins(15, 13, 15, 13);
    layout->setSpacing(6);
    
    // === Device Information Section ===
    layout->addWidget(createSectionHeader("Device Information"));
    
    QString deviceName = !driver.name.empty() 
        ? QString::fromStdWString(driver.name) 
        : "Unknown";
    layout->addWidget(createFieldRow("Name", deviceName));
    
    QString providerName = !driver.provider.empty() && driver.provider != L"Unknown"
        ? QString::fromStdWString(driver.provider)
        : "Unknown";
    layout->addWidget(createFieldRow("Provider", providerName));
    
    QString manufacturer = !driver.manufacturer.empty() 
        ? QString::fromStdWString(driver.manufacturer) 
        : "Unknown";
    layout->addWidget(createFieldRow("Manufacturer", manufacturer));
    
    QString deviceClass = !driver.deviceClassName.empty() 
        ? QString::fromStdWString(driver.deviceClassName) 
        : "Unknown";
    layout->addWidget(createFieldRow("Device Class", deviceClass));
    
    layout->addSpacing(6);
    
    // === Hardware Identification Section ===
    layout->addWidget(createSectionHeader("Hardware Identification"));
    
    QString hardwareId = !driver.hardwareId.empty() 
        ? QString::fromStdWString(driver.hardwareId) 
        : "Not available";
    layout->addWidget(createCopyableFieldRow("Hardware ID", hardwareId));
    
    QString instanceId = !driver.instanceId.empty() 
        ? QString::fromStdWString(driver.instanceId) 
        : "Not available";
    layout->addWidget(createCopyableFieldRow("Instance ID", instanceId));
    
    QString parentId = !driver.parentInstanceId.empty() 
        ? QString::fromStdWString(driver.parentInstanceId) 
        : "Not available";
    layout->addWidget(createCopyableFieldRow("Parent Device", parentId));
    
    // Parent status (if parent exists)
    if (!driver.parentInstanceId.empty()) {
        QString parentStatus = DriverStatusFormatter::statusToString(driver.parentStatus);
        layout->addWidget(createFieldRow("Parent Status", parentStatus));
    }
    
    layout->addSpacing(8);
    
    // === Driver Details Section ===
    layout->addWidget(createSectionHeader("Driver Details"));
    
    QString version = DriverVersionFormatter::versionToString(driver.version);
    layout->addWidget(createFieldRow("Version", version));
    
    QString driverDate = DriverDateFormatter::isDriverDateValid(driver.driverDate)
        ? DriverDateFormatter::dateToString(driver.driverDate)
        : "Not available";
    layout->addWidget(createFieldRow("Driver Date", driverDate));
    
    QString installDate = driver.installDate.has_value()
        ? DriverDateFormatter::dateToString(driver.installDate)
        : "Not available";
    layout->addWidget(createFieldRow("Install Date", installDate));
    
    // INF Path - prefer full path from driverFiles, fall back to property
    QString infPath = "Not available";
    if (!driver.driverFiles.empty()) {
        // First file is the full INF path
        QString firstFile = QString::fromStdWString(driver.driverFiles[0]);
        if (firstFile.endsWith(".inf", Qt::CaseInsensitive)) {
            infPath = firstFile;
        }
    }
    // Fallback to property if driverFiles didn't have it
    if (infPath == "Not available" && !driver.driverInfPath.empty()) {
        infPath = QString::fromStdWString(driver.driverInfPath);
    }
    layout->addWidget(createCopyableFieldRow("INF Path", infPath));
    
    layout->addSpacing(8);
    
    // === Status Information Section ===
    layout->addWidget(createSectionHeader("Status Information"));
    
    QString status = DriverStatusFormatter::statusToString(driver.status);
    layout->addWidget(createFieldRow("Status", status));
    
    QString problemCode = QString("0x%1").arg(driver.problemCode, 2, 16, QChar('0'));
    if (driver.problemCode == 0) {
        problemCode += " (No problems)";
    }
    layout->addWidget(createFieldRow("Problem Code", problemCode));
    
    QString rawStatus = QString("0x%1").arg(driver.rawStatus, 8, 16, QChar('0'));
    layout->addWidget(createFieldRow("Raw Status", rawStatus));
    
    layout->addSpacing(6);
    
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);
    
    // === Bottom button bar ===
    QWidget* buttonBar = new QWidget();
    buttonBar->setStyleSheet(
        "QWidget {"
        "   background-color: #2b2b2b;"
        "   border-bottom-left-radius: 5px;"
        "   border-bottom-right-radius: 5px;"
        "}"
    );
    QHBoxLayout* buttonLayout = new QHBoxLayout(buttonBar);
    buttonLayout->setContentsMargins(12, 8, 12, 11);
    
    QPushButton* copyAllButton = new QPushButton("Copy All");
    copyAllButton->setFixedSize(100, 32);
    copyAllButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #3498db;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 4px;"
        "   font-weight: bold;"
        "   font-size: 11px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #2980b9;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #1c5a85;"
        "}"
    );
    connect(copyAllButton, &QPushButton::clicked, [this, copyAllButton]() {
        QString fullReport = generateFullReport(m_driver, m_systemInfo);
        copyToClipboard(fullReport, copyAllButton);
    });
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(copyAllButton);
    
    mainLayout->addWidget(buttonBar);
}

QLabel* DriverInfoPopup::createSectionHeader(const QString& title)
{
    QLabel* header = new QLabel(title);
    header->setStyleSheet(
        "color: #5dade2;"
        "font-weight: bold;"
        "font-size: 12px;"
        "padding-top: 6px;"
        "padding-bottom: 4px;"
    );
    return header;
}

QWidget* DriverInfoPopup::createFieldRow(const QString& label, const QString& value)
{
    QWidget* row = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(row);
    layout->setContentsMargins(10, 5, 10, 5);
    layout->setSpacing(14);
    
    QLabel* labelWidget = new QLabel(label + ":");
    labelWidget->setStyleSheet("color: #999; font-size: 11px;");
    labelWidget->setFixedWidth(125);
    layout->addWidget(labelWidget);
    
    QLabel* valueWidget = new QLabel(value);
    valueWidget->setStyleSheet("color: #eee; font-size: 11px;");
    valueWidget->setWordWrap(true);
    layout->addWidget(valueWidget, 1);
    
    return row;
}

QWidget* DriverInfoPopup::createCopyableFieldRow(const QString& label, const QString& value)
{
    QWidget* row = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(row);
    mainLayout->setContentsMargins(10, 5, 10, 5);
    mainLayout->setSpacing(5);
    
    // Label
    QLabel* labelWidget = new QLabel(label + ":");
    labelWidget->setStyleSheet("color: #999; font-size: 11px;");
    mainLayout->addWidget(labelWidget);
    
    // Container with value + copy button
    QWidget* container = new QWidget();
    container->setStyleSheet(
        "QWidget {"
        "   background-color: #1e1e1e;"
        "   border: 1px solid #3a3a3a;"
        "   border-radius: 5px;"
        "}"
    );
    
    QHBoxLayout* containerLayout = new QHBoxLayout(container);
    containerLayout->setContentsMargins(10, 7, 10, 7);
    containerLayout->setSpacing(10);
    
    // Value text (with elision for very long strings)
    QLabel* valueWidget = new QLabel(value);
    valueWidget->setStyleSheet(
        "color: #ddd;"
        "font-size: 10px;"
        "font-family: 'Consolas', 'Courier New', monospace;"
        "background: transparent;"
        "border: none;"
    );
    valueWidget->setWordWrap(false);
    valueWidget->setTextInteractionFlags(Qt::TextSelectableByMouse);
    containerLayout->addWidget(valueWidget, 1);
    
    // Copy button
    QPushButton* copyButton = new QPushButton("⎘");
    copyButton->setFixedSize(24, 24);
    copyButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #3498db;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 3px;"
        "   font-size: 14px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: #2980b9;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #1c5a85;"
        "}"
    );
    copyButton->setCursor(Qt::PointingHandCursor);
    copyButton->setToolTip("Copy to clipboard");
    
    connect(copyButton, &QPushButton::clicked, [this, value, copyButton]() {
        copyToClipboard(value, copyButton);
    });
    
    containerLayout->addWidget(copyButton);
    
    mainLayout->addWidget(container);
    
    return row;
}

void DriverInfoPopup::copyToClipboard(const QString& text, QPushButton* button)
{
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(text);
    
    // Visual feedback
    QString originalText = button->text();
    button->setText("✓");
    button->setStyleSheet(
        "QPushButton {"
        "   background-color: #27ae60;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 3px;"
        "   font-size: 14px;"
        "   font-weight: bold;"
        "}"
    );
    
    // Reset after 1 second
    QTimer::singleShot(1000, [button, originalText]() {
        button->setText(originalText);
        button->setStyleSheet(
            "QPushButton {"
            "   background-color: #3498db;"
            "   color: white;"
            "   border: none;"
            "   border-radius: 3px;"
            "   font-size: 14px;"
            "   font-weight: bold;"
            "}"
            "QPushButton:hover {"
            "   background-color: #2980b9;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #1c5a85;"
            "}"
        );
    });
}

QString DriverInfoPopup::generateFullReport(const DriverInfo& driver, const SystemInfo& sysInfo)
{
    QString report;
    
    // System Information Header
    report += "SYSTEM INFORMATION\n";
    report += QString("-").repeated(60) + "\n";
    
    // Build full OS version string
    QString osVersion = QString::fromStdWString(sysInfo.osName);
    if (!sysInfo.displayVersion.empty() && sysInfo.displayVersion != L"Unknown") {
        osVersion += " " + QString::fromStdWString(sysInfo.displayVersion);
    }
    osVersion += QString(" (Build %1.%2)")
        .arg(QString::fromStdWString(sysInfo.buildNumber))
        .arg(QString::fromStdWString(sysInfo.ubr));
    
    report += QString("OS:           %1\n").arg(osVersion);
    report += QString("Architecture: %1\n").arg(QString::fromStdWString(sysInfo.architecture));
    report += QString("Generated:    %1\n\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    
    // Device Report Header
    QString deviceName = !driver.name.empty() 
        ? QString::fromStdWString(driver.name) 
        : "Unknown Device";
    report += deviceName + " - Driver Report\n";
    report += QString("=").repeated(60) + "\n\n";
    
    // Device Information
    report += "DEVICE INFORMATION\n";
    report += QString("-").repeated(60) + "\n";
    report += QString("Name:         %1\n").arg(QString::fromStdWString(driver.name));
    report += QString("Provider:     %1\n").arg(
        !driver.provider.empty() && driver.provider != L"Unknown"
        ? QString::fromStdWString(driver.provider)
        : "Unknown"
    );
    report += QString("Manufacturer: %1\n").arg(QString::fromStdWString(driver.manufacturer));
    report += QString("Device Class: %1\n\n").arg(QString::fromStdWString(driver.deviceClassName));
    
    // Hardware Identification
    report += "HARDWARE IDENTIFICATION\n";
    report += QString("-").repeated(60) + "\n";
    report += QString("Hardware ID:  %1\n").arg(QString::fromStdWString(driver.hardwareId));
    report += QString("Instance ID:  %1\n").arg(QString::fromStdWString(driver.instanceId));
    report += QString("Parent:       %1\n").arg(
        !driver.parentInstanceId.empty()
        ? QString::fromStdWString(driver.parentInstanceId)
        : "Not available"
    );
    if (!driver.parentInstanceId.empty()) {
        report += QString("Parent Status: %1\n").arg(DriverStatusFormatter::statusToString(driver.parentStatus));
    }
    report += QString("Location:     %1\n\n").arg(
        !driver.locationPath.empty()
        ? QString::fromStdWString(driver.locationPath)
        : "Not available"
    );
    
    // Driver Details
    report += "DRIVER DETAILS\n";
    report += QString("-").repeated(60) + "\n";
    report += QString("Version:      %1\n").arg(DriverVersionFormatter::versionToString(driver.version));
    report += QString("Driver Date:  %1\n").arg(
        DriverDateFormatter::isDriverDateValid(driver.driverDate)
        ? DriverDateFormatter::dateToString(driver.driverDate)
        : "Not available"
    );
    report += QString("Install Date: %1\n").arg(
        driver.installDate.has_value()
        ? DriverDateFormatter::dateToString(driver.installDate)
        : "Not available"
    );
    
    // INF Path - use full path from driverFiles if available
    QString infPathReport = QString::fromStdWString(driver.driverInfPath);
    if (!driver.driverFiles.empty()) {
        QString firstFile = QString::fromStdWString(driver.driverFiles[0]);
        if (firstFile.endsWith(".inf", Qt::CaseInsensitive)) {
            infPathReport = firstFile;
        }
    }
    report += QString("INF Path:     %1\n\n").arg(infPathReport);
    
    // Status Information
    report += "STATUS INFORMATION\n";
    report += QString("-").repeated(60) + "\n";
    report += QString("Status:       %1\n").arg(DriverStatusFormatter::statusToString(driver.status));
    report += QString("Problem Code: 0x%1\n").arg(driver.problemCode, 2, 16, QChar('0'));
    report += QString("Raw Status:   0x%1\n\n").arg(driver.rawStatus, 8, 16, QChar('0'));
    
    // Footer
    report += QString("=").repeated(60) + "\n";
    report += QString("Generated: %1\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    
    return report;
}

void DriverInfoPopup::showRelativeTo(QWidget* anchor)
{
    // Position below and aligned to the right edge of the anchor
    QPoint pos = anchor->mapToGlobal(QPoint(anchor->width() - width(), anchor->height() + 6));
    
    // Adjust if would go off screen
    QScreen* screen = QApplication::screenAt(pos);
    if (screen) {
        QRect screenRect = screen->availableGeometry();
        
        // Adjust horizontal position if off-screen left
        if (pos.x() < screenRect.left()) {
            pos.setX(screenRect.left() + 10);
        }
        
        // Adjust horizontal position if off-screen right
        if (pos.x() + width() > screenRect.right()) {
            pos.setX(screenRect.right() - width() - 10);
        }
        
        // Adjust vertical position (show above if not enough space below)
        if (pos.y() + height() > screenRect.bottom()) {
            pos.setY(anchor->mapToGlobal(QPoint(0, 0)).y() - height() - 6);
        }
    }
    
    move(pos);
    show();
}

void DriverInfoPopup::focusOutEvent(QFocusEvent* event)
{
    // Qt::Popup flag handles closing on click-outside automatically
    // We don't need to do anything here
    QFrame::focusOutEvent(event);
}

void DriverInfoPopup::hideEvent(QHideEvent* event)
{
    Q_UNUSED(event);
    m_closeTime = QDateTime::currentMSecsSinceEpoch();
    QFrame::hideEvent(event);
}

bool DriverInfoPopup::wasRecentlyClosed() const
{
    // Return true if closed within last 150ms
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    return (now - m_closeTime) < 150;
}

bool DriverInfoPopup::event(QEvent* event)
{
    // Hide on escape key
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            hide();
            return true;
        }
    }
    return QFrame::event(event);
}