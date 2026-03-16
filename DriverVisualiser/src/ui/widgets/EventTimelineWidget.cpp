#include "EventTimelineWidget.h"
#include "AppTheme.h"
#include "DayEventsPopup.h"
#include <QPainter>
#include <QPainterPath>
#include <QDateTime>
#include <QVBoxLayout>
#include <QApplication>
#include <QScreen>
#include <algorithm>

// ============================================================================
// Construction
// ============================================================================

EventTimelineWidget::EventTimelineWidget(QWidget* parent)
    : QFrame(parent)
{
    setupUi();
}

void EventTimelineWidget::setupUi()
{
    setFixedHeight(220);
    
    // Match card styling
    setStyleSheet(QString(
        "EventTimelineWidget {"
        "   background-color: %1;"
        "   border: 1px solid %2;"
        "   border-radius: 10px;"
        "}"
    ).arg(AppTheme::colors().bgCard, AppTheme::colors().borderNormal));

    // Tooltip label (proper window with SQUARE border)
    m_tooltipLabel = new QLabel(this);
    m_tooltipLabel->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    m_tooltipLabel->setStyleSheet(QString(
        "QLabel {"
        "   background-color: %1;"
        "   border: 1px solid %2;"
        "   border-radius: 0px;"  // SQUARE/RECTANGULAR border
        "   padding: 12px 14px;"
        "}"
    ).arg(AppTheme::colors().bgOverlay, AppTheme::colors().borderStrong));
    m_tooltipLabel->hide();

    setMouseTracking(true);
}

// ============================================================================
// Data Management
// ============================================================================

void EventTimelineWidget::setEvents(const std::vector<ErrorLogEntry>& events)
{
    m_allEvents = events;
    aggregateEventsByDay();
    update();
}

void EventTimelineWidget::setDayRange(int days)
{
    m_dayRange = days;
    aggregateEventsByDay();
    update();
}

void EventTimelineWidget::aggregateEventsByDay()
{
    m_dayData.clear();

    // Initialize all days in range
    QDate today = QDate::currentDate();
    QDate startDate = today.addDays(-m_dayRange + 1);

    for (int i = 0; i < m_dayRange; ++i) {
        DayData day;
        day.date = startDate.addDays(i);
        m_dayData.push_back(day);
    }

    // Aggregate events into days
    for (const auto& entry : m_allEvents) {
        auto timeT = std::chrono::system_clock::to_time_t(entry.timestamp);
        QDateTime dt = QDateTime::fromSecsSinceEpoch(timeT);
        QDate eventDate = dt.date();

        // Skip events outside our range
        if (eventDate < startDate || eventDate > today) {
            continue;
        }

        // Find matching day and categorize
        for (auto& day : m_dayData) {
            if (day.date == eventDate) {
                day.events.push_back(entry);

                // Categorize by severity
                if (entry.level == L"Critical" || entry.level == L"Error") {
                    day.criticalCount++;
                } else if (entry.level == L"Warning") {
                    day.warningCount++;
                } else {
                    day.infoCount++;
                }
                break;
            }
        }
    }
}

// ============================================================================
// Smart Scaling
// ============================================================================

int EventTimelineWidget::calculateSmartMaxScale() const
{
    // Find actual max
    int actualMax = 0;
    for (const auto& day : m_dayData) {
        actualMax = std::max(actualMax, day.totalCount());
    }

    if (actualMax == 0) return 10;  // Empty state

    // Smart scaling: round up to nice numbers
    // Prevents one extreme day from making others invisible
    
    if (actualMax <= 5) return 5;
    if (actualMax <= 10) return 10;
    if (actualMax <= 20) return 20;
    if (actualMax <= 30) return 30;
    if (actualMax <= 50) return 50;
    if (actualMax <= 100) return 100;
    
    // For very high counts, cap at 2x the second-highest to prevent single spike distortion
    std::vector<int> counts;
    for (const auto& day : m_dayData) {
        if (day.totalCount() > 0) {
            counts.push_back(day.totalCount());
        }
    }
    
    if (counts.size() >= 2) {
        std::sort(counts.begin(), counts.end(), std::greater<int>());
        int secondMax = counts[1];
        int cappedMax = std::min(actualMax, secondMax * 3);  // Cap at 3x second highest
        
        // Round to nice number
        if (cappedMax <= 150) return 150;
        if (cappedMax <= 200) return 200;
        return 250;
    }
    
    return actualMax;
}

