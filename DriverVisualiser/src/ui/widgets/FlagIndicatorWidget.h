#pragma once

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <vector>
#include "HealthFlag.h"

class FlagPopupWidget;

/// ============================================================================
/// FlagIndicatorWidget
/// ============================================================================
/// Compact pill showing the most critical health flag for a driver.
/// Clicking opens FlagPopupWidget with the full flag list.
///
/// Visual design (updated):
///   - border-radius: 8px (consistent with card's 10px rounding)
///   - 2px colored border + subtle tinted background when issues exist
///   - Greyed-out "No issues" state with no border emphasis
///   - Fixed width so all cards align their right-side indicators
/// ============================================================================
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

    static QString outlineColor(HealthFlagSeverity severity);
    static QString backgroundColor(HealthFlagSeverity severity);

    const HealthFlag* mostCriticalFlag() const;
    static std::vector<HealthFlag> sortedBySeverity(const std::vector<HealthFlag>& flags);

    std::vector<HealthFlag> m_flags;
    QLabel*          m_textLabel  = nullptr;
    QLabel*          m_arrowLabel = nullptr;  // Visual indicator (not a button)
    FlagPopupWidget* m_popup      = nullptr;
    bool             m_hasIssues  = false;
};