#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <vector>
#include "DriverInfo.h"
#include "SystemHealthSummary.h"
#include "SystemInfo.h"

class ScoreRingWidget;
class IssueListWidget;

/**
 * @class DashboardWidget
 * @brief Top-level dashboard panel displayed above the driver category list.
 *
 * Layout (left to right):
 *   [ScoreRingWidget] | [Stats + Issues] | [System Info + Controls]
 *
 * Controls:
 *   - Manual Rescan button  -> emits scanRequested()
 *   - Log window toggle (7 / 30 / 90 days) -> emits logWindowChanged(int days)
 *     (changes take effect on next rescan, not immediately)
 *
 * The dashboard is populated via populate() after scanning is complete.
 * It does not own or trigger any scanning itself.
 */
class DashboardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardWidget(QWidget* parent = nullptr);

    /// Populate dashboard from scanned data (called after scan completes)
    void populate(const std::vector<DriverInfo>& drivers, const SystemInfo& sysInfo);

    /// Returns the currently selected log window in days (7, 30, or 90)
    int selectedLogDays() const;

    /// Restore the log window selection (called before populate() on rescan)
    void setSelectedLogDays(int days);

    /// Enable/disable the scan button (disabled while scanning)
    void setScanButtonEnabled(bool enabled);

signals:
    /// Emitted when user clicks the Rescan button
    void scanRequested();

    /// Emitted when user changes the log window toggle
    /// NOTE: Changes take effect on next rescan - not applied immediately
    void logWindowChanged(int days);

    /// Emitted when user clicks the Generate Report button
    void reportRequested();

private:
    void setupUi();

    /// Build the left column: score ring + counts
    QWidget* buildScoreColumn();

    /// Build the center column: issue list
    QWidget* buildIssueColumn();

    /// Build the right column: system info + controls
    QWidget* buildControlColumn();

    /// Update the driver count labels after populate()
    void updateCountLabels(const SystemHealthResult& result);

    /// Update the system info labels after populate()
    void updateSystemInfoLabels(const SystemInfo& sysInfo);

    /// Style the active/inactive log window buttons
    void updateLogWindowButtons();

    // === Score column ===
    ScoreRingWidget* m_scoreRing        = nullptr;
    QLabel*          m_criticalCountLabel = nullptr;
    QLabel*          m_warningCountLabel  = nullptr;
    QLabel*          m_healthyCountLabel  = nullptr;

    // === Issue column ===
    IssueListWidget* m_issueList = nullptr;

    // === Control column ===
    QLabel*      m_osLabel        = nullptr;
    QLabel*      m_buildLabel     = nullptr;
    QLabel*      m_archLabel      = nullptr;
    QLabel*      m_lastScanLabel  = nullptr;
    QPushButton* m_rescanButton   = nullptr;
    QPushButton* m_reportButton   = nullptr;

    // Log window toggle buttons (7 / 30 / 90)
    QPushButton* m_log7Button  = nullptr;
    QPushButton* m_log30Button = nullptr;
    QPushButton* m_log90Button = nullptr;

    int m_selectedLogDays = 7;  ///< Currently selected log window (default: 7 days)
};