// ============================================================================
// Drawing
// ============================================================================

void EventTimelineWidget::paintEvent(QPaintEvent* event)
{
    QFrame::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int maxCount = calculateSmartMaxScale();

    drawBackground(painter);
    drawGridLines(painter, maxCount);
    drawBars(painter, maxCount);
    drawXAxisLabels(painter);
}

void EventTimelineWidget::drawBackground(QPainter& painter)
{
    // Already drawn by stylesheet
}

void EventTimelineWidget::drawGridLines(QPainter& painter, int maxCount)
{
    int chartHeight = height() - PADDING_TOP - PADDING_BOTTOM;
    int chartY = PADDING_TOP;

    painter.setPen(QPen(QColor(AppTheme::colors().borderSubtle), 1, Qt::DotLine));
    
    // Draw horizontal grid lines at useful integer values
    QFont labelFont = painter.font();
    labelFont.setPointSize(8);
    painter.setFont(labelFont);

    // Determine which values to show based on maxCount
    std::vector<int> values;
    if (maxCount <= 5) {
        // Show every integer: 0, 1, 2, 3, 4, 5
        for (int v = 0; v <= maxCount; ++v) {
            values.push_back(v);
        }
    } else if (maxCount <= 10) {
        // Show every 2: 0, 2, 4, 6, 8, 10
        for (int v = 0; v <= maxCount; v += 2) {
            values.push_back(v);
        }
    } else {
        // Show 5 evenly spaced values using integer division
        for (int i = 0; i <= 4; ++i) {
            int value = (maxCount * (4 - i)) / 4;
            values.push_back(value);
        }
    }

    // Draw grid lines at positions that match bar heights
    for (int value : values) {
        // Calculate Y position based on value (bars are drawn bottom-up)
        // A bar with height `value` reaches: chartY + chartHeight - (value * chartHeight / maxCount)
        int y = chartY + chartHeight - (value * chartHeight / maxCount);
        
        // Draw dotted line
        painter.setPen(QPen(QColor(AppTheme::colors().borderSubtle), 1, Qt::DotLine));
        painter.drawLine(PADDING_LEFT, y, width() - PADDING_RIGHT, y);
        
        // Draw Y-axis number on left
        painter.setPen(AppTheme::colors().textMuted);
        QRect labelRect(2, y - 10, PADDING_LEFT - 6, 20);
        painter.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, QString::number(value));
    }
}

