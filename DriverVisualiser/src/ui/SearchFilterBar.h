#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QTimer>
#include "FilterPopupWidget.h"

/**
 * @class SearchFilterBar
 * @brief Top bar with search field and filter button on the same row
 * 
 * Semi-transparent search bar with placeholder text.
 * Filter button opens FilterPopupWidget.
 */
class SearchFilterBar : public QWidget
{
    Q_OBJECT

public:
    explicit SearchFilterBar(QWidget* parent = nullptr);
    
    /// Get current search text
    QString getSearchText() const;
    
    /// Get current filter state
    FilterPopupWidget::FilterState getFilterState() const;
    
    /// Set available manufacturers for filter popup
    void setManufacturers(const std::set<std::wstring>& manufacturers);

signals:
    /// Emitted when search text changes (after 300ms debounce)
    void searchChanged(const QString& text);
    
    /// Emitted when filters change
    void filtersChanged();

private slots:
    void onSearchTextChanged();
    void onFilterButtonClicked();

private:
    void setupUi();
    
    QLineEdit* m_searchBox;
    QPushButton* m_filterButton;
    FilterPopupWidget* m_filterPopup;
    QTimer* m_searchDebounceTimer;
    
    std::set<std::wstring> m_manufacturers;
};
