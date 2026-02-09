#pragma once

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include "DriverInfo.h"

/**
 * @class DriverInfoPopup
 * @brief Displays technical driver information in a popup window.
 *
 * Shows raw data collected from Windows APIs, allowing users to
 * copy specific fields or all data for troubleshooting purposes.
 */
class DriverInfoPopup : public QFrame
{
    Q_OBJECT

public:
    explicit DriverInfoPopup(const DriverInfo& driver, QWidget* parent = nullptr);
    
    /// Show popup positioned relative to the anchor widget
    void showRelativeTo(QWidget* anchor);
    
    /// Check if popup was closed very recently (prevents reopen on same click)
    bool wasRecentlyClosed() const;

protected:
    void focusOutEvent(QFocusEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool event(QEvent* event) override;

private:
    void setupUi(const DriverInfo& driver);
    
    /// Create section header
    QLabel* createSectionHeader(const QString& title);
    
    /// Create a simple text field row
    QWidget* createFieldRow(const QString& label, const QString& value);
    
    /// Create a copyable field row (with rounded container + copy button)
    QWidget* createCopyableFieldRow(const QString& label, const QString& value);
    
    /// Copy text to clipboard and show visual feedback
    void copyToClipboard(const QString& text, QPushButton* button);
    
    /// Generate full text for "Copy All" button
    QString generateFullReport(const DriverInfo& driver);
    
    qint64 m_closeTime = 0;
    DriverInfo m_driver;
};