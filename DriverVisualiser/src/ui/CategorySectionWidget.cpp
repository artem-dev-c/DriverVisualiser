#include "CategorySectionWidget.h"
#include "DriverCardWidget.h"
#include <QHBoxLayout>

CategorySectionWidget::CategorySectionWidget(const ProcessedCategory& category,
                                             QWidget* parent)
    : QWidget(parent)
    , m_expanded(false)
    , m_category(category)
{
    setupUi();
    populateDrivers();
    
    // Auto-expand sections with issues
    if (m_category.healthInfo.hasIssues) {
        setExpanded(true);
    }
}

void CategorySectionWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // === Header - Match Driver Cards ===
    QWidget* headerWidget = new QWidget();
    headerWidget->setAutoFillBackground(true);
    
    // Match driver cards - 16px rounded corners
    QString headerStyle = 
        "QWidget { "
        "   background-color: #2b2b2b; "
        "   border-radius: 16px; "        
        "   border: 1px solid #3d3d3d; "
        "}"
        "QWidget:hover { "
        "   background-color: #353535; "
        "}";
    
    headerWidget->setStyleSheet(headerStyle);
    headerWidget->setCursor(Qt::PointingHandCursor);
    
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(16, 14, 16, 14);
    headerLayout->setSpacing(12);

    // Bigger ASCII arrows (18px font)
    m_toggleButton = new QPushButton(m_expanded ? "⌄" : "›");
    m_toggleButton->setFixedSize(24, 24);
    m_toggleButton->setFlat(true);
    m_toggleButton->setStyleSheet(
        "QPushButton { "
        "   color: #cccccc; "             // More visible (was #aaaaaa)
        "   font-size: 18px; "            // Bigger arrows (was 16px)
        "   font-weight: normal; "
        "   border: none; "
        "   background: transparent; "   // No pill
        "   padding: 0px; "              // No padding
        "}"
        "QPushButton:hover { "
        "   color: #ffffff; "
        "}"
    );
    connect(m_toggleButton, &QPushButton::clicked, this, &CategorySectionWidget::toggleExpanded);
    headerLayout->addWidget(m_toggleButton);

    // Title - BOLD WHITE, no background, no border
    QString titleText = QString::fromStdWString(m_category.displayName);
    m_titleLabel = new QLabel(titleText);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(11);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setStyleSheet(
        "QLabel { "
        "   color: #ffffff; "            // Pure white
        "   background: transparent; "   // No pill
        "   border: none; "              // No border
        "   padding: 0px; "              // No padding
        "}"
    );
    headerLayout->addWidget(m_titleLabel);

    // Status text
    QString statusText = getStatusText();
    QLabel* statusLabel = new QLabel(statusText);
    QFont statusFont = statusLabel->font();
    statusFont.setPointSize(10);
    statusLabel->setFont(statusFont);
    statusLabel->setStyleSheet(
        "QLabel { "
        "   color: #cccccc; "            // More visible (was #aaaaaa)
        "   background: transparent; "   // No pill
        "   border: none; "              // No border
        "   padding: 0px; "              // No padding
        "}"
    );
    headerLayout->addWidget(statusLabel);

    // Spacer
    headerLayout->addStretch();

    // Health badge - only if problems
    if (m_category.healthInfo.criticalCount > 0 || 
        m_category.healthInfo.warningCount > 0) {
        
        QString badgeText = getHealthIndicatorText();
        m_healthIndicator = new QLabel(badgeText);
        
        QColor badgeColor = getHealthIndicatorColor();
        QFont badgeFont = m_healthIndicator->font();
        badgeFont.setBold(true);
        badgeFont.setPointSize(9);
        m_healthIndicator->setFont(badgeFont);
        
        m_healthIndicator->setStyleSheet(QString(
            "QLabel {"
            "   color: #ffffff; "
            "   padding: 6px 12px; "
            "   border-radius: 6px; "
            "   background-color: %1; "
            "   border: none; "           // No pill border
            "}"
        ).arg(badgeColor.name()));
        
        headerLayout->addWidget(m_healthIndicator);
    } else {
        m_healthIndicator = nullptr;
    }
    
    mainLayout->addWidget(headerWidget);

    // === Content area ===
    m_contentWidget = new QWidget();
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(0, 8, 0, 0);
    m_contentLayout->setSpacing(4);
    m_contentWidget->setVisible(m_expanded);
    mainLayout->addWidget(m_contentWidget);
}

QString CategorySectionWidget::getStatusText() const
{
    // Create readable status text
    int total = m_category.healthInfo.totalDrivers;
    int critical = m_category.healthInfo.criticalCount;
    int warnings = m_category.healthInfo.warningCount;
    
    if (critical > 0) {
        return QString("%1 driver%2 • %3 critical")
            .arg(total)
            .arg(total > 1 ? "s" : "")
            .arg(critical);
    } else if (warnings > 0) {
        return QString("%1 driver%2 • %3 warning%4")
            .arg(total)
            .arg(total > 1 ? "s" : "")
            .arg(warnings)
            .arg(warnings > 1 ? "s" : "");
    } else {
        return QString("%1 driver%2 • No issues")
            .arg(total)
            .arg(total > 1 ? "s" : "");
    }
}

void CategorySectionWidget::populateDrivers()
{
    // Drivers are already sorted by CategoryProcessor
    for (const auto& driver : m_category.sortedDrivers) {
        DriverCardWidget* card = new DriverCardWidget(driver);
        m_contentLayout->addWidget(card);
    }
}

QString CategorySectionWidget::getHealthIndicatorText() const
{
    if (m_category.healthInfo.criticalCount > 0) {
        return QString("%1 critical").arg(m_category.healthInfo.criticalCount);
    } else if (m_category.healthInfo.warningCount > 0) {
        return QString("%1 warning%2")
            .arg(m_category.healthInfo.warningCount)
            .arg(m_category.healthInfo.warningCount > 1 ? "s" : "");
    } else {
        return "All healthy";
    }
}

QString CategorySectionWidget::getHealthIndicatorIcon() const
{
    // No icon needed - badge color is enough
    return "";
}

QColor CategorySectionWidget::getHealthIndicatorColor() const
{
    if (m_category.healthInfo.criticalCount > 0) {
        return QColor(231, 76, 60);   // Red
    } else if (m_category.healthInfo.warningCount > 0) {
        return QColor(243, 156, 18);  // Orange
    } else {
        return QColor(46, 204, 113);  // Green
    }
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
    m_toggleButton->setText(m_expanded ? "⌄" : "›");  // Modern thin chevrons
}