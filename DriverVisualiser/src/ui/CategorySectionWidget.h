#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFrame>
#include "CategoryProcessor.h"

class DriverCardWidget;

/// UI widget for displaying a category section
/// Accepts ProcessedCategory from business logic layer
class CategorySectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CategorySectionWidget(const ProcessedCategory& category,
                                   QWidget* parent = nullptr);
    
    void setExpanded(bool expanded);
    bool isExpanded() const;

private slots:
    void toggleExpanded();

private:
    void setupUi();
    void populateDrivers();
    void updateToggleButton();
    QString getHealthIndicatorText() const;
    QString getHealthIndicatorIcon() const;
    QColor getHealthIndicatorColor() const;
    QString getStatusText() const;  // New: "X drivers • No issues"

    QPushButton* m_toggleButton;
    QLabel* m_titleLabel;
    QLabel* m_healthIndicator;
    QWidget* m_contentWidget;
    QVBoxLayout* m_contentLayout;
    bool m_expanded;
    ProcessedCategory m_category;  // Holds pre-processed data
};