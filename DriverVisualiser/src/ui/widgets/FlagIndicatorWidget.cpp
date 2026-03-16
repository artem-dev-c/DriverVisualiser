#include "FlagIndicatorWidget.h"
#include "AppTheme.h"
#include "FlagPopupWidget.h"
#include "IconProvider.h"
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QFontMetrics>
#include <algorithm>

// ============================================================================
// Construction
// ============================================================================

FlagIndicatorWidget::FlagIndicatorWidget(const std::vector<HealthFlag>& flags, QWidget* parent)
    : QFrame(parent)
    , m_flags(sortedBySeverity(flags))
{
    // Strip "by design" flags — they are not actionable issues
    std::vector<HealthFlag> actionable;
    for (const auto& flag : m_flags) {
        if (flag.id == L"NOT_STARTED_BY_DESIGN"      ||
            flag.id == L"DRIVER_NOT_LOADED_BY_DESIGN" ||
            flag.id == L"SYSTEM_CRITICAL") {
            continue;
        }
        actionable.push_back(flag);
    }
    m_flags     = actionable;
    m_hasIssues = !m_flags.empty();

    setupUi();
    updateAppearance();
}

// ============================================================================
// UI
// ============================================================================

void FlagIndicatorWidget::setupUi()
{
    // Fixed dimensions — aligns all indicators across cards
    setFixedHeight(34);
    setFixedWidth(190);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 10, 0);
    layout->setSpacing(6);

    m_textLabel = new QLabel();
    m_textLabel->setWordWrap(false);
    QFont f = m_textLabel->font();
    f.setPointSize(9);
    m_textLabel->setFont(f);
    layout->addWidget(m_textLabel, 1);

    // Small chevron icon — only visible when clickable (no button, just visual indicator)
    m_arrowLabel = new QLabel();
    m_arrowLabel->setPixmap(IconProvider::icon(
        IconProvider::ChevronDown,
        AppTheme::colors().textMuted,
        14
    ).pixmap(14, 14));
    m_arrowLabel->setFixedSize(18, 18);
    layout->addWidget(m_arrowLabel);

    if (m_hasIssues) {
        setCursor(Qt::PointingHandCursor);
    }
}

void FlagIndicatorWidget::updateAppearance()
{
    if (!m_hasIssues) {
        // Healthy state — subdued, no emphasis
        m_textLabel->setText("No issues");
        m_textLabel->setStyleSheet(QString("color: %1; background: transparent;").arg(AppTheme::colors().textMuted));
        m_arrowLabel->setVisible(false);

        setStyleSheet(QString(
            "FlagIndicatorWidget {"
            "   border: 1px solid %1;"
            "   border-radius: 8px;"
            "   background-color: transparent;"
            "}"
        ).arg(AppTheme::colors().borderSubtle));
        return;
    }

    const HealthFlag* flag = mostCriticalFlag();
    if (!flag) return;

    // Get full description for tooltip
    QString fullDesc = QString::fromStdWString(flag->description);
    
    // Use Qt's eliding for proper text truncation
    QFontMetrics fm(m_textLabel->font());
    int availableWidth = 160;  // Approximate space after accounting for padding and arrow
    QString elidedText = fm.elidedText(fullDesc, Qt::ElideRight, availableWidth);
    
    m_textLabel->setText(elidedText);
    
    // Add tooltip showing full text if truncated
    if (elidedText != fullDesc) {
        setToolTip(fullDesc);
    } else {
        setToolTip("");  // Clear tooltip if not truncated
    }

    QString outline = outlineColor(flag->severity);
    QString bg      = backgroundColor(flag->severity);

    setStyleSheet(QString(
        "FlagIndicatorWidget {"
        "   border: 1px solid %1;"
        "   border-radius: 8px;"
        "   background-color: %2;"
        "}"
    ).arg(outline, bg));

    m_textLabel->setStyleSheet(
        QString("color: %1; font-weight: bold; background: transparent;").arg(outline)
    );
    
    // Update arrow icon color - use solid color for warnings (not rgba)
    QString iconColor = outline;
    if (flag->severity == HealthFlagSeverity::Warning) {
        iconColor = AppTheme::colors().warning;  // Solid orange for visibility
    }
    m_arrowLabel->setPixmap(IconProvider::icon(
        IconProvider::ChevronDown,
        iconColor,
        14
    ).pixmap(14, 14));
    m_arrowLabel->setVisible(true);
}

// ============================================================================
// Interaction
// ============================================================================

void FlagIndicatorWidget::mousePressEvent(QMouseEvent* event)
{
    if (!m_hasIssues) {
        QFrame::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (!m_popup) {
            m_popup = new FlagPopupWidget(m_flags, nullptr);
        }
        if (m_popup->wasRecentlyClosed()) return;

        if (m_popup->isVisible()) {
            m_popup->hide();
        } else {
            m_popup->showRelativeTo(this);
        }
    }

    QFrame::mousePressEvent(event);
}

// ============================================================================
// Helpers
// ============================================================================

const HealthFlag* FlagIndicatorWidget::mostCriticalFlag() const
{
    return m_flags.empty() ? nullptr : &m_flags[0];
}

std::vector<HealthFlag> FlagIndicatorWidget::sortedBySeverity(const std::vector<HealthFlag>& flags)
{
    std::vector<HealthFlag> sorted = flags;
    std::sort(sorted.begin(), sorted.end(), [](const HealthFlag& a, const HealthFlag& b) {
        return static_cast<int>(a.severity) < static_cast<int>(b.severity);
    });
    return sorted;
}

QString FlagIndicatorWidget::outlineColor(HealthFlagSeverity severity)
{
    switch (severity) {
        case HealthFlagSeverity::Critical: return AppTheme::colors().critical;
        case HealthFlagSeverity::Warning:  return AppTheme::colors().warningOutline;
        case HealthFlagSeverity::Caution:  return AppTheme::colors().caution;
        case HealthFlagSeverity::Info:     return AppTheme::colors().accent;
        default:                           return "#555555";
    }
}

QString FlagIndicatorWidget::backgroundColor(HealthFlagSeverity severity)
{
    switch (severity) {
        case HealthFlagSeverity::Critical: return "rgba(231, 76, 60, 0.12)";
        case HealthFlagSeverity::Warning:  return "rgba(243, 156, 18, 0.08)";  // reduced from 0.12
        case HealthFlagSeverity::Caution:  return "rgba(241, 196, 15, 0.10)";
        case HealthFlagSeverity::Info:     return AppTheme::isDark() ? "rgba(93, 173, 226, 0.10)" : "rgba(41, 128, 185, 0.10)";
        default:                           return "transparent";
    }
}