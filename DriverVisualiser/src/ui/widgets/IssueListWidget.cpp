#include "IssueListWidget.h"
#include "ClassNameMapper.h"
#include "AppTheme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>

IssueListWidget::IssueListWidget(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // === Section header ===
    QLabel* header = new QLabel("Detected Issues");
    QFont headerFont = header->font();
    headerFont.setPointSize(13);
    headerFont.setBold(true);
    header->setFont(headerFont);
    header->setStyleSheet(QString("color: %1; padding-bottom: 10px;").arg(AppTheme::colors().textSecondary));
    outerLayout->addWidget(header);

    // === Scroll area ===
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setMaximumHeight(240);  // Shows ~4 issues before scrolling
    m_scrollArea->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
        + AppTheme::scrollbarStyleSheet()
    );

    m_listContainer = new QWidget();
    m_listContainer->setStyleSheet("background: transparent;");

    m_listLayout = new QVBoxLayout(m_listContainer);
    m_listLayout->setContentsMargins(0, 0, 4, 0);
    m_listLayout->setSpacing(5);

    m_scrollArea->setWidget(m_listContainer);
    outerLayout->addWidget(m_scrollArea);

    // Show placeholder on construction
    m_listLayout->addWidget(createNoIssuesRow());
    m_listLayout->addStretch();
}

void IssueListWidget::setIssues(const std::vector<DashboardIssue>& issues)
{
    clearIssues();

    if (issues.empty()) {
        m_listLayout->addWidget(createNoIssuesRow());
    } else {
        for (const auto& issue : issues) {
            m_listLayout->addWidget(createIssueRow(issue));
        }
    }

    m_listLayout->addStretch();
}

void IssueListWidget::clearIssues()
{
    QLayoutItem* item;
    while ((item = m_listLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}

QWidget* IssueListWidget::createIssueRow(const DashboardIssue& issue)
{
    QWidget* row = new QWidget();
    row->setStyleSheet(QString(
        "QWidget {"
        "   background-color: %1;"
        "   border-radius: 4px;"
        "}"
    ).arg(AppTheme::colors().bgIssueRow));

    QHBoxLayout* layout = new QHBoxLayout(row);
    layout->setContentsMargins(10, 9, 10, 9);
    layout->setSpacing(14);

    // === Severity indicator — small rounded rectangle ===
    QLabel* dot = new QLabel();
    dot->setFixedSize(6, 20);
    if (issue.isUserDisabled) {
        dot->setStyleSheet(QString("background-color: %1; border-radius: 2px;").arg(AppTheme::colors().textDisabled));
    } else {
        dot->setStyleSheet(QString("background-color: %1; border-radius: 2px;").arg(severityColor(issue.severity)));
    }
    layout->addWidget(dot);

    // === Driver name ===
    QString displayName = QString::fromStdWString(issue.driverName);
    QLabel* nameLabel = new QLabel(displayName);
    nameLabel->setFixedWidth(220);

    QFont nameFont = nameLabel->font();
    nameFont.setPointSize(11);
    nameFont.setBold(!issue.isUserDisabled);
    nameLabel->setFont(nameFont);

    if (issue.isUserDisabled) {
        nameLabel->setStyleSheet(QString(
            "color: %1; background: transparent;"
        ).arg(AppTheme::colors().textDisabled, AppTheme::colors().bgOverlay,
              AppTheme::colors().textPrimary,  AppTheme::colors().borderStrong));
    } else {
        nameLabel->setStyleSheet(QString(
            "color: %1; background: transparent;"
        ).arg(AppTheme::colors().textPrimary,  AppTheme::colors().bgOverlay,
              AppTheme::colors().textPrimary,  AppTheme::colors().borderStrong));
    }

    layout->addWidget(nameLabel);

    // === Description ===
    QString descText = QString::fromStdWString(issue.description);
    QLabel* descLabel = new QLabel(descText);

    QFont descFont = descLabel->font();
    descFont.setPointSize(11);
    descLabel->setFont(descFont);

    if (issue.isUserDisabled) {
        descLabel->setStyleSheet(QString(
            "color: %1; background: transparent;"
        ).arg(AppTheme::colors().textMuted,    AppTheme::colors().bgOverlay,
              AppTheme::colors().textPrimary,  AppTheme::colors().borderStrong));
    } else {
        descLabel->setStyleSheet(QString(
            "color: %1; background: transparent;"
        ).arg(AppTheme::colors().textSecondary, AppTheme::colors().bgOverlay,
              AppTheme::colors().textPrimary,   AppTheme::colors().borderStrong));
    }

    layout->addWidget(descLabel, 1);  // Stretch to fill

    // === Category badge ===
    if (!issue.categoryName.empty()) {
        QString catText = QString::fromStdWString(
            ClassNameMapper::getDisplayName(issue.categoryName)
        );
        QLabel* badge = new QLabel(catText);

        QFont badgeFont = badge->font();
        badgeFont.setPointSize(10);
        badge->setFont(badgeFont);

        if (issue.isUserDisabled) {
            badge->setStyleSheet(QString(
                "QLabel {"
                "   color: %1;"
                "   background-color: %2;"
                "   border: 1px solid %3;"
                "   border-radius: 6px;"
                "   padding: 1px 6px;"
                "}"
            ).arg(AppTheme::colors().textMuted, AppTheme::colors().bgCard, AppTheme::colors().borderNormal
            ));
        } else {
            badge->setStyleSheet(QString(
                "QLabel {"
                "   color: %1;"
                "   background-color: %2;"
                "   border: 1px solid %3;"
                "   border-radius: 6px;"
                "   padding: 1px 6px;"
                "}"
            ).arg(AppTheme::colors().accentText, AppTheme::colors().accentBg, AppTheme::colors().accentBgBorder
            ));
        }
        layout->addWidget(badge);
    }

    return row;
}

QWidget* IssueListWidget::createNoIssuesRow()
{
    QLabel* label = new QLabel("✓  No critical issues detected");
    QFont font = label->font();
    font.setPointSize(11);
    label->setFont(font);
    label->setStyleSheet(
        "QLabel {"
        "   color: %1;"
        "   padding: 10px 4px;"
        "   background: transparent;"
        "}"
    );
    return label;
}

QString IssueListWidget::severityColor(HealthFlagSeverity severity) const
{
    switch (severity) {
        case HealthFlagSeverity::Critical: return AppTheme::colors().critical;
        case HealthFlagSeverity::Warning:  return AppTheme::colors().warning;
        case HealthFlagSeverity::Caution:  return AppTheme::colors().caution;
        default:                           return AppTheme::colors().textSecondary;
    }
}