void EventTimelineWidget::drawBars(QPainter& painter, int maxCount)
{
    if (m_dayData.empty() || maxCount == 0) return;

    int chartWidth = width() - PADDING_LEFT - PADDING_RIGHT;
    int chartHeight = height() - PADDING_TOP - PADDING_BOTTOM;
    int chartY = PADDING_TOP;

    int totalBars = static_cast<int>(m_dayData.size());
    int barWidth = (chartWidth - (totalBars - 1) * BAR_SPACING) / totalBars;
    barWidth = std::max(barWidth, 4);  // Minimum 4px per bar

    for (int i = 0; i < m_dayData.size(); ++i) {
        const auto& day = m_dayData[i];
        int x = PADDING_LEFT + i * (barWidth + BAR_SPACING);

        if (day.totalCount() == 0) {
            // Empty day - show subtle baseline
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(AppTheme::colors().borderSubtle));
            QRect baseRect(x, chartY + chartHeight - 2, barWidth, 2);
            painter.drawRect(baseRect);
            continue;
        }

        // Calculate segment heights (proportional to maxCount)
        double scale = double(chartHeight) / maxCount;
        int criticalHeight = qRound(day.criticalCount * scale);
        int warningHeight = qRound(day.warningCount * scale);
        int infoHeight = qRound(day.infoCount * scale);

        int totalBarHeight = criticalHeight + warningHeight + infoHeight;
        int currentY = chartY + chartHeight - totalBarHeight;

        // Draw stacked segments (bottom to top: critical, warning, info)
        painter.setPen(Qt::NoPen);

        // Critical (red) - bottom
        if (criticalHeight > 0) {
            QRect rect(x, currentY + warningHeight + infoHeight, barWidth, criticalHeight);
            
            // Only round top corners if this is the top segment
            if (warningHeight == 0 && infoHeight == 0) {
                QPainterPath path;
                path.addRoundedRect(rect, 0, 0);
                path.setFillRule(Qt::WindingFill);
                
                // Round top corners only
                QPainterPath topCorners;
                topCorners.addRoundedRect(
                    QRect(x, currentY + warningHeight + infoHeight, barWidth, BAR_TOP_RADIUS * 2),
                    BAR_TOP_RADIUS, BAR_TOP_RADIUS
                );
                
                painter.setBrush(QColor("#e74c3c"));
                painter.drawPath(path.united(topCorners));
            } else {
                painter.setBrush(QColor("#e74c3c"));
                painter.drawRect(rect);
            }
        }

        // Warning (orange) - middle
        if (warningHeight > 0) {
            QRect rect(x, currentY + infoHeight, barWidth, warningHeight);
            
            if (infoHeight == 0) {
                // Top segment - round top corners
                QPainterPath path;
                path.addRoundedRect(rect, BAR_TOP_RADIUS, BAR_TOP_RADIUS);
                painter.setBrush(QColor("#f39c12"));
                painter.drawPath(path);
            } else {
                painter.setBrush(QColor("#f39c12"));
                painter.drawRect(rect);
            }
        }

        // Info (blue) - top
        if (infoHeight > 0) {
            QRect rect(x, currentY, barWidth, infoHeight);
            
            // Always top segment - round top corners
            QPainterPath path;
            path.addRoundedRect(rect, BAR_TOP_RADIUS, BAR_TOP_RADIUS);
            painter.setBrush(QColor("#5dade2"));
            painter.drawPath(path);
        }

        // Hover highlight
        if (i == m_hoveredBarIndex) {
            QRect hoverRect(x, chartY, barWidth, chartHeight);
            painter.setBrush(QColor(255, 255, 255, 15));
            painter.drawRect(hoverRect);
        }
    }
}

void EventTimelineWidget::drawXAxisLabels(QPainter& painter)
{
    if (m_dayData.empty()) return;

    int chartWidth = width() - PADDING_LEFT - PADDING_RIGHT;
    int chartHeight = height() - PADDING_TOP - PADDING_BOTTOM;
    int labelY = PADDING_TOP + chartHeight + 8;

    painter.setPen(AppTheme::colors().textMuted);
    QFont labelFont = painter.font();
    labelFont.setPointSize(8);
    painter.setFont(labelFont);

    int totalBars = static_cast<int>(m_dayData.size());
    int barWidth = (chartWidth - (totalBars - 1) * BAR_SPACING) / totalBars;

    // Show week markers (every 7 days) but ensure first label doesn't get cut off
    for (int i = 0; i < m_dayData.size(); i += 7) {
        const auto& day = m_dayData[i];
        
        int x = PADDING_LEFT + i * (barWidth + BAR_SPACING);

        QString label = day.date.toString("MMM d");
        
        // For first label, align left to prevent cutoff
        // For others, center on bar
        QRect textRect;
        if (i == 0) {
            textRect = QRect(x, labelY, 80, 20);
            painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, label);
        } else {
            textRect = QRect(x - 40, labelY, 80, 20);
            painter.drawText(textRect, Qt::AlignCenter | Qt::AlignVCenter, label);
        }
    }

    // Always show "Today" label at the end
    if (!m_dayData.empty()) {
        const auto& lastDay = m_dayData.back();
        if (lastDay.date == QDate::currentDate()) {
            int x = PADDING_LEFT + (totalBars - 1) * (barWidth + BAR_SPACING);

            painter.setPen(AppTheme::colors().accent);
            QFont todayFont = labelFont;
            todayFont.setBold(true);
            painter.setFont(todayFont);
            
            QRect textRect(x - 30, labelY, 80, 20);
            painter.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, "Today");
        }
    }
}

