#pragma once

#include <QFrame>
#include <QLabel>
#include <QPainter>
#include <QMouseEvent>
#include <QDate>
#include <vector>
#include "ErrorLogEntry.h"

class DayEventsPopup;

/// Event timeline graph widget - displays event history as stacked bar chart
class EventTimelineWidget : public QFrame
{
    Q_OBJECT

public:
    explicit EventTimelineWidget(QWidget* parent = nullptr);

    /// Set the events to display (automatically aggregates by day)
    void setEvents(const std::vector<ErrorLogEntry>& events);

    /// Set how many days to show (default: 30)
    void setDayRange(int days);

    /// Aggregated totals across all days — used by the external header row
    struct Totals {
        int critical = 0;
        int warnings = 0;
        int info     = 0;
        int swd      = 0;
        /// Meaningful events only — matches what the bar chart displays
        int total()  const { return critical + warnings + info; }
    };
    Totals getTotals() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct DayData {
        QDate date;
        int criticalCount = 0;  // Critical + Error
        int warningCount = 0;
        int infoCount = 0;      // Hardware info events
        int swdCount = 0;       // SWD\ software device info events (tooltip only)
        std::vector<ErrorLogEntry> events;

        /// Meaningful driver health events — used for bar height and scaling
        int totalCount() const {
            return criticalCount + warningCount + infoCount;
        }

        /// True if there is anything to show in the popup (includes SWD background events)
        bool hasAnyEvents() const {
            return criticalCount > 0 || warningCount > 0 || infoCount > 0 || swdCount > 0;
        }
    };

    void setupUi();
    void aggregateEventsByDay();
    void drawBackground(QPainter& painter);
    void drawGridLines(QPainter& painter, int maxCount);
    void drawBars(QPainter& painter, int maxCount);
    void drawXAxisLabels(QPainter& painter);
    void showTooltip(const QPoint& pos, const DayData& day);
    void hideTooltip();
    void showDayEventsPopup(const DayData& day, const QPoint& barCenter);

    int barIndexAtPosition(const QPoint& pos) const;
    QRect barRect(int index, int maxCount) const;
    int calculateSmartMaxScale() const;

    /// Compute bar width and spacing dynamically so bars are always wider than gaps.
    /// All drawing and hit-testing functions must use this to stay consistent.
    struct BarMetrics { int barWidth; int spacing; int columnWidth; };
    BarMetrics computeBarMetrics() const;

    std::vector<ErrorLogEntry> m_allEvents;
    std::vector<DayData> m_dayData;
    int m_dayRange = 30;
    int m_hoveredBarIndex = -1;

    // UI
    QLabel* m_tooltipLabel = nullptr;

    // Layout constants
    static constexpr int PADDING_LEFT = 35;  // Increased for Y-axis labels
    static constexpr int PADDING_RIGHT = 20;
    static constexpr int PADDING_TOP = 20;
    static constexpr int PADDING_BOTTOM = 40;  // Space for date labels
    static constexpr int BAR_SPACING = 4;
    static constexpr int BAR_TOP_RADIUS = 3;
};