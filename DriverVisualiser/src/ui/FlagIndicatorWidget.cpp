#include "FlagIndicatorWidget.h"
#include "FlagPopupWidget.h"
#include <QHBoxLayout>
#include <QMouseEvent>
#include <algorithm>

FlagIndicatorWidget::FlagIndicatorWidget(const std::vector<HealthFlag>& flags, QWidget* parent)
    : QFrame(parent)
    , m_flags(sortFlagsBySeverity(flags))
    , m_textLabel(nullptr)
    , m_arrowLabel(nullptr)
    , m_popup(nullptr)
    , m_hasIssues(false)
{
    // Filter out "by design" info flags - they're not really issues
    std::vector<HealthFlag> displayFlags;
    for (const auto& flag : m_flags) {
        // Skip flags that indicate normal behavior
        if (flag.id == L"NOT_STARTED_BY_DESIGN" ||
            flag.id == L"DRIVER_NOT_LOADED_BY_DESIGN" ||
            flag.id == L"SYSTEM_CRITICAL") {
            continue;
        }
        displayFlags.push_back(flag);
    }
    m_flags = displayFlags;
    m_hasIssues = !m_flags.empty();
    
    setupUi();
    updateAppearance();
}

void FlagIndicatorWidget::setupUi()
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
    
    // Arrow indicator (only if has issues)
    m_arrowLabel = new QLabel("▼");
    m_arrowLabel->setFixedWidth(12);
    layout->addWidget(m_arrowLabel);
    
    if (m_hasIssues) {
        setCursor(Qt::PointingHandCursor);
    }
}

void FlagIndicatorWidget::updateAppearance()
{
    if (!m_hasIssues) {
        // No issues - gray appearance
        m_textLabel->setText("No issues detected");
        m_arrowLabel->setVisible(false);
        
        setStyleSheet(
            "FlagIndicatorWidget {"
            "   border: 1px solid #666666;"
            "   border-radius: 4px;"
            "   background-color: rgba(100, 100, 100, 0.1);"
            "}"
        );
        m_textLabel->setStyleSheet("color: #888888;");
    } else {
        // Has issues - show most critical
        const HealthFlag* critical = getMostCriticalFlag();
        if (critical) {
            // Truncate description if too long
            QString desc = QString::fromStdWString(critical->description);
            if (desc.length() > 25) {
                desc = desc.left(22) + "...";
            }
            m_textLabel->setText(desc);
            
            QString outlineColor = getOutlineColor(critical->severity);
            QString bgColor = getBackgroundColor(critical->severity);
            
            setStyleSheet(QString(
                "FlagIndicatorWidget {"
                "   border: 2px solid %1;"
                "   border-radius: 4px;"
                "   background-color: %2;"
                "}"
            ).arg(outlineColor, bgColor));
            
            m_textLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(outlineColor));
            m_arrowLabel->setStyleSheet(QString("color: %1;").arg(outlineColor));
        }
        m_arrowLabel->setVisible(true);
    }
}

void FlagIndicatorWidget::mousePressEvent(QMouseEvent* event)
{
    if (!m_hasIssues) {
        return;
    }
    
    if (event->button() == Qt::LeftButton) {
        // Create popup if not exists
        if (!m_popup) {
            m_popup = new FlagPopupWidget(m_flags, nullptr);
        }
        
        // Check if popup was just closed (within 100ms) - prevents reopen on same click
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

const HealthFlag* FlagIndicatorWidget::getMostCriticalFlag() const
{
    if (m_flags.empty()) {
        return nullptr;
    }
    // Flags are already sorted, first one is most critical
    return &m_flags[0];
}

std::vector<HealthFlag> FlagIndicatorWidget::sortFlagsBySeverity(const std::vector<HealthFlag>& flags)
{
    std::vector<HealthFlag> sorted = flags;
    
    std::sort(sorted.begin(), sorted.end(), [](const HealthFlag& a, const HealthFlag& b) {
        // Critical < Warning < Caution < Info (we want Critical first)
        return static_cast<int>(a.severity) < static_cast<int>(b.severity);
    });
    
    return sorted;
}

QString FlagIndicatorWidget::getOutlineColor(HealthFlagSeverity severity)
{
    switch (severity) {
        case HealthFlagSeverity::Critical: return "#e74c3c";  // Red
        case HealthFlagSeverity::Warning:  return "#f39c12";  // Orange
        case HealthFlagSeverity::Caution:  return "#f1c40f";  // Yellow
        case HealthFlagSeverity::Info:     return "#3498db";  // Blue
        default:                           return "#95a5a6";  // Gray
    }
}

QString FlagIndicatorWidget::getBackgroundColor(HealthFlagSeverity severity)
{
    switch (severity) {
        case HealthFlagSeverity::Critical: return "rgba(231, 76, 60, 0.15)";
        case HealthFlagSeverity::Warning:  return "rgba(243, 156, 18, 0.15)";
        case HealthFlagSeverity::Caution:  return "rgba(241, 196, 15, 0.15)";
        case HealthFlagSeverity::Info:     return "rgba(52, 152, 219, 0.15)";
        default:                           return "rgba(149, 165, 166, 0.1)";
    }
}