// ============================================================================
// Interaction
// ============================================================================

void EventTimelineWidget::mouseMoveEvent(QMouseEvent* event)
{
    int index = barIndexAtPosition(event->pos());

    if (index != m_hoveredBarIndex) {
        m_hoveredBarIndex = index;
        update();

        if (index >= 0 && index < m_dayData.size()) {
            showTooltip(event->pos(), m_dayData[index]);
        } else {
            hideTooltip();
        }
    }

    QFrame::mouseMoveEvent(event);
}

void EventTimelineWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        int index = barIndexAtPosition(event->pos());
        if (index >= 0 && index < m_dayData.size()) {
            const auto& day = m_dayData[index];
            if (day.totalCount() > 0) {
                // Hide tooltip first
                hideTooltip();
                
                // Get bar position to show popup next to it
                QRect barArea = barRect(index, calculateSmartMaxScale());
                QPoint barCenter = QPoint(barArea.center().x(), barArea.top());
                showDayEventsPopup(day, barCenter);
                
                // Accept event to prevent propagation
                event->accept();
                return;
            }
        }
    }

    QFrame::mousePressEvent(event);
}

void EventTimelineWidget::leaveEvent(QEvent* event)
{
    m_hoveredBarIndex = -1;
    hideTooltip();
    update();
    QFrame::leaveEvent(event);
}

int EventTimelineWidget::barIndexAtPosition(const QPoint& pos) const
{
    if (m_dayData.empty()) return -1;

    int chartWidth = width() - PADDING_LEFT - PADDING_RIGHT;
    int chartHeight = height() - PADDING_TOP - PADDING_BOTTOM;
    int chartY = PADDING_TOP;
    
    // Check if click is within chart area vertically
    if (pos.y() < chartY || pos.y() > chartY + chartHeight) {
        return -1;
    }

    int totalBars = static_cast<int>(m_dayData.size());
    int barWidth = (chartWidth - (totalBars - 1) * BAR_SPACING) / totalBars;

    int relativeX = pos.x() - PADDING_LEFT;
    if (relativeX < 0) return -1;

    // Calculate which bar column we're in (including spacing)
    int columnWidth = barWidth + BAR_SPACING;
    int index = relativeX / columnWidth;
    
    if (index >= totalBars) return -1;

    // Accept clicks anywhere in the bar column (bar + spacing)
    // This makes it easier to click, especially on narrow bars
    return index;
}

QRect EventTimelineWidget::barRect(int index, int maxCount) const
{
    if (index < 0 || index >= m_dayData.size()) {
        return QRect();
    }

    int chartWidth = width() - PADDING_LEFT - PADDING_RIGHT;
    int chartHeight = height() - PADDING_TOP - PADDING_BOTTOM;
    int totalBars = static_cast<int>(m_dayData.size());
    int barWidth = (chartWidth - (totalBars - 1) * BAR_SPACING) / totalBars;

    int x = PADDING_LEFT + index * (barWidth + BAR_SPACING);
    int y = PADDING_TOP;

    return QRect(x, y, barWidth, chartHeight);
}

