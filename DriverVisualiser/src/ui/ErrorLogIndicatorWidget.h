#pragma once

#include <QFrame>
#include <QLabel>
#include <vector>
#include "ErrorLogEntry.h"

class ErrorLogPopupWidget;

/**
 * @class ErrorLogIndicatorWidget
 * @brief Displays error log summary with expandable popup.
 *
 * Similar to FlagIndicatorWidget - shows a compact summary that expands
 * to show full error log entries when clicked.
 */
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
    
    /// Get color for error count severity
    static QString getOutlineColor(int errorCount);
    static QString getBackgroundColor(int errorCount);
    
    /// Count errors by level
    int countErrors() const;
    int countWarnings() const;
    int countCritical() const;

    std::vector<ErrorLogEntry> m_entries;
    int m_logDays;
    QLabel* m_textLabel;
    QLabel* m_arrowLabel;
    ErrorLogPopupWidget* m_popup;
    bool m_hasLogs;
};