#include "ErrorLogIndicatorWidget.h"
#include "AppTheme.h"
#include "ErrorLogPopupWidget.h"
#include <QHBoxLayout>
#include <QMouseEvent>

// ============================================================================
// Construction
// ============================================================================

ErrorLogIndicatorWidget::ErrorLogIndicatorWidget(const std::vector<ErrorLogEntry>& entries,
                                                 int logDays,
                                                 QWidget* parent)
    : QFrame(parent)
    , m_entries(entries)
    , m_logDays(logDays)
    , m_hasLogs(!entries.empty())
{
    setupUi();
    updateAppearance();
}

// ============================================================================
// UI
// ============================================================================

void ErrorLogIndicatorWidget::setupUi()
{
    // Fixed dimensions — mirrors FlagIndicatorWidget for consistent alignment
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

    m_arrowLabel = new QLabel("▾");
    m_arrowLabel->setFixedWidth(10);
    QFont af = m_arrowLabel->font();
    af.setPointSize(9);
    m_arrowLabel->setFont(af);
    layout->addWidget(m_arrowLabel);

    if (m_hasLogs) {
        setCursor(Qt::PointingHandCursor);
    }
}

void ErrorLogIndicatorWidget::updateAppearance()
{
    if (!m_hasLogs) {
        // Neutral "no logs" — subdued, mirrors FlagIndicatorWidget healthy state
        m_textLabel->setText("No logs");
        m_textLabel->setStyleSheet(QString("color: %1; background: transparent;").arg(AppTheme::colors().textMuted));
        m_arrowLabel->setVisible(false);

        setStyleSheet(QString(
            "ErrorLogIndicatorWidget {"
            "   border: 1px solid %1;"
            "   border-radius: 8px;"
            "   background-color: transparent;"
            "}"
        ).arg(AppTheme::colors().borderSubtle));
        return;
    }

    // Build compact summary text
    int critical = countCritical();
    int errors   = countErrors();
    int warnings = countWarnings();
    int total    = (int)m_entries.size();

    QString dayLabel = QString("(%1d)").arg(m_logDays);
    QString text;

    if (critical > 0) {
        text = QString("%1 critical %2").arg(critical).arg(dayLabel);
    } else if (errors > 0) {
        text = QString("%1 error%2 %3")
                   .arg(errors)
                   .arg(errors > 1 ? "s" : "")
                   .arg(dayLabel);
    } else {
        text = QString("%1 warning%2 %3")
                   .arg(warnings)
                   .arg(warnings > 1 ? "s" : "")
                   .arg(dayLabel);
    }

    m_textLabel->setText(text);

    // Color severity based on errors+critical count
    int errCount     = critical + errors;
    QString outline  = outlineColor(errCount);
    QString bg       = bgColor(errCount);

    setStyleSheet(QString(
        "ErrorLogIndicatorWidget {"
        "   border: 1px solid %1;"
        "   border-radius: 8px;"
        "   background-color: %2;"
        "}"
    ).arg(outline, bg));

    m_textLabel->setStyleSheet(
        QString("color: %1; font-weight: bold; background: transparent;").arg(outline)
    );
    m_arrowLabel->setStyleSheet(
        QString("color: %1; background: transparent;").arg(outline)
    );
    m_arrowLabel->setVisible(true);
}

// ============================================================================
// Interaction
// ============================================================================

void ErrorLogIndicatorWidget::mousePressEvent(QMouseEvent* event)
{
    if (!m_hasLogs) {
        QFrame::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (!m_popup) {
            m_popup = new ErrorLogPopupWidget(m_entries, m_logDays, nullptr);
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
// Counting
// ============================================================================

int ErrorLogIndicatorWidget::countCritical() const
{
    int n = 0;
    for (const auto& e : m_entries) {
        if (e.level == L"Critical") ++n;
    }
    return n;
}

int ErrorLogIndicatorWidget::countErrors() const
{
    int n = 0;
    for (const auto& e : m_entries) {
        if (e.level == L"Error" || e.level == L"Critical") ++n;
    }
    return n;
}

int ErrorLogIndicatorWidget::countWarnings() const
{
    int n = 0;
    for (const auto& e : m_entries) {
        if (e.level == L"Warning") ++n;
    }
    return n;
}

// ============================================================================
// Color Helpers
// ============================================================================

QString ErrorLogIndicatorWidget::outlineColor(int errorCount)
{
    if (errorCount >= 5) return AppTheme::colors().critical;
    if (errorCount >= 2) return AppTheme::colors().warningOutline;
    if (errorCount >= 1) return AppTheme::colors().caution;
    return AppTheme::colors().accent;
}

QString ErrorLogIndicatorWidget::bgColor(int errorCount)
{
    if (errorCount >= 5) return "rgba(231, 76, 60, 0.12)";
    if (errorCount >= 2) return "rgba(243, 156, 18, 0.08)";   // reduced from 0.12
    if (errorCount >= 1) return "rgba(241, 196, 15, 0.10)";
    return AppTheme::isDark() ? "rgba(93, 173, 226, 0.10)" : "rgba(41, 128, 185, 0.10)";
}