#include "DayEventsPopup.h"
#include "AppTheme.h"
#include <QLabel>
#include <QDateTime>
#include <QCloseEvent>
#include <QApplication>
#include <QScreen>
#include <algorithm>

// ============================================================================
// Construction
// ============================================================================

DayEventsPopup::DayEventsPopup(const QDate& date,
                               const std::vector<ErrorLogEntry>& events,
                               QWidget* parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)  // Same as DriverInfoPopup
    , m_date(date)
    , m_events(events)
{
    setAttribute(Qt::WA_DeleteOnClose);

    // Sort events by time (most recent first)
    std::sort(m_events.begin(), m_events.end(),
        [](const ErrorLogEntry& a, const ErrorLogEntry& b) {
            return a.timestamp > b.timestamp;  // Descending (newest first)
        });

    setupUi();
    populateEvents();

    // Close timer setup (300ms cooldown)
    m_closeTimer.setSingleShot(true);
    connect(&m_closeTimer, &QTimer::timeout, this, [this]() {
        m_recentlyClosed = false;
    });
}

// ============================================================================
// UI Setup
// ============================================================================

void DayEventsPopup::setupUi()
{
    setFixedWidth(400);  // Increased from 350px for better single-line content
    setMaximumHeight(420);

    // Main popup styling - SQUARE border (0px radius)
    setStyleSheet(QString(
        "DayEventsPopup {"
        "   background-color: %1;"
        "   border: 1px solid %2;"
        "   border-radius: 0px;"  // Square corners
        "}"
    ).arg(AppTheme::colors().bgOverlay, AppTheme::colors().borderStrong));

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(1, 1, 1, 1);  // 1px margin for border visibility
    mainLayout->setSpacing(0);

    // Header
    QLabel* titleLabel = new QLabel(QString("Events on %1").arg(m_date.toString("MMMM d, yyyy")));
    titleLabel->setStyleSheet(QString(
        "QLabel {"
        "   color: %1;"
        "   font-size: 13px;"
        "   font-weight: bold;"
        "   padding: 16px 20px 12px 20px;"
        "   background: transparent;"
        "   border: none;"
        "}"
    ).arg(AppTheme::colors().textPrimary));
    mainLayout->addWidget(titleLabel);

    // Event count subtitle
    QLabel* countLabel = new QLabel(QString("%1 event%2")
        .arg(m_events.size())
        .arg(m_events.size() == 1 ? "" : "s"));
    countLabel->setStyleSheet(QString(
        "QLabel {"
        "   color: %1;"
        "   font-size: 11px;"
        "   padding: 0px 20px 12px 20px;"
        "   background: transparent;"
        "   border: none;"
        "}"
    ).arg(AppTheme::colors().textSecondary));
    mainLayout->addWidget(countLabel);

    // Scroll area
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setStyleSheet(QString(
        "QScrollArea {"
        "   border: none;"
        "   background: transparent;"
        "}"
        "QScrollBar:vertical {"
        "   background: transparent;"
        "   width: 8px;"
        "   margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background: %1;"
        "   border-radius: 4px;"
        "   min-height: 30px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "   background: %2;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "   height: 0px;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "   background: transparent;"
        "}"
    ).arg(AppTheme::colors().borderNormal, AppTheme::colors().borderStrong));

    m_contentWidget = new QWidget();
    m_contentWidget->setStyleSheet("background: transparent;");
    
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(20, 0, 20, 20);
    m_contentLayout->setSpacing(8);

    m_scrollArea->setWidget(m_contentWidget);
    mainLayout->addWidget(m_scrollArea);
}

void DayEventsPopup::populateEvents()
{
    for (const auto& entry : m_events) {
        QWidget* card = createEventCard(entry);
        m_contentLayout->addWidget(card);
    }

    m_contentLayout->addStretch();
}

