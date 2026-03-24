#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <vector>
#include "ErrorLogEntry.h"
#include <QDateTime>

/// ============================================================================
/// EventLogDiagnosticWidget
/// ============================================================================
/// TEMPORARY DIAGNOSTIC TOOL - Shows ALL collected events in raw form
/// for debugging/verification before they're filtered and matched to drivers.
///
/// Usage:
///   1. After querySystemLog(), before populateErrorLogs()
///   2. Shows: EventID, Provider, Level, Category, DeviceInstanceId, Message
///   3. Color-coded by category (DeviceMapped, SystemWide, SWD)
///
/// This is NOT for production - it's a temporary debug tool.
/// ============================================================================
class EventLogDiagnosticWidget : public QWidget
{
    Q_OBJECT

public:
    explicit EventLogDiagnosticWidget(const std::vector<ErrorLogEntry>& events,
                                      QWidget* parent = nullptr)
        : QWidget(parent)
        , m_events(events)
    {
        setupUi();
        populateEvents();
    }

private:
    void setupUi()
    {
        setWindowTitle("Event Log Diagnostic - RAW COLLECTED EVENTS");
        resize(1200, 800);
        
        QVBoxLayout* layout = new QVBoxLayout(this);
        
        // Header
        QLabel* header = new QLabel(QString("Total Events Collected: %1").arg(m_events.size()));
        QFont headerFont = header->font();
        headerFont.setPointSize(12);
        headerFont.setBold(true);
        header->setFont(headerFont);
        layout->addWidget(header);
        
        // Stats
        int deviceMapped = 0, systemWide = 0, swd = 0;
        int critical = 0, errors = 0, warnings = 0, info = 0;
        
        for (const auto& e : m_events) {
            if (e.category == ErrorLogEntry::Category::DeviceMapped) deviceMapped++;
            else if (e.category == ErrorLogEntry::Category::SystemWide) systemWide++;
            else if (e.category == ErrorLogEntry::Category::SwdBackground) swd++;
            
            if (e.level == L"Critical") critical++;
            else if (e.level == L"Error") errors++;
            else if (e.level == L"Warning") warnings++;
            else if (e.level == L"Information") info++;
        }
        
        QString stats = QString(
            "Categories: DeviceMapped=%1, SystemWide=%2, SWD=%3  |  "
            "Severity: Critical=%4, Error=%5, Warning=%6, Info=%7"
        ).arg(deviceMapped).arg(systemWide).arg(swd)
         .arg(critical).arg(errors).arg(warnings).arg(info);
        
        QLabel* statsLabel = new QLabel(stats);
        layout->addWidget(statsLabel);
        
        // Text display
        m_textEdit = new QTextEdit();
        m_textEdit->setReadOnly(true);
        m_textEdit->setFont(QFont("Consolas", 9));
        layout->addWidget(m_textEdit);
        
        // Buttons
        QHBoxLayout* btnLayout = new QHBoxLayout();
        
        QPushButton* btnClose = new QPushButton("Close");
        connect(btnClose, &QPushButton::clicked, this, &QWidget::close);
        btnLayout->addStretch();
        btnLayout->addWidget(btnClose);
        
        layout->addLayout(btnLayout);
    }
    
    void populateEvents()
    {
        QString html;
        html += "<style>";
        html += "  .devicemapped { color: #2ecc71; }";  // Green
        html += "  .systemwide { color: #e74c3c; }";    // Red
        html += "  .swd { color: #95a5a6; }";           // Gray
        html += "  .critical { background: #c0392b; color: white; }";
        html += "  .error { background: #e74c3c; color: white; }";
        html += "  .warning { background: #f39c12; color: white; }";
        html += "  .info { background: #3498db; color: white; }";
        html += "</style>";
        
        html += "<pre style='font-family: Consolas, monospace; font-size: 9pt;'>";
        html += "┌──────────────────┬─────┬──────────────────────────────────┬──────────┬──────────────┬──────────────────────────────────────────────┬────────────────────────────────────────────┐\n";
        html += "│ Timestamp        │ ID  │ Provider                         │ Level    │ Category     │ DeviceInstanceId                             │ Message                                     │\n";
        html += "├──────────────────┼─────┼──────────────────────────────────┼──────────┼──────────────┼──────────────────────────────────────────────┼────────────────────────────────────────────┤\n";
        
        for (const auto& e : m_events) {
            // Format timestamp
            auto timeT = std::chrono::system_clock::to_time_t(e.timestamp);
            QDateTime dt = QDateTime::fromSecsSinceEpoch(timeT);
            QString timestamp = dt.toString("MM/dd hh:mm:ss");
            
            QString provider = QString::fromStdWString(e.provider);
            if (provider.length() > 32) provider = provider.left(29) + "...";
            
            QString level = QString::fromStdWString(e.level);
            QString levelClass;
            if (level == "Critical") levelClass = "critical";
            else if (level == "Error") levelClass = "error";
            else if (level == "Warning") levelClass = "warning";
            else levelClass = "info";
            
            QString category;
            QString categoryClass;
            if (e.category == ErrorLogEntry::Category::DeviceMapped) {
                category = "DeviceMapped";
                categoryClass = "devicemapped";
            } else if (e.category == ErrorLogEntry::Category::SystemWide) {
                category = "SystemWide";
                categoryClass = "systemwide";
            } else {
                category = "SWD";
                categoryClass = "swd";
            }
            
            QString deviceId = e.deviceInstanceId.has_value() 
                ? QString::fromStdWString(e.deviceInstanceId.value())
                : "(none)";
            if (deviceId.length() > 44) deviceId = deviceId.left(41) + "...";
            
            QString message = QString::fromStdWString(e.message);
            if (message.length() > 42) message = message.left(39) + "...";
            
            html += QString("│ %1 │ %2 │ %3 │ <span class='%4'>%5</span> │ <span class='%6'>%7</span> │ %8 │ %9 │\n")
                .arg(timestamp, -16)
                .arg(e.eventId, 3)
                .arg(provider, -32)
                .arg(levelClass)
                .arg(level, -8)
                .arg(categoryClass)
                .arg(category, -12)
                .arg(deviceId, -44)
                .arg(message, -42);
        }
        
        html += "└──────────────────┴─────┴──────────────────────────────────┴──────────┴──────────────┴──────────────────────────────────────────────┴────────────────────────────────────────────┘\n";
        html += "</pre>";
        
        m_textEdit->setHtml(html);
    }
    
    std::vector<ErrorLogEntry> m_events;
    QTextEdit* m_textEdit = nullptr;
};