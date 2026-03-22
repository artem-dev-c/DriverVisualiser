#pragma once

#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <set>
#include <string>
#include <map>

/**
 * @class FilterPopupWidget
 * @brief Redesigned filter popup with chip toggles, per-section clear,
 *        manufacturer search, and visual section separators.
 */
class FilterPopupWidget : public QFrame
{
    Q_OBJECT

public:
    struct FilterState {
        bool showCritical       = false;
        bool showWarnings       = false;
        bool showHealthy        = false;
        bool showCriticalDevices  = false;
        bool showImportantDevices = false;
        bool showOptionalDevices  = false;
        bool showVirtualDevices   = false;
        std::set<std::wstring> selectedManufacturers;
    };

    explicit FilterPopupWidget(const std::set<std::wstring>& allManufacturers,
                               QWidget* parent = nullptr);

    void showRelativeTo(QWidget* anchor);
    bool wasRecentlyClosed() const;

    FilterState getFilterState() const;
    void setFilterState(const FilterState& state);

    /// Returns number of active filter criteria (for badge on filter button)
    int activeFilterCount() const;

signals:
    void filtersChanged();

protected:
    void focusOutEvent(QFocusEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool event(QEvent* event) override;

private:
    void setupUi(const std::set<std::wstring>& allManufacturers);

    /// Section header row with title on left and "Clear" link on right
    QWidget* createSectionHeader(const QString& title, std::function<void()> onClear);

    /// Horizontal row of toggleable chip buttons
    QWidget* createChipRow(const QStringList& labels, std::vector<QPushButton*>& chips);

    /// Single chip toggle button
    QPushButton* createChip(const QString& label);
    void updateChipStyle(QPushButton* chip);

    /// Manufacturer checkbox (kept as checkbox for long lists)
    QCheckBox* createCheckbox(const QString& label, bool checked = false);

    void clearAllChips(std::vector<QPushButton*>& chips);
    void clearAllManufacturers();
    void onManufacturerSearchChanged(const QString& text);

    // Health chips
    std::vector<QPushButton*> m_healthChips;   // Critical, Warnings, Healthy

    // Importance chips
    std::vector<QPushButton*> m_importanceChips; // Critical Devices, Important, Optional, Virtual

    // Manufacturer checkboxes
    std::map<std::wstring, QCheckBox*> m_manufacturerChecks;
    QWidget* m_mfgContainer  = nullptr;
    QVBoxLayout* m_mfgLayout = nullptr;

    qint64 m_closeTime = 0;
};