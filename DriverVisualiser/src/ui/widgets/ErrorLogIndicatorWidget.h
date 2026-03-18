#pragma once

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <vector>
#include "ErrorLogEntry.h"

class ErrorLogPopupWidget;

/// ============================================================================
/// ErrorLogIndicatorWidget
/// ============================================================================
/// Compact pill summarising event log entries for a driver.
/// Clicking opens ErrorLogPopupWidget with the full entry list.
///
/// Visual design (updated):
///   - border-radius: 8px (consistent with FlagIndicatorWidget and card)
///   - Fixed width to align with FlagIndicatorWidget across all cards
///   - "No logs" state is subtle/transparent — not green (green implies healthy,
///     absence of logs is just neutral)
///   - Severity coloring mirrors FlagIndicatorWidget palette exactly
/// ============================================================================
class ErrorLogIndicatorWidget : public QFrame
{
    Q_OBJECT

public:
    explicit ErrorLogIndicatorWidget(const std::vector<ErrorLogEntry>& entries,
                                     int logDays = 7,
                                     QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    void setupUi();
    void updateAppearance();

    int countCritical() const;
    int countErrors()   const;
    int countWarnings() const;
    int countInformation() const;

    static QString outlineColor(int errorCount);
    static QString bgColor(int errorCount);

    std::vector<ErrorLogEntry> m_entries;
    int     m_logDays  = 7;
    QLabel* m_textLabel  = nullptr;
    QPushButton* m_arrowButton = nullptr;  // Changed from QLabel to QPushButton for SVG icon
    ErrorLogPopupWidget* m_popup = nullptr;
    bool    m_hasLogs = false;
};