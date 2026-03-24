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
#include <cmath>
#include <windows.h>

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
    OutputDebugStringA(QString("Timeline received %1 events\n").arg(events.size()).toStdString().c_str());
    // DEBUG: Log event count to verify we're getting the expected number of events from the main thread
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

EventTimelineWidget::Totals EventTimelineWidget::getTotals() const
{
    Totals t;
    for (const auto& day : m_dayData) {
        t.critical += day.criticalCount;
        t.warnings += day.warningCount;
        t.info     += day.infoCount;
        t.swd      += day.swdCount;
    }
    return t;
}

EventTimelineWidget::BarMetrics EventTimelineWidget::computeBarMetrics() const
{
    int totalBars  = static_cast<int>(m_dayData.size());
    if (totalBars == 0) return {4, 4, 8};

    int chartWidth = width() - PADDING_LEFT - PADDING_RIGHT;

    // Scale spacing down as bar count grows so bars always dominate over gaps.
    // At 30 bars: spacing=4. At 90 bars: spacing=2. At 180 bars: spacing=1.
    int spacing = std::max(1, BAR_SPACING - (totalBars > 90 ? 3 : totalBars > 45 ? 2 : 0));

    int barWidth = (chartWidth - (totalBars - 1) * spacing) / totalBars;
    barWidth = std::max(barWidth, 2);  // Minimum 2px per bar

    return { barWidth, spacing, barWidth + spacing };
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
                    // Information-level events: separate SWD from hardware
                    if (entry.category == ErrorLogEntry::Category::SwdBackground) {
                        day.swdCount++;
                    } else {
                        day.infoCount++;
                    }
                }
                break;
            }
        }
    }
    // DEBUG: Log aggregated counts to verify correctness of aggregation logic
    int totalCritical = 0, totalWarning = 0, totalInfo = 0, totalSwd = 0;
    for (const auto& day : m_dayData) {
        totalCritical += day.criticalCount;
        totalWarning += day.warningCount;
        totalInfo += day.infoCount;
        totalSwd += day.swdCount;
    }
    OutputDebugStringA(QString("Timeline aggregated: Critical=%1, Warning=%2, Info=%3, SWD=%4\n")
        .arg(totalCritical).arg(totalWarning).arg(totalInfo).arg(totalSwd).toStdString().c_str());
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

    QFont labelFont = painter.font();
    labelFont.setPointSize(8);
    painter.setFont(labelFont);

    // Choose evenly spaced Y pixel positions in log space so the visual gaps
    // between lines are equal regardless of the log scale used for bars.
    // We pick NUM_LINES evenly spaced fractions of chartHeight (0 = bottom, 1 = top),
    // then back-calculate the event count each fraction corresponds to so the
    // label shows an honest count aligned exactly with the bar height.
    static constexpr int NUM_LINES = 5;
    double maxLogValue = std::log1p(static_cast<double>(maxCount));

    for (int i = 0; i < NUM_LINES; ++i) {
        // fraction 0.0 = bottom of chart (0 events), 1.0 = top (maxCount events)
        double fraction = static_cast<double>(i) / (NUM_LINES - 1);

        // Pixel Y: fraction 0 -> chartY + chartHeight, fraction 1 -> chartY
        int y = chartY + chartHeight - qRound(fraction * chartHeight);

        // Back-calculate the event count this pixel height represents
        // inverse of: fraction = log1p(count) / log1p(maxCount)
        int labelValue = (fraction == 0.0)
            ? 0
            : qRound(std::expm1(fraction * maxLogValue));

        // Draw dotted line
        painter.setPen(QPen(QColor(AppTheme::colors().borderSubtle), 1, Qt::DotLine));
        painter.drawLine(PADDING_LEFT, y, width() - PADDING_RIGHT, y);

        // Draw Y-axis label
        painter.setPen(AppTheme::colors().textMuted);
        QRect labelRect(2, y - 10, PADDING_LEFT - 6, 20);
        painter.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, QString::number(labelValue));
    }
}

