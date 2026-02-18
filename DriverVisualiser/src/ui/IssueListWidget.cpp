#include "IssueListWidget.h"

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
    headerFont.setPointSize(10);
    headerFont.setBold(true);
    header->setFont(headerFont);
    header->setStyleSheet("color: #cccccc; padding-bottom: 6px;");
    outerLayout->addWidget(header);

    // === Scroll area ===
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setMaximumHeight(160);  // Compact - shows ~4 issues before scrolling
    m_scrollArea->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
        "QScrollBar:vertical {"
        "   background: #1e1e1e;"
        "   width: 6px;"
        "   border-radius: 3px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background: #444;"
        "   border-radius: 3px;"
        "   min-height: 20px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "   height: 0px;"
        "}"
    );

    m_listContainer = new QWidget();
    m_listContainer->setStyleSheet("background: transparent;");

    m_listLayout = new QVBoxLayout(m_listContainer);
    m_listLayout->setContentsMargins(0, 0, 4, 0);
    m_listLayout->setSpacing(3);

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
    row->setStyleSheet(
        "QWidget {"
        "   background-color: #252525;"
        "   border-radius: 4px;"
        "}"
    );

    QHBoxLayout* layout = new QHBoxLayout(row);
    layout->setContentsMargins(8, 5, 8, 5);
    layout->setSpacing(10);

    // === Severity dot ===
    QLabel* dot = new QLabel("●");
    dot->setFixedWidth(12);

    if (issue.isUserDisabled) {
        dot->setStyleSheet("color: #666666; font-size: 10px; background: transparent;");
    } else {
        QString color = severityColor(issue.severity);
        dot->setStyleSheet(QString("color: %1; font-size: 10px; background: transparent;").arg(color));
    }
    layout->addWidget(dot);

    // === Driver name ===
    QString displayName = QString::fromStdWString(issue.driverName);
    QLabel* nameLabel = new QLabel(displayName);
    nameLabel->setFixedWidth(160);

    QFont nameFont = nameLabel->font();
    nameFont.setPointSize(9);
    nameFont.setBold(!issue.isUserDisabled);
    nameLabel->setFont(nameFont);

    if (issue.isUserDisabled) {
        nameLabel->setStyleSheet("color: #666666; background: transparent;");
    } else {
        nameLabel->setStyleSheet("color: #e0e0e0; background: transparent;");
    }

    nameLabel->setToolTip(displayName);  // Full name on hover if truncated
    layout->addWidget(nameLabel);

    // === Description ===
    QString descText = QString::fromStdWString(issue.description);
    QLabel* descLabel = new QLabel(descText);

    QFont descFont = descLabel->font();
    descFont.setPointSize(9);
    descLabel->setFont(descFont);

    if (issue.isUserDisabled) {
        descLabel->setStyleSheet("color: #555555; background: transparent;");
    } else {
        descLabel->setStyleSheet("color: #aaaaaa; background: transparent;");
    }

    descLabel->setToolTip(descText);
    layout->addWidget(descLabel, 1);  // Stretch to fill

    // === Category badge ===
    if (!issue.categoryName.empty()) {
        QString catText = QString::fromStdWString(issue.categoryName);
        QLabel* badge = new QLabel(catText);

        QFont badgeFont = badge->font();
        badgeFont.setPointSize(8);
        badge->setFont(badgeFont);

        if (issue.isUserDisabled) {
            badge->setStyleSheet(
                "QLabel {"
                "   color: #555555;"
                "   background-color: #2a2a2a;"
                "   border: 1px solid #333;"
                "   border-radius: 3px;"
                "   padding: 1px 6px;"
                "}"
            );
        } else {
            badge->setStyleSheet(
                "QLabel {"
                "   color: #5dade2;"
                "   background-color: rgba(93, 173, 226, 0.1);"
                "   border: 1px solid rgba(93, 173, 226, 0.3);"
                "   border-radius: 3px;"
                "   padding: 1px 6px;"
                "}"
            );
        }
        layout->addWidget(badge);
    }

    return row;
}

QWidget* IssueListWidget::createNoIssuesRow()
{
    QLabel* label = new QLabel("✓  No critical issues detected");
    QFont font = label->font();
    font.setPointSize(9);
    label->setFont(font);
    label->setStyleSheet(
        "QLabel {"
        "   color: #27ae60;"
        "   padding: 6px 4px;"
        "   background: transparent;"
        "}"
    );
    return label;
}

QString IssueListWidget::severityColor(HealthFlagSeverity severity) const
{
    switch (severity) {
        case HealthFlagSeverity::Critical: return "#e74c3c";
        case HealthFlagSeverity::Warning:  return "#f39c12";
        case HealthFlagSeverity::Caution:  return "#f1c40f";
        default:                           return "#888888";
    }
}
