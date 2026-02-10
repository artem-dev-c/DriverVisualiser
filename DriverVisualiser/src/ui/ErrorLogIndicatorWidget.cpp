#include "ErrorLogIndicatorWidget.h"
#include "ErrorLogPopupWidget.h"
#include <QHBoxLayout>
#include <QMouseEvent>

ErrorLogIndicatorWidget::ErrorLogIndicatorWidget(const std::vector<ErrorLogEntry>& entries, QWidget* parent)
    : QFrame(parent)
    , m_entries(entries)
    , m_textLabel(nullptr)
    , m_arrowLabel(nullptr)
    , m_popup(nullptr)
    , m_hasLogs(!entries.empty())
{
    setupUi();
    updateAppearance();
}

void ErrorLogIndicatorWidget::setupUi()
{
    setFixedHeight(40);
    setMinimumWidth(180);
    setMaximumWidth(220);
    
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(8);
    
    // Main text
    m_textLabel = new QLabel();
    m_textLabel->setWordWrap(false);
    layout->addWidget(m_textLabel, 1);
    
    // Arrow indicator (only if has logs)
    m_arrowLabel = new QLabel("▼");
    m_arrowLabel->setFixedWidth(12);
    layout->addWidget(m_arrowLabel);
    
    if (m_hasLogs) {
        setCursor(Qt::PointingHandCursor);
    }
}

void ErrorLogIndicatorWidget::updateAppearance()
{
    if (!m_hasLogs) {
        // No logs - green/gray appearance
        m_textLabel->setText("No logs found");
        m_arrowLabel->setVisible(false);
        
        setStyleSheet(
            "ErrorLogIndicatorWidget {"
            "   border: 1px solid #27ae60;"
            "   border-radius: 4px;"
            "   background-color: rgba(39, 174, 96, 0.1);"
            "}"
        );
        m_textLabel->setStyleSheet("color: #27ae60;");
    } else {
        // Has logs - count by severity
        int critical = countCritical();
        int errors = countErrors();
        int warnings = countWarnings();
        
        QString text;
        if (critical > 0) {
            text = QString("%1 critical, %2 errors").arg(critical).arg(errors);
        } else if (errors > 0) {
            text = QString("%1 errors (7 days)").arg(errors);
        } else {
            text = QString("%1 warnings (7 days)").arg(warnings);
        }
        
        m_textLabel->setText(text);
        
        int totalIssues = critical + errors;
        QString outlineColor = getOutlineColor(totalIssues);
        QString bgColor = getBackgroundColor(totalIssues);
        
        setStyleSheet(QString(
            "ErrorLogIndicatorWidget {"
            "   border: 2px solid %1;"
            "   border-radius: 4px;"
            "   background-color: %2;"
            "}"
        ).arg(outlineColor, bgColor));
        
        m_textLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(outlineColor));
        m_arrowLabel->setStyleSheet(QString("color: %1;").arg(outlineColor));
        m_arrowLabel->setVisible(true);
    }
}

void ErrorLogIndicatorWidget::mousePressEvent(QMouseEvent* event)
{
    if (!m_hasLogs) {
        return;  // No popup if no logs
    }
    
    if (event->button() == Qt::LeftButton) {
        // Create popup if not exists
        if (!m_popup) {
            m_popup = new ErrorLogPopupWidget(m_entries, nullptr);
        }
        
        // Check if popup was just closed (prevent reopen on same click)
        if (m_popup->wasRecentlyClosed()) {
            return;
        }
        
        // Toggle popup
        if (m_popup->isVisible()) {
            m_popup->hide();
        } else {
            m_popup->showRelativeTo(this);
        }
    }
    
    QFrame::mousePressEvent(event);
}

// ============================================================================
// Severity Counting
// ============================================================================

int ErrorLogIndicatorWidget::countCritical() const
{
    int count = 0;
    for (const auto& entry : m_entries) {
        if (entry.level == L"Critical") {
            count++;
        }
    }
    return count;
}

int ErrorLogIndicatorWidget::countErrors() const
{
    int count = 0;
    for (const auto& entry : m_entries) {
        if (entry.level == L"Error" || entry.level == L"Critical") {
            count++;
        }
    }
    return count;
}

int ErrorLogIndicatorWidget::countWarnings() const
{
    int count = 0;
    for (const auto& entry : m_entries) {
        if (entry.level == L"Warning") {
            count++;
        }
    }
    return count;
}

// ============================================================================
// Color Helpers
// ============================================================================

QString ErrorLogIndicatorWidget::getOutlineColor(int errorCount)
{
    if (errorCount >= 5) {
        return "#e74c3c";  // Red - many errors
    } else if (errorCount >= 2) {
        return "#f39c12";  // Orange - some errors
    } else if (errorCount >= 1) {
        return "#f1c40f";  // Yellow - few errors
    } else {
        return "#3498db";  // Blue - warnings only
    }
}

QString ErrorLogIndicatorWidget::getBackgroundColor(int errorCount)
{
    if (errorCount >= 5) {
        return "rgba(231, 76, 60, 0.15)";   // Red
    } else if (errorCount >= 2) {
        return "rgba(243, 156, 18, 0.15)";  // Orange
    } else if (errorCount >= 1) {
        return "rgba(241, 196, 15, 0.15)";  // Yellow
    } else {
        return "rgba(52, 152, 219, 0.15)";  // Blue
    }
}