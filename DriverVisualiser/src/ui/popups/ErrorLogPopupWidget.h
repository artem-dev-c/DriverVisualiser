#pragma once

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <vector>
#include "ErrorLogEntry.h"

/**
 * @class ErrorLogPopupWidget
 * @brief Displays detailed error log entries in a popup window.
 *
 * Similar to DriverInfoPopup - shows list of events with timestamps,
 * levels, and messages. Popup style with rounded borders.
 */
class ErrorLogPopupWidget : public QFrame
{
    Q_OBJECT

public:
    explicit ErrorLogPopupWidget(const std::vector<ErrorLogEntry>& entries,
                               int logDays = 7,
                               QWidget* parent = nullptr);
    
    /// Show popup positioned relative to the anchor widget
    void showRelativeTo(QWidget* anchor);
    
    /// Check if popup was closed very recently (prevents reopen on same click)
    bool wasRecentlyClosed() const;

protected:
    void focusOutEvent(QFocusEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool event(QEvent* event) override;

private:
    void setupUi();
    
    /// Create section header
    QLabel* createSectionHeader(const QString& title);
    
    /// Create a single error log row
    QWidget* createLogRow(const ErrorLogEntry& entry);
    
    /// Format timestamp as readable string
    static QString formatTimestamp(const std::chrono::system_clock::time_point& timestamp);
    
    /// Get color for severity level
    static QString getSeverityColor(const std::wstring& level);

    std::vector<ErrorLogEntry> m_entries;
    int m_logDays = 7;
    qint64 m_closeTime = 0;
};