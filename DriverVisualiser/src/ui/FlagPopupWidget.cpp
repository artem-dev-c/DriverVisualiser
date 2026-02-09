#include "FlagPopupWidget.h"
#include <QApplication>
#include <QScreen>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QDateTime>

FlagPopupWidget::FlagPopupWidget(const std::vector<HealthFlag>& sortedFlags, QWidget* parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setFocusPolicy(Qt::StrongFocus);
    
    setupUi(sortedFlags);
}

void FlagPopupWidget::setupUi(const std::vector<HealthFlag>& sortedFlags)
{
    setMinimumWidth(320);
    setMaximumWidth(420);
    setMaximumHeight(400);
    
    // Main popup styling
    setStyleSheet(
        "FlagPopupWidget {"
        "   background-color: #2b2b2b;"
        "   border: 1px solid #555555;"
        "   border-radius: 6px;"
        "}"
    );
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    
    // Add each flag as a row
    for (const auto& flag : sortedFlags) {
        QFrame* row = createFlagRow(flag);
        layout->addWidget(row);
    }
}

QFrame* FlagPopupWidget::createFlagRow(const HealthFlag& flag)
{
    QFrame* row = new QFrame();
    row->setMinimumHeight(36);
    
    QString outlineColor = getOutlineColor(flag.severity);
    QString bgColor = getBackgroundColor(flag.severity);
    
    row->setStyleSheet(QString(
        "QFrame {"
        "   border: 1px solid %1;"
        "   border-radius: 4px;"
        "   background-color: %2;"
        "}"
    ).arg(outlineColor, bgColor));
    
    QHBoxLayout* layout = new QHBoxLayout(row);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(10);
    
    // Severity indicator dot
    QLabel* dot = new QLabel();
    dot->setFixedSize(10, 10);
    dot->setStyleSheet(QString(
        "background-color: %1;"
        "border-radius: 5px;"
        "border: none;"
    ).arg(outlineColor));
    layout->addWidget(dot);
    
    // Description text
    QLabel* text = new QLabel(QString::fromStdWString(flag.description));
    text->setWordWrap(true);
    text->setMinimumWidth(250);
    text->setStyleSheet(QString(
        "color: %1;"
        "border: none;"
        "background: transparent;"
        "font-size: 12px;"
    ).arg(getTextColor(flag.severity)));
    layout->addWidget(text, 1);
    
    return row;
}

void FlagPopupWidget::showRelativeTo(QWidget* anchor)
{
    // Position below and to the left of the anchor
    QPoint pos = anchor->mapToGlobal(QPoint(0, anchor->height() + 4));
    
    // Adjust if would go off screen
    QScreen* screen = QApplication::screenAt(pos);
    if (screen) {
        QRect screenRect = screen->availableGeometry();
        
        // Adjust horizontal position
        if (pos.x() + width() > screenRect.right()) {
            pos.setX(screenRect.right() - width() - 10);
        }
        
        // Adjust vertical position (show above if not enough space below)
        if (pos.y() + height() > screenRect.bottom()) {
            pos.setY(anchor->mapToGlobal(QPoint(0, 0)).y() - height() - 4);
        }
    }
    
    move(pos);
    show();
    setFocus();
}

void FlagPopupWidget::focusOutEvent(QFocusEvent* event)
{
    Q_UNUSED(event);
    hide();
}

void FlagPopupWidget::hideEvent(QHideEvent* event)
{
    Q_UNUSED(event);
    m_closeTime = QDateTime::currentMSecsSinceEpoch();
    QFrame::hideEvent(event);
}

bool FlagPopupWidget::wasRecentlyClosed() const
{
    // Return true if closed within last 150ms
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    return (now - m_closeTime) < 150;
}

bool FlagPopupWidget::event(QEvent* event)
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

QString FlagPopupWidget::getOutlineColor(HealthFlagSeverity severity)
{
    switch (severity) {
        case HealthFlagSeverity::Critical: return "#e74c3c";  // Red
        case HealthFlagSeverity::Warning:  return "#f39c12";  // Orange
        case HealthFlagSeverity::Caution:  return "#f1c40f";  // Yellow
        case HealthFlagSeverity::Info:     return "#3498db";  // Blue
        default:                           return "#95a5a6";  // Gray
    }
}

QString FlagPopupWidget::getBackgroundColor(HealthFlagSeverity severity)
{
    switch (severity) {
        case HealthFlagSeverity::Critical: return "rgba(231, 76, 60, 0.2)";
        case HealthFlagSeverity::Warning:  return "rgba(243, 156, 18, 0.2)";
        case HealthFlagSeverity::Caution:  return "rgba(241, 196, 15, 0.2)";
        case HealthFlagSeverity::Info:     return "rgba(52, 152, 219, 0.2)";
        default:                           return "rgba(149, 165, 166, 0.15)";
    }
}

QString FlagPopupWidget::getTextColor(HealthFlagSeverity severity)
{
    switch (severity) {
        case HealthFlagSeverity::Critical: return "#ff6b5b";  // Lighter red
        case HealthFlagSeverity::Warning:  return "#ffb347";  // Lighter orange
        case HealthFlagSeverity::Caution:  return "#ffe066";  // Lighter yellow
        case HealthFlagSeverity::Info:     return "#5dade2";  // Lighter blue
        default:                           return "#bdc3c7";  // Light gray
    }
}