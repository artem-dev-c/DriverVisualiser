#include "ErrorLogPopupWidget.h"
#include "AppTheme.h"
#include <QScrollArea>
#include <QApplication>
#include <QScreen>
#include <QDateTime>
#include <QKeyEvent>
#include <QFocusEvent>

ErrorLogPopupWidget::ErrorLogPopupWidget(const std::vector<ErrorLogEntry>& entries,
                                           int logDays,
                                           QWidget* parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
    , m_entries(entries)
    , m_logDays(logDays)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setupUi();
}

void ErrorLogPopupWidget::setupUi()
{
    setFixedWidth(520);
    setMaximumHeight(400);
    
    // Main popup styling (match DriverInfoPopup)
    setStyleSheet(QString(
        "ErrorLogPopupWidget {"
        "   background-color: %1;"
        "   border: 1px solid %2;"
        "   border-radius: 0px;"
        "}"
    ).arg(AppTheme::colors().bgOverlay, AppTheme::colors().borderStrong));
    
    // Main layout with scroll area
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(1, 1, 1, 1);
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
        + AppTheme::scrollbarStyleSheet()
    );
    
    QWidget* contentWidget = new QWidget();
    contentWidget->setStyleSheet("background-color: transparent;");
    QVBoxLayout* layout = new QVBoxLayout(contentWidget);
    layout->setContentsMargins(15, 13, 15, 13);
    layout->setSpacing(6);
    
    // === Header ===
    layout->addWidget(createSectionHeader(
        QString("Error Log (Last %1 Days) [%2]").arg(m_logDays).arg(m_entries.size())
    ));
    
    layout->addSpacing(6);
    
    if (m_entries.empty()) {
        QLabel* noLogs = new QLabel("No error logs found ✓");
        noLogs->setStyleSheet(QString("color: %1; font-style: italic; font-size: 11px;").arg(AppTheme::colors().healthy));
        layout->addWidget(noLogs);
    } else {
        // Show all entries
        for (size_t i = 0; i < m_entries.size(); i++) {
            layout->addWidget(createLogRow(m_entries[i]));
            if (i < m_entries.size() - 1) {  // Don't add spacing after last item
                layout->addSpacing(4);
            }
        }
    }
    
    layout->addStretch();
    
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);
}

QLabel* ErrorLogPopupWidget::createSectionHeader(const QString& title)
{
    QLabel* header = new QLabel(title);
    header->setStyleSheet(QString(
        "color: %1;"
        "font-weight: bold;"
        "font-size: 12px;"
        "padding-top: 6px;"
        "padding-bottom: 4px;"
    ).arg(AppTheme::colors().accentText));
    return header;
}

