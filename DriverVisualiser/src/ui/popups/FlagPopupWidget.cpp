#include "FlagPopupWidget.h"
#include "AppTheme.h"
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
    setStyleSheet(QString(
        "FlagPopupWidget {"
        "   background-color: %1;"
        "   border: 1px solid %2;"
        "   border-radius: 0px;"
        "}"
    ).arg(AppTheme::colors().bgOverlay, AppTheme::colors().borderStrong));
    
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
    row->setMinimumHeight(40);
    
    QString outlineColor = getOutlineColor(flag.severity);
    
    // Clean card style with subtle border
    row->setStyleSheet(QString(
        "QFrame {"
        "   border: 1px solid %1;"
        "   border-radius: 6px;"
        "   background-color: %2;"
        "}"
    ).arg(AppTheme::colors().borderNormal, AppTheme::colors().bgCard));
    
    QHBoxLayout* layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(10);
    
    // Severity indicator - small colored pill badge
    QLabel* badge = new QLabel();
    QString badgeColor = outlineColor;
    QString badgeBg = (flag.severity == HealthFlagSeverity::Critical) 
        ? "rgba(231, 76, 60, 0.15)"
        : (flag.severity == HealthFlagSeverity::Warning)
            ? "rgba(243, 156, 18, 0.15)"
            : (flag.severity == HealthFlagSeverity::Caution)
                ? "rgba(241, 196, 15, 0.15)"
                : "rgba(93, 173, 226, 0.15)";
    
    badge->setFixedSize(8, 8);
    badge->setStyleSheet(QString(
        "background-color: %1;"
        "border-radius: 4px;"
        "border: none;"
    ).arg(badgeColor));
    layout->addWidget(badge);
    
    // Description text
    QLabel* text = new QLabel(QString::fromStdWString(flag.description));
    text->setWordWrap(true);
    text->setMinimumWidth(250);
    text->setStyleSheet(QString(
        "color: %1;"
        "border: none;"
        "background: transparent;"
        "font-size: 12px;"
    ).arg(AppTheme::colors().textValue));
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
        case HealthFlagSeverity::Critical: return AppTheme::colors().critical;
        case HealthFlagSeverity::Warning:  return AppTheme::colors().warning;
        case HealthFlagSeverity::Caution:  return AppTheme::colors().caution;
        case HealthFlagSeverity::Info:     return AppTheme::colors().accent;
        default:                           return "#95a5a6";
    }
}