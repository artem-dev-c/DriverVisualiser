#pragma once

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <vector>
#include "HealthFlag.h"

class FlagPopupWidget;

class FlagIndicatorWidget : public QFrame
{
    Q_OBJECT

public:
    explicit FlagIndicatorWidget(const std::vector<HealthFlag>& flags, QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    void setupUi();
    void updateAppearance();
    
    /// Get color for severity level
    static QString getOutlineColor(HealthFlagSeverity severity);
    static QString getBackgroundColor(HealthFlagSeverity severity);
    
    /// Find the most critical flag (for display)
    const HealthFlag* getMostCriticalFlag() const;
    
    /// Sort flags by severity (Critical first)
    static std::vector<HealthFlag> sortFlagsBySeverity(const std::vector<HealthFlag>& flags);

    std::vector<HealthFlag> m_flags;
    QLabel* m_textLabel;
    QLabel* m_arrowLabel;
    FlagPopupWidget* m_popup;
    bool m_hasIssues;
};
