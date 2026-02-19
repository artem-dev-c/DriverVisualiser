#pragma once

#include <QFrame>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <set>
#include <string>

/**
 * @class FilterPopupWidget
 * @brief Filter popup with checkboxes for Health, Importance, and Manufacturer
 * 
 * Styled like FlagPopupWidget/ErrorLogPopupWidget - dark theme, rounded corners.
 * Auto-closes on focus loss, ESC key.
 */
class FilterPopupWidget : public QFrame
{
    Q_OBJECT

public:
    struct FilterState {
        // Health filters (all unchecked by default)
        bool showCritical = false;
        bool showWarnings = false;
        bool showHealthy = false;
        
        // Importance filters (all unchecked by default)
        bool showCriticalDevices = false;
        bool showImportantDevices = false;
        bool showOptionalDevices = false;
        bool showVirtualDevices = false;
        
        // Manufacturer filters (empty = none selected)
        std::set<std::wstring> selectedManufacturers;
    };

    explicit FilterPopupWidget(const std::set<std::wstring>& allManufacturers, 
                               QWidget* parent = nullptr);
    
    /// Show popup positioned relative to the anchor widget
    void showRelativeTo(QWidget* anchor);
    
    /// Check if popup was closed very recently
    bool wasRecentlyClosed() const;
    
    /// Get current filter state
    FilterState getFilterState() const;
    
    /// Set filter state (for persistence)
    void setFilterState(const FilterState& state);

signals:
    /// Emitted when any filter changes (real-time)
    void filtersChanged();

protected:
    void focusOutEvent(QFocusEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool event(QEvent* event) override;

private:
    void setupUi(const std::set<std::wstring>& allManufacturers);
    QWidget* createSection(const QString& title);
    QCheckBox* createCheckbox(const QString& label, bool checked = true);
    
    // Health checkboxes
    QCheckBox* m_criticalCheck;
    QCheckBox* m_warningsCheck;
    QCheckBox* m_healthyCheck;
    
    // Importance checkboxes
    QCheckBox* m_criticalDevicesCheck;
    QCheckBox* m_importantDevicesCheck;
    QCheckBox* m_optionalDevicesCheck;
    QCheckBox* m_virtualDevicesCheck;
    
    // Manufacturer checkboxes (map: manufacturer name -> checkbox)
    std::map<std::wstring, QCheckBox*> m_manufacturerChecks;
    
    qint64 m_closeTime = 0;
};