#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <vector>
#include "SystemHealthSummary.h"

/**
 * @class IssueListWidget
 * @brief Scrollable list of actionable driver issues for the dashboard.
 *
 * Displays only real, actionable issues (blocked drivers, failed to start,
 * unsigned drivers, needs restart, device disconnected).
 * User-disabled devices are shown separately at the bottom, greyed out.
 *
 * Each row contains:
 *   [severity dot] [driver name]  [description]  [category badge]
 */
class IssueListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit IssueListWidget(QWidget* parent = nullptr);

    /// Populate the list with computed issues
    void setIssues(const std::vector<DashboardIssue>& issues);

    /// Clear all issue rows
    void clearIssues();

private:
    /// Build a single issue row widget
    QWidget* createIssueRow(const DashboardIssue& issue);

    /// Build the "no issues" placeholder widget
    QWidget* createNoIssuesRow();

    /// Returns color hex string for severity
    QString severityColor(HealthFlagSeverity severity) const;

    QVBoxLayout* m_listLayout = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QWidget*     m_listContainer = nullptr;
};