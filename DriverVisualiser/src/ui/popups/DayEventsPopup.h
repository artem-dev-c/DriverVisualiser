#pragma once

#include <QFrame>
#include <QDate>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QTimer>
#include <vector>
#include "ErrorLogEntry.h"

/// Popup showing all events for a specific day, sorted by time
class DayEventsPopup : public QFrame
{
    Q_OBJECT

public:
    explicit DayEventsPopup(const QDate& date,
                            const std::vector<ErrorLogEntry>& events,
                            QWidget* parent = nullptr);

    /// Show popup relative to a widget (positions near it)
    void showRelativeTo(QWidget* anchor);

    /// Check if popup was recently closed (prevents immediate re-open)
    bool wasRecentlyClosed() const;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUi();
    void populateEvents();
    QWidget* createEventCard(const ErrorLogEntry& entry);
    QString formatTime(const ErrorLogEntry& entry) const;
    QString getSeverityColor(const std::wstring& level) const;
    QString getSeverityEmoji(const std::wstring& level) const;

    QDate m_date;
    std::vector<ErrorLogEntry> m_events;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_contentWidget = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;

    QTimer m_closeTimer;
    bool m_recentlyClosed = false;
};