#include "CategorySectionWidget.h"
#include "DriverCardWidget.h"

#include <QHBoxLayout>

CategorySectionWidget::CategorySectionWidget(const QString& categoryName,
                                             int driverCount,
                                             QWidget* parent)
    : QWidget(parent)
    , m_expanded(false)
{
    setupUi(categoryName, driverCount);
}

void CategorySectionWidget::setupUi(const QString& categoryName, int driverCount)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // === Header row (clickable to expand/collapse) ===
    QWidget* headerWidget = new QWidget();
    headerWidget->setAutoFillBackground(true);
    
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(8, 6, 8, 6);
    headerLayout->setSpacing(8);

    // Toggle button (arrow indicator)
    m_toggleButton = new QPushButton("▶");
    m_toggleButton->setFixedSize(20, 20);
    m_toggleButton->setFlat(true);
    connect(m_toggleButton, &QPushButton::clicked, this, &CategorySectionWidget::toggleExpanded);
    headerLayout->addWidget(m_toggleButton);

    // Category title with count
    QString titleText = QString("%1 (%2)").arg(categoryName).arg(driverCount);
    m_titleLabel = new QLabel(titleText);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    headerLayout->addWidget(m_titleLabel);
    headerLayout->addStretch();

    mainLayout->addWidget(headerWidget);

    // === Content area (holds driver cards) ===
    m_contentWidget = new QWidget();
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(0, 4, 0, 4);
    m_contentLayout->setSpacing(4);

    // Initially collapsed
    m_contentWidget->setVisible(false);
    mainLayout->addWidget(m_contentWidget);
}

void CategorySectionWidget::addDriverCard(DriverCardWidget* card)
{
    m_contentLayout->addWidget(card);
}

void CategorySectionWidget::setExpanded(bool expanded)
{
    m_expanded = expanded;
    m_contentWidget->setVisible(expanded);
    updateToggleButton();
}

bool CategorySectionWidget::isExpanded() const
{
    return m_expanded;
}

void CategorySectionWidget::toggleExpanded()
{
    setExpanded(!m_expanded);
}

void CategorySectionWidget::updateToggleButton()
{
    m_toggleButton->setText(m_expanded ? "▼" : "▶");
}