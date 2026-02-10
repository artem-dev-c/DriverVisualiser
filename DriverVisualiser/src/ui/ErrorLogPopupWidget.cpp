#include "ErrorLogPopupWidget.h"
#include <QScrollArea>
#include <QApplication>
#include <QScreen>
#include <QDateTime>
#include <QKeyEvent>
#include <QFocusEvent>

ErrorLogPopupWidget::ErrorLogPopupWidget(const std::vector<ErrorLogEntry>& entries, QWidget* parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
    , m_entries(entries)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setupUi();
}

void ErrorLogPopupWidget::setupUi()
{
    setFixedWidth(520);
    setMaximumHeight(400);
    
    // Styling: rounded border, drop shadow
    setStyleSheet(
        "ErrorLogPopupWidget {"
        "   background-color: #2b2b2b;"
        "   border: 1px solid #444;"
        "   border-radius: 6px;"
        "}"
    );
    
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
    );
    
    QWidget* contentWidget = new QWidget();
    contentWidget->setStyleSheet("background-color: transparent;");
    QVBoxLayout* layout = new QVBoxLayout(contentWidget);
    layout->setContentsMargins(15, 13, 15, 13);
    layout->setSpacing(6);
    
    // === Header ===
    layout->addWidget(createSectionHeader(
        QString("Error Log (Last 7 Days) [%1]").arg(m_entries.size())
    ));
    
    layout->addSpacing(6);
    
    if (m_entries.empty()) {
        QLabel* noLogs = new QLabel("No error logs found ✓");
        noLogs->setStyleSheet("color: #27ae60; font-style: italic; font-size: 11px;");
        layout->addWidget(noLogs);
    } else {
        // Show up to 10 most recent entries
        int showCount = std::min(10, static_cast<int>(m_entries.size()));
        
        for (int i = 0; i < showCount; i++) {
            layout->addWidget(createLogRow(m_entries[i]));
            layout->addSpacing(4);
        }
        
        // "View more" message if there are more entries
        if (m_entries.size() > 10) {
            QLabel* moreLabel = new QLabel(
                QString("+ %1 more entries (open Event Viewer for full log)")
                .arg(m_entries.size() - 10)
            );
            moreLabel->setStyleSheet("color: #888; font-style: italic; font-size: 10px;");
            layout->addWidget(moreLabel);
        }
    }
    
    layout->addStretch();
    
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);
}

QLabel* ErrorLogPopupWidget::createSectionHeader(const QString& title)
{
    QLabel* header = new QLabel(title);
    header->setStyleSheet(
        "color: #5dade2;"
        "font-weight: bold;"
        "font-size: 12px;"
        "padding-top: 6px;"
        "padding-bottom: 4px;"
        "color: #5dade2;"
        "font-weight: bold;"
        "font-size: 12px;"
        "padding-top: 6px;"
        "padding-bottom: 4px;"
        "color: #5dade2;"
        "font-weight: bold;"
        "font-size: 12px;"
        "padding-top: 6px;"
        "padding-bottom: 4px;"
        "color: #5dade2;"
        "font-weight: bold;"
        "font-size: 12px;"
        "padding-top: 6px;"
        "padding-bottom: 4px;"
    );
    return header;
}

QWidget* ErrorLogPopupWidget::createLogRow(const ErrorLogEntry& entry)
{
    QWidget* row = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(row);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(4);
    
    // === Top line: Timestamp + Level badge ===
    QHBoxLayout* topLine = new QHBoxLayout();
    topLine->setSpacing(10);
    
    // Timestamp
    QString timestamp = formatTimestamp(entry.timestamp);
    QLabel* timeLabel = new QLabel(timestamp);
    timeLabel->setStyleSheet("color: #999; font-size: 10px;");
    topLine->addWidget(timeLabel);
    
    // Level badge
    QString levelText = QString::fromStdWString(entry.level);
    QString color = getSeverityColor(entry.level);
    QLabel* levelLabel = new QLabel(levelText);
    levelLabel->setStyleSheet(QString(
        "color: %1;"
        "font-weight: bold;"
        "font-size: 10px;"
    ).arg(color));
    topLine->addWidget(levelLabel);
    
    // Event ID
    QLabel* eventIdLabel = new QLabel(QString("(Event %1)").arg(entry.eventId));
    eventIdLabel->setStyleSheet("color: #777; font-size: 9px;");
    topLine->addWidget(eventIdLabel);
    
    topLine->addStretch();
    layout->addLayout(topLine);
    
    // === Message ===
    QString message = QString::fromStdWString(entry.message);
    // Truncate if very long
    if (message.length() > 200) {
        message = message.left(197) + "...";
    }
    
    QLabel* msgLabel = new QLabel(message);
    msgLabel->setWordWrap(true);
    msgLabel->setStyleSheet("color: #ddd; font-size: 11px;");
    layout->addWidget(msgLabel);
    
    // Styling: colored left border
    row->setStyleSheet(QString(
        "QWidget {"
        "   background-color: rgba(0, 0, 0, 0.2);"
        "   border-left: 3px solid %1;"
        "   border-radius: 3px;"
        "}"
    ).arg(color));
    
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
        return "#e74c3c";  // Red
    } else if (level == L"Error") {
        return "#e74c3c";  // Red
    } else if (level == L"Warning") {
        return "#f39c12";  // Orange
    } else {
        return "#3498db";  // Blue
    }
}