QWidget* ErrorLogPopupWidget::createLogRow(const ErrorLogEntry& entry)
{
    QWidget* row = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(row);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(5);
    
    // === Top line: Level badge + Event ID ===
    QHBoxLayout* topLine = new QHBoxLayout();
    topLine->setSpacing(10);
    
    // Level badge - colored background
    QString levelText = QString::fromStdWString(entry.level);
    QString color = getSeverityColor(entry.level);
    QString bgColor = (entry.level == L"Critical" || entry.level == L"Error") 
        ? "rgba(231, 76, 60, 0.2)" 
        : (entry.level == L"Warning")
            ? "rgba(243, 156, 18, 0.2)"
            : "rgba(93, 173, 226, 0.2)";
    
    QLabel* levelLabel = new QLabel(levelText);
    levelLabel->setStyleSheet(QString(
        "color: %1;"
        "background-color: %2;"
        "font-weight: bold;"
        "font-size: 10px;"
        "padding: 4px 10px;"
        "border-radius: 4px;"
        "border: none;"
    ).arg(color, bgColor));
    topLine->addWidget(levelLabel);
    
    // Event ID - plain text, no pill
    QLabel* eventIdLabel = new QLabel(QString("Event %1").arg(entry.eventId));
    eventIdLabel->setStyleSheet(QString(
        "color: %1;"
        "font-size: 10px;"
        "background: transparent;"
        "border: none;"
        "padding: 0px;"
    ).arg(AppTheme::colors().textSecondary));
    topLine->addWidget(eventIdLabel);
    
    topLine->addStretch();
    
    // Timestamp on the right - plain text, no pill
    QString timestamp = formatTimestamp(entry.timestamp);
    QLabel* timeLabel = new QLabel(timestamp);
    timeLabel->setStyleSheet(QString(
        "color: %1;"
        "font-size: 10px;"
        "background: transparent;"
        "border: none;"
        "padding: 0px;"
    ).arg(AppTheme::colors().textMuted));
    topLine->addWidget(timeLabel);
    
    layout->addLayout(topLine);
    
    // === Message - with more opacity ===
    QString message = QString::fromStdWString(entry.message);
    // Truncate if very long
    if (message.length() > 300) {
        message = message.left(297) + "...";
    }
    
    QLabel* msgLabel = new QLabel(message);
    msgLabel->setWordWrap(true);
    msgLabel->setStyleSheet(QString(
        "color: %1;"
        "font-size: 11px;"
        "line-height: 1.4;"
        "background: transparent;"
        "border: none;"
    ).arg(AppTheme::colors().textSecondary));  // Use textSecondary for subtle but visible text
    layout->addWidget(msgLabel);
    
    // Styling: subtle card with border
    row->setStyleSheet(QString(
        "QWidget {"
        "   background-color: %1;"
        "   border: 1px solid %2;"
        "   border-radius: 6px;"
        "}"
    ).arg(AppTheme::colors().bgCard, AppTheme::colors().borderNormal));
    
    return row;
}

// ============================================================================
// Popup Behavior (Same as DriverInfoPopup)
// ============================================================================

void ErrorLogPopupWidget::showRelativeTo(QWidget* anchor)
{
    if (!anchor) {
        return;
    }
    
    // Get anchor position in global coordinates
    QPoint anchorGlobal = anchor->mapToGlobal(anchor->rect().bottomRight());
    
    // Position popup to align right edge with button, 6px below
    QPoint popupPos = anchorGlobal;
    popupPos.setX(popupPos.x() - width());
    popupPos.setY(popupPos.y() + 6);
    
    // Ensure popup stays on screen
    QScreen* screen = QApplication::screenAt(anchorGlobal);
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();
        
        if (popupPos.x() < screenGeometry.left()) {
            popupPos.setX(screenGeometry.left());
        }
        if (popupPos.x() + width() > screenGeometry.right()) {
            popupPos.setX(screenGeometry.right() - width());
        }
        if (popupPos.y() + height() > screenGeometry.bottom()) {
            popupPos.setY(anchorGlobal.y() - height() - 6);
        }
    }
    
    move(popupPos);
    show();
    raise();
    activateWindow();
}

bool ErrorLogPopupWidget::wasRecentlyClosed() const
{
    if (m_closeTime == 0) {
        return false;
    }
    
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    return (now - m_closeTime) < 100;
}

void ErrorLogPopupWidget::focusOutEvent(QFocusEvent* event)
{
    hide();
    QFrame::focusOutEvent(event);
}

void ErrorLogPopupWidget::hideEvent(QHideEvent* event)
{
    m_closeTime = QDateTime::currentMSecsSinceEpoch();
    QFrame::hideEvent(event);
}

bool ErrorLogPopupWidget::event(QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            hide();
            return true;
        }
    }
    return QFrame::event(event);
}

// ============================================================================
// Formatting Helpers
// ============================================================================

QString ErrorLogPopupWidget::formatTimestamp(const std::chrono::system_clock::time_point& timestamp)
{
    auto timeT = std::chrono::system_clock::to_time_t(timestamp);
    QDateTime dt = QDateTime::fromSecsSinceEpoch(timeT);
    
    // Format: "Feb 09 14:32"
    return dt.toString("MMM dd hh:mm");
}

QString ErrorLogPopupWidget::getSeverityColor(const std::wstring& level)
{
    if (level == L"Critical") {
        return AppTheme::colors().critical;
    } else if (level == L"Error") {
        return AppTheme::colors().critical;
    } else if (level == L"Warning") {
        return AppTheme::colors().warning;
    } else {
        return AppTheme::colors().accent;
    }
}