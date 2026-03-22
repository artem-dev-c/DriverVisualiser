#pragma once

#include <QFrame>
#include <QDate>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QTimer>
#include <QEvent>
#include <vector>
#include <unordered_map>
#include "ErrorLogEntry.h"

/// Popup showing all events for a specific day in chronological order.
/// Hardware events are shown as cards with a dot-line thread connecting
/// consecutive events from the same device. SWD background events are
/// in a collapsed section at the bottom.
class DayEventsPopup : public QFrame
{
    Q_OBJECT

public:
    explicit DayEventsPopup(const QDate& date,
                            const std::vector<ErrorLogEntry>& events,
                            QWidget* parent = nullptr);

    void showRelativeTo(QWidget* anchor);
    bool wasRecentlyClosed() const;

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    // ── Chain position for thread drawing ────────────────────────────────────
    enum class ChainPos { None, First, Middle, Last };

    void buildChains();
    void setupUi();
    void populateEvents();
    void populateSwdSection();

    QWidget* createEventCard(int eventIndex);
    QWidget* createThreadMarker(ChainPos pos, const QString& color) const;
    QWidget* createSwdRow(const ErrorLogEntry& entry) const;
    QWidget* createStatsRow() const;

    QString formatTime(const ErrorLogEntry& entry) const;
    QString getSeverityColor(const std::wstring& level) const;
    QString getSeverityEmoji(const std::wstring& level) const;

    // ── Data ──────────────────────────────────────────────────────────────────
    QDate                      m_date;
    std::vector<ErrorLogEntry> m_events;     ///< Hardware events (chronological)
    std::vector<ErrorLogEntry> m_swdEvents;  ///< SWD background events

    /// Per-event chain metadata (parallel to m_events)
    std::vector<ChainPos> m_chainPos;
    std::vector<QString>  m_chainColors;

    // ── Widgets ───────────────────────────────────────────────────────────────
    QScrollArea* m_scrollArea    = nullptr;
    QWidget*     m_contentWidget = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;
    QWidget*     m_swdContent    = nullptr;

    QTimer m_closeTimer;
    bool   m_recentlyClosed = false;
};