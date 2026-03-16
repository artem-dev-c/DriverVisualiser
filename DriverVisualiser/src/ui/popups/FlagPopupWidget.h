#pragma once

#include <QFrame>
#include <QVBoxLayout>
#include <vector>
#include "HealthFlag.h"

class FlagPopupWidget : public QFrame
{
    Q_OBJECT

public:
    explicit FlagPopupWidget(const std::vector<HealthFlag>& sortedFlags, QWidget* parent = nullptr);
    
    /// Show popup positioned relative to the indicator widget
    void showRelativeTo(QWidget* anchor);
    
    /// Check if popup was closed very recently (prevents reopen on same click)
    bool wasRecentlyClosed() const;

protected:
    void focusOutEvent(QFocusEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool event(QEvent* event) override;

private:
    void setupUi(const std::vector<HealthFlag>& sortedFlags);
    
    /// Create a single flag row
    QFrame* createFlagRow(const HealthFlag& flag);
    
    /// Get border color for severity
    static QString getOutlineColor(HealthFlagSeverity severity);
    
    qint64 m_closeTime = 0;
};