void EventTimelineWidget::showTooltip(const QPoint& pos, const DayData& day)
{
    if (day.totalCount() == 0) {
        hideTooltip();
        return;
    }

    // Build simple HTML content (background is in QLabel stylesheet)
    QString html = QString(
        "<div style='font-size: 11px;'>"
        "<div style='font-weight: bold; font-size: 12px; margin-bottom: 8px; color: %1;'>%2</div>"
    ).arg(
        AppTheme::colors().textPrimary,
        day.date.toString("MMMM d, yyyy")
    );

    // Add counts with colored rectangle badges
    if (day.criticalCount > 0) {
        html += QString(
            "<div style='margin: 4px 0;'>"
            "<span style='display: inline-block; width: 12px; height: 12px; "
            "background-color: #e74c3c; border-radius: 3px; margin-right: 8px; vertical-align: middle;'></span>"
            "<span style='color: %1; vertical-align: middle;'>%2 Critical</span>"
            "</div>"
        ).arg(AppTheme::colors().textPrimary).arg(day.criticalCount);
    }
    if (day.warningCount > 0) {
        html += QString(
            "<div style='margin: 4px 0;'>"
            "<span style='display: inline-block; width: 12px; height: 12px; "
            "background-color: #f39c12; border-radius: 3px; margin-right: 8px; vertical-align: middle;'></span>"
            "<span style='color: %1; vertical-align: middle;'>%2 Warning%3</span>"
            "</div>"
        ).arg(AppTheme::colors().textPrimary)
         .arg(day.warningCount)
         .arg(day.warningCount > 1 ? "s" : "");
    }
    if (day.infoCount > 0) {
        html += QString(
            "<div style='margin: 4px 0;'>"
            "<span style='display: inline-block; width: 12px; height: 12px; "
            "background-color: #5dade2; border-radius: 3px; margin-right: 8px; vertical-align: middle;'></span>"
            "<span style='color: %1; vertical-align: middle;'>%2 Info</span>"
            "</div>"
        ).arg(AppTheme::colors().textPrimary).arg(day.infoCount);
    }

    // Hint text at bottom
    html += QString(
        "<div style='margin-top: 10px; padding-top: 8px; border-top: 1px solid %1; "
        "color: %2; font-size: 10px; font-style: italic;'>Click bar to view details</div>"
    ).arg(AppTheme::colors().borderSubtle, AppTheme::colors().textMuted);

    html += "</div>";

    m_tooltipLabel->setText(html);
    m_tooltipLabel->adjustSize();

    // Position above cursor
    QPoint globalPos = mapToGlobal(pos);
    m_tooltipLabel->move(globalPos.x() - m_tooltipLabel->width() / 2, 
                         globalPos.y() - m_tooltipLabel->height() - 10);
    m_tooltipLabel->show();
    m_tooltipLabel->raise();
}

void EventTimelineWidget::hideTooltip()
{
    if (m_tooltipLabel) {
        m_tooltipLabel->hide();
    }
}

void EventTimelineWidget::showDayEventsPopup(const DayData& day, const QPoint& barCenter)
{
    // Create popup
    DayEventsPopup* popup = new DayEventsPopup(day.date, day.events, nullptr);
    
    // Convert bar center to global coordinates
    QPoint globalBarCenter = mapToGlobal(barCenter);
    
    // Get screen geometry
    QScreen* screen = QApplication::screenAt(globalBarCenter);
    QRect screenGeometry = screen ? screen->geometry() : QRect();
    
    // Calculate popup height based on content
    int contentHeight = popup->sizeHint().height();
    int popupHeight = std::min(contentHeight, 420);
    popup->setFixedHeight(popupHeight);
    
    // Position popup to the right of the bar, or left if not enough space
    int popupX = globalBarCenter.x() + 20;  // 20px to the right of bar center
    int popupY = globalBarCenter.y() - 50;  // Slightly above bar center
    
    // Check if popup fits on the right
    if (popupX + popup->width() > screenGeometry.right()) {
        // Position on the left instead
        popupX = globalBarCenter.x() - popup->width() - 20;
    }
    
    // Ensure popup is on screen vertically
    if (popupY + popupHeight > screenGeometry.bottom()) {
        popupY = screenGeometry.bottom() - popupHeight - 20;
    }
    if (popupY < screenGeometry.top()) {
        popupY = screenGeometry.top() + 20;
    }
    
    popup->move(popupX, popupY);
    popup->show();
    popup->raise();
}