QWidget* DayEventsPopup::createEventCard(const ErrorLogEntry& entry)
{
    QFrame* card = new QFrame();
    card->setStyleSheet(QString(
        "QFrame {"
        "   background-color: %1;"
        "   border: 1px solid %2;"
        "   border-radius: 6px;"
        "}"
    ).arg(AppTheme::colors().bgCard, AppTheme::colors().borderNormal));

    QVBoxLayout* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(6);

    // Top row: Badge (LEFT), Event ID, Time (RIGHT)
    QHBoxLayout* topRow = new QHBoxLayout();
    topRow->setSpacing(10);

    // Level badge with colored background - like ErrorLogPopup
    QString severityColor = getSeverityColor(entry.level);
    QString bgColor;
    if (entry.level == L"Critical" || entry.level == L"Error") {
        bgColor = "rgba(231, 76, 60, 0.2)";
    } else if (entry.level == L"Warning") {
        bgColor = "rgba(243, 156, 18, 0.2)";
    } else {
        bgColor = "rgba(93, 173, 226, 0.2)";
    }

    QLabel* levelLabel = new QLabel(QString::fromStdWString(entry.level));
    levelLabel->setStyleSheet(QString(
        "QLabel {"
        "   color: %1;"
        "   background-color: %2;"
        "   font-size: 10px;"
        "   font-weight: bold;"
        "   padding: 4px 10px;"
        "   border-radius: 4px;"
        "   border: none;"
        "}"
    ).arg(severityColor, bgColor));
    topRow->addWidget(levelLabel);

    // Event ID
    if (entry.eventId != 0) {
        QLabel* idLabel = new QLabel(QString("Event %1").arg(entry.eventId));
        idLabel->setStyleSheet(QString(
            "color: %1; font-size: 10px; background: transparent; border: none; padding: 0px;"
        ).arg(AppTheme::colors().textSecondary));
        topRow->addWidget(idLabel);
    }

    topRow->addStretch();

    // Time on the RIGHT
    QLabel* timeLabel = new QLabel(formatTime(entry));
    timeLabel->setStyleSheet(QString(
        "color: %1; font-size: 10px; background: transparent; border: none;"
    ).arg(AppTheme::colors().textSecondary));
    topRow->addWidget(timeLabel);

    layout->addLayout(topRow);

    // Driver name (if available)
    if (!entry.driverName.empty()) {
        QLabel* driverLabel = new QLabel(QString::fromStdWString(entry.driverName));
        driverLabel->setStyleSheet(QString(
            "color: %1; font-size: 11px; font-weight: bold; background: transparent; border: none;"
        ).arg(AppTheme::colors().textValue));
        layout->addWidget(driverLabel);
    }

    // Message
    if (!entry.message.empty()) {
        QString message = QString::fromStdWString(entry.message);
        
        // Truncate very long messages
        if (message.length() > 300) {
            message = message.left(297) + "...";
        }

        QLabel* msgLabel = new QLabel(message);
        msgLabel->setWordWrap(true);
        msgLabel->setStyleSheet(QString(
            "color: %1; font-size: 11px; background: transparent; border: none; line-height: 1.4;"
        ).arg(AppTheme::colors().textSecondary));
        layout->addWidget(msgLabel);
    }

    return card;
}

// ============================================================================
// Helpers
// ============================================================================

QString DayEventsPopup::formatTime(const ErrorLogEntry& entry) const
{
    auto timeT = std::chrono::system_clock::to_time_t(entry.timestamp);
    QDateTime dt = QDateTime::fromSecsSinceEpoch(timeT);
    return dt.toString("HH:mm:ss");  // 24-hour format (e.g., "14:45:30")
}

QString DayEventsPopup::getSeverityColor(const std::wstring& level) const
{
    if (level == L"Critical" || level == L"Error") {
        return "#e74c3c";  // Red
    } else if (level == L"Warning") {
        return "#f39c12";  // Orange
    } else {
        return "#5dade2";  // Blue
    }
}

QString DayEventsPopup::getSeverityEmoji(const std::wstring& level) const
{
    if (level == L"Critical" || level == L"Error") {
        return "🔴";
    } else if (level == L"Warning") {
        return "🟠";
    } else {
        return "🔵";
    }
}

// ============================================================================
// Show/Close
// ============================================================================

void DayEventsPopup::showRelativeTo(QWidget* anchor)
{
    if (!anchor) {
        show();
        return;
    }

    // Position to the right of anchor, or left if not enough space
    QPoint anchorPos = anchor->mapToGlobal(QPoint(0, 0));
    QRect anchorRect(anchorPos, anchor->size());
    QScreen* screen = QApplication::screenAt(anchorPos);
    QRect screenGeometry = screen ? screen->geometry() : QRect();

    // Adjust height based on content (smaller now)
    int contentHeight = m_contentWidget->sizeHint().height();
    int desiredHeight = std::min(contentHeight + 80, 420);  // Reduced from 600
    setFixedHeight(desiredHeight);

    int popupX = anchorRect.right() + 10;  // 10px gap
    int popupY = anchorRect.top();

    // Check if popup fits on the right
    if (popupX + width() > screenGeometry.right()) {
        // Position on the left instead
        popupX = anchorRect.left() - width() - 10;
    }

    // Ensure popup is on screen vertically
    if (popupY + height() > screenGeometry.bottom()) {
        popupY = screenGeometry.bottom() - height() - 20;
    }
    if (popupY < screenGeometry.top()) {
        popupY = screenGeometry.top() + 20;
    }

    move(popupX, popupY);
    show();
    raise();
    // Don't call activateWindow() - let main window keep focus
}

void DayEventsPopup::closeEvent(QCloseEvent* event)
{
    m_recentlyClosed = true;
    m_closeTimer.start(300);  // 300ms cooldown
    QFrame::closeEvent(event);
}

bool DayEventsPopup::wasRecentlyClosed() const
{
    return m_recentlyClosed;
}