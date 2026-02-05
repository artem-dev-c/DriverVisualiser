#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFrame>
#include <vector>
#include "DriverInfo.h"

class DriverCardWidget;

class CategorySectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CategorySectionWidget(const QString& categoryName, 
                                   int driverCount,
                                   QWidget* parent = nullptr);
    
    // Add a driver card to this category
    void addDriverCard(DriverCardWidget* card);
    
    // Expand or collapse the section
    void setExpanded(bool expanded);
    bool isExpanded() const;

private slots:
    void toggleExpanded();

private:
    void setupUi(const QString& categoryName, int driverCount);
    void updateToggleButton();

    QPushButton* m_toggleButton;
    QLabel* m_titleLabel;
    QWidget* m_contentWidget;
    QVBoxLayout* m_contentLayout;
    bool m_expanded;
};
