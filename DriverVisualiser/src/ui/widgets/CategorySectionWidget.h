#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFrame>
#include "CategoryProcessor.h"
#include "FilterPopupWidget.h"

class DriverCardWidget;

/// UI widget for displaying a category section
/// Accepts ProcessedCategory from business logic layer
class CategorySectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CategorySectionWidget(const ProcessedCategory& category,
                                   int logDays = 7,
                                   QWidget* parent = nullptr);

    void setExpanded(bool expanded);
    bool isExpanded() const;

    /// Show/hide individual driver cards based on search text and filter state.
    /// Returns true if at least one driver card is visible (section should show).
    /// Pass empty searchText and default FilterState to show all.
    bool applyFilter(const QString& searchText,
                     const FilterPopupWidget::FilterState& filters);

private slots:
    void toggleExpanded();

private:
    void setupUi();
    void populateDrivers();
    void updateToggleButton();
    QString getHealthIndicatorText() const;
    QString getHealthIndicatorIcon() const;
    QColor getHealthIndicatorColor() const;
    QString getStatusText() const;

    QPushButton* m_toggleButton;
    QLabel* m_titleLabel;
    QLabel* m_healthIndicator;
    QWidget* m_contentWidget;
    QVBoxLayout* m_contentLayout;
    bool m_expanded;
    ProcessedCategory m_category;
    int m_logDays = 7;
};