#include "CategorySectionWidget.h"
#include "DriverCardWidget.h"
#include "AppTheme.h"
#include "IconProvider.h"
#include <QHBoxLayout>

CategorySectionWidget::CategorySectionWidget(const ProcessedCategory& category,
                                             int logDays,
                                             QWidget* parent)
    : QWidget(parent)
    , m_expanded(false)
    , m_category(category)
    , m_logDays(logDays)
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
    const auto& c = AppTheme::colors();
    QString headerStyle = QString(
        "QWidget { "
        "   background-color: %1; "
        "   border-radius: 16px; "        
        "   border: 1px solid %2; "
        "}"
        "QWidget:hover { "
        "   background-color: %3; "
        "}").arg(c.bgElevated, c.borderNormal, c.bgHover);
    
    headerWidget->setStyleSheet(headerStyle);
    headerWidget->setCursor(Qt::PointingHandCursor);
    
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(16, 14, 16, 14);
    headerLayout->setSpacing(12);

    // Toggle button with SVG icon
    m_toggleButton = new QPushButton();
    m_toggleButton->setFixedSize(24, 24);
    m_toggleButton->setFlat(true);
    m_toggleButton->setIcon(IconProvider::icon(
        m_expanded ? IconProvider::ChevronDown : IconProvider::ChevronRight,
        AppTheme::colors().textSecondary,
        20  // Slightly smaller than button for padding
    ));
    m_toggleButton->setIconSize(QSize(20, 20));
    m_toggleButton->setStyleSheet(QString(
        "QPushButton { "
        "   border: none; "
        "   background: transparent; "
        "   padding: 0px; "
        "}"
        "QPushButton:hover { "
        "}"
    ));
    connect(m_toggleButton, &QPushButton::clicked, this, &CategorySectionWidget::toggleExpanded);
    headerLayout->addWidget(m_toggleButton);

    // Title - BOLD WHITE, no background, no border
    QString titleText = QString::fromStdWString(m_category.displayName);
    m_titleLabel = new QLabel(titleText);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(11);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setStyleSheet(QString(
        "QLabel { "
        "   color: %1; "            // Pure white
        "   background: transparent; "   // No pill
        "   border: none; "              // No border
        "   padding: 0px; "              // No padding
        "}"
    ).arg(c.textPrimary));
    headerLayout->addWidget(m_titleLabel);

    // Status text
    QString statusText = getStatusText();
    QLabel* statusLabel = new QLabel(statusText);
    QFont statusFont = statusLabel->font();
    statusFont.setPointSize(10);
    statusLabel->setFont(statusFont);
    statusLabel->setStyleSheet(QString(
        "QLabel { "
        "   color: %1; "            // More visible (was #aaaaaa)
        "   background: transparent; "   // No pill
        "   border: none; "              // No border
        "   padding: 0px; "              // No padding
        "}"
    ).arg(c.textSecondary));
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
            "   color: %2; "
            "   padding: 6px 12px; "
            "   border-radius: 6px; "
            "   background-color: %1; "
            "   border: none; "           // No pill border
            "}"
        ).arg(badgeColor.name(), c.textOnAccent));
        
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
        DriverCardWidget* card = new DriverCardWidget(driver, m_logDays);
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
    const auto& c = AppTheme::colors();
    if (m_category.healthInfo.criticalCount > 0) return QColor(c.critical);
    if (m_category.healthInfo.warningCount  > 0) return QColor(c.warning);
    return QColor(c.healthy);
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
    const auto& c = AppTheme::colors();
    m_toggleButton->setIcon(IconProvider::icon(
        m_expanded ? IconProvider::ChevronDown : IconProvider::ChevronRight,
        c.textSecondary,
        20
    ));
}