void EventTimelineWidget::drawBars(QPainter& painter, int maxCount)
{
    if (m_dayData.empty() || maxCount == 0) return;

    int chartWidth = width() - PADDING_LEFT - PADDING_RIGHT;
    int chartHeight = height() - PADDING_TOP - PADDING_BOTTOM;
    int chartY = PADDING_TOP;

    int totalBars = static_cast<int>(m_dayData.size());
    const auto bm = computeBarMetrics();
    const int barWidth = bm.barWidth;
    const int barSpacing = bm.spacing;

    for (int i = 0; i < m_dayData.size(); ++i) {
        const auto& day = m_dayData[i];
        int x = PADDING_LEFT + i * (barWidth + barSpacing);

        if (day.totalCount() == 0) {
            // No meaningful events — draw subtle baseline tick.
            // Day may still have SWD background events so remains hoverable/clickable.
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(day.swdCount > 0
                ? AppTheme::colors().borderNormal   // Slightly more visible if SWD activity exists
                : AppTheme::colors().borderSubtle));
            QRect baseRect(x, chartY + chartHeight - 2, barWidth, 2);
            painter.drawRect(baseRect);

            // Hover highlight even on empty bars so cursor feedback still works
            if (i == m_hoveredBarIndex && day.hasAnyEvents()) {
                painter.setBrush(QColor(255, 255, 255, 10));
                painter.drawRect(QRect(x, chartY, barWidth, chartHeight));
            }
            continue;
        }

        // Use logarithmic scaling for heights (log(1 + count))
        // This prevents 250 info events from completely overwhelming 3 warnings
        auto logScale = [](int count) -> double {
            return std::log1p(static_cast<double>(count));  // log(1 + x)
        };
        
        double maxLogValue = logScale(maxCount);
        double scale = chartHeight / maxLogValue;
        
        int criticalHeight = qRound(logScale(day.criticalCount) * scale);
        int warningHeight  = qRound(logScale(day.warningCount)  * scale);
        int infoHeight     = qRound(logScale(day.infoCount)     * scale);

        // Ensure minimum visible height for any non-zero count
        if (day.criticalCount > 0 && criticalHeight < 3) criticalHeight = 3;
        if (day.warningCount  > 0 && warningHeight  < 3) warningHeight  = 3;
        if (day.infoCount     > 0 && infoHeight     < 3) infoHeight     = 3;

        int totalBarHeight = criticalHeight + warningHeight + infoHeight;
        int currentY = chartY + chartHeight - totalBarHeight;

        // Draw stacked segments bottom to top: critical, warning, info
        painter.setPen(Qt::NoPen);

        // Critical (red) - bottom
        if (criticalHeight > 0) {
            QRect rect(x, currentY + warningHeight + infoHeight, barWidth, criticalHeight);

            if (warningHeight == 0 && infoHeight == 0) {
                QPainterPath path;
                path.addRoundedRect(rect, BAR_TOP_RADIUS, BAR_TOP_RADIUS);
                painter.setBrush(QColor("#e74c3c"));
                painter.drawPath(path);
            } else {
                painter.setBrush(QColor("#e74c3c"));
                painter.drawRect(rect);
            }
        }

        // Warning (orange) - middle
        if (warningHeight > 0) {
            QRect rect(x, currentY + infoHeight, barWidth, warningHeight);

            if (infoHeight == 0) {
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
    const auto bm = computeBarMetrics();
    const int barWidth = bm.barWidth;
    const int barSpacing = bm.spacing;

    // Show week markers (every 7 days) but ensure first label doesn't get cut off
    for (int i = 0; i < m_dayData.size(); i += 7) {
        const auto& day = m_dayData[i];
        
        int x = PADDING_LEFT + i * (barWidth + barSpacing);

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
            int x = PADDING_LEFT + (totalBars - 1) * (barWidth + barSpacing);

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
            if (day.hasAnyEvents()) {
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
    const auto bm = computeBarMetrics();
    const int barWidth = bm.barWidth;
    const int barSpacing = bm.spacing;

    int relativeX = pos.x() - PADDING_LEFT;
    if (relativeX < 0) return -1;

    // Calculate which bar column we're in (including spacing)
    int columnWidth = barWidth + barSpacing;
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
    const auto bm = computeBarMetrics();
    const int barWidth = bm.barWidth;
    const int barSpacing = bm.spacing;

    int x = PADDING_LEFT + index * (barWidth + barSpacing);
    int y = PADDING_TOP;

    return QRect(x, y, barWidth, chartHeight);
}

void EventTimelineWidget::showTooltip(const QPoint& pos, const DayData& day)
{
    if (!day.hasAnyEvents()) {
        hideTooltip();
        return;
    }

    // Build HTML content
    QString html = QString(
        "<div style='font-size: 11px;'>"
        "<div style='font-weight: bold; font-size: 12px; margin-bottom: 8px; color: %1;'>%2</div>"
    ).arg(
        AppTheme::colors().textPrimary,
        day.date.toString("MMMM d, yyyy")
    );

    // Meaningful event counts
    if (day.criticalCount > 0) {
        html += QString(
            "<div style='margin: 4px 0; display: flex; align-items: center;'>"
            "<span style='display: inline-block; background-color: rgba(231, 76, 60, 0.2); "
            "border-radius: 4px; padding: 4px 10px; margin-right: 8px;'></span>"
            "<span style='color: #e74c3c; font-weight: bold; font-size: 10px;'>%1 Critical</span>"
            "</div>"
        ).arg(day.criticalCount);
    }
    if (day.warningCount > 0) {
        html += QString(
            "<div style='margin: 4px 0; display: flex; align-items: center;'>"
            "<span style='display: inline-block; background-color: rgba(243, 156, 18, 0.2); "
            "border-radius: 4px; padding: 4px 10px; margin-right: 8px;'></span>"
            "<span style='color: #f39c12; font-weight: bold; font-size: 10px;'>%1 Warning%2</span>"
            "</div>"
        ).arg(day.warningCount)
         .arg(day.warningCount > 1 ? "s" : "");
    }
    if (day.infoCount > 0) {
        html += QString(
            "<div style='margin: 4px 0; display: flex; align-items: center;'>"
            "<span style='display: inline-block; background-color: rgba(93, 173, 226, 0.2); "
            "border-radius: 4px; padding: 4px 10px; margin-right: 8px;'></span>"
            "<span style='color: #5dade2; font-weight: bold; font-size: 10px;'>%1 Info</span>"
            "</div>"
        ).arg(day.infoCount);
    }

    // SWD background events — separated, muted, explained
    if (day.swdCount > 0) {
        // Add a subtle divider if there were also meaningful events above
        if (day.totalCount() > 0) {
            html += QString(
                "<div style='margin: 8px 0 4px 0; border-top: 1px solid %1;'></div>"
            ).arg(AppTheme::colors().borderSubtle);
        }
        html += QString(
            "<div style='margin: 4px 0; color: %1; font-size: 10px;'>"
            "+ %2 software background event%3 (SWD)"
            "</div>"
        ).arg(AppTheme::colors().textMuted)
         .arg(day.swdCount)
         .arg(day.swdCount > 1 ? "s" : "");
    }

    // Click hint
    html += QString(
        "<div style='margin-top: 10px; padding-top: 8px; border-top: 1px solid %1; "
        "color: %2; font-size: 10px; font-style: italic;'>Click to view details</div>"
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