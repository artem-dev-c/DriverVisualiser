#include "DashboardWidget.h"
#include "AppTheme.h"
#include "ScoreRingWidget.h"
#include "IssueListWidget.h"
#include "SystemHealthSummary.h"
#include "IconProvider.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QDateTime>
#include <QFont>

// ============================================================================
// Construction
// ============================================================================

DashboardWidget::DashboardWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void DashboardWidget::setupUi()
{
    // Outer card styling - matches CategorySectionWidget header style
    setAutoFillBackground(true);
    setStyleSheet(QString(
        "DashboardWidget {"
        "   background-color: %1;"
        "   border: 1px solid %2;"
        "   border-radius: 16px;"
        "}"
    ).arg(AppTheme::colors().bgElevated, AppTheme::colors().borderNormal));

    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(20, 18, 20, 18);
    mainLayout->setSpacing(0);

    // === Left column: score ring + counts ===
    mainLayout->addWidget(buildScoreColumn());

    // Vertical divider
    QFrame* divider1 = new QFrame();
    divider1->setFrameShape(QFrame::VLine);
    divider1->setStyleSheet(QString("color: %1; background-color: %1;").arg(AppTheme::colors().divider));
    divider1->setFixedWidth(1);
    mainLayout->addSpacing(20);
    mainLayout->addWidget(divider1);
    mainLayout->addSpacing(20);

    // === Center column: issue list ===
    mainLayout->addWidget(buildIssueColumn(), 1);  // Stretch to fill

    // Vertical divider
    QFrame* divider2 = new QFrame();
    divider2->setFrameShape(QFrame::VLine);
    divider2->setStyleSheet(QString("color: %1; background-color: %1;").arg(AppTheme::colors().divider));
    divider2->setFixedWidth(1);
    mainLayout->addSpacing(20);
    mainLayout->addWidget(divider2);
    mainLayout->addSpacing(20);

    // === Right column: system info + controls ===
    mainLayout->addWidget(buildControlColumn());
}

// ============================================================================
// Column Builders
// ============================================================================

QWidget* DashboardWidget::buildScoreColumn()
{
    QWidget* col = new QWidget();
    col->setStyleSheet("background: transparent;");
    col->setFixedWidth(190);

    QVBoxLayout* layout = new QVBoxLayout(col);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->setAlignment(Qt::AlignHCenter);

    // Score ring
    m_scoreRing = new ScoreRingWidget();
    layout->addWidget(m_scoreRing, 0, Qt::AlignHCenter);

    // Driver count pills
    auto makeCountLabel = [](const QString& text, const QString& color) -> QLabel* {
        QLabel* label = new QLabel(text);
        QFont f = label->font();
        f.setPointSize(10);
        label->setFont(f);
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet(QString(
            "QLabel {"
            "   color: %1;"
            "   background-color: transparent;"
            "   padding: 0px;"
            "}"
        ).arg(color));
        return label;
    };

    m_criticalCountLabel = makeCountLabel("— Critical", AppTheme::colors().critical);
    m_warningCountLabel  = makeCountLabel("— Warnings", AppTheme::colors().warning);
    m_healthyCountLabel  = makeCountLabel("— Healthy",  AppTheme::colors().healthy);

    layout->addWidget(m_criticalCountLabel);
    layout->addWidget(m_warningCountLabel);
    layout->addWidget(m_healthyCountLabel);
    layout->addStretch();

    return col;
}

QWidget* DashboardWidget::buildIssueColumn()
{
    QWidget* col = new QWidget();
    col->setStyleSheet("background: transparent;");

    QVBoxLayout* layout = new QVBoxLayout(col);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_issueList = new IssueListWidget();
    layout->addWidget(m_issueList);
    layout->addStretch();

    return col;
}

QWidget* DashboardWidget::buildControlColumn()
{
    const auto& c = AppTheme::colors();
    QWidget* col = new QWidget();
    col->setStyleSheet("background: transparent;");
    col->setFixedWidth(200);

    QVBoxLayout* layout = new QVBoxLayout(col);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    // === System Info ===
    auto makeSysLabel = [](const QString& text) -> QLabel* {
        QLabel* label = new QLabel(text);
        QFont f = label->font();
        f.setPointSize(10);
        label->setFont(f);
        label->setStyleSheet(QString("color: %1; background: transparent;").arg(AppTheme::colors().textSecondary));
        label->setWordWrap(true);
        return label;
    };

    m_osLabel    = makeSysLabel("OS: —");
    m_buildLabel = makeSysLabel("Build: —");
    m_archLabel  = makeSysLabel("Arch: —");

    layout->addWidget(m_osLabel);
    layout->addWidget(m_buildLabel);
    layout->addWidget(m_archLabel);

    // Divider
    QFrame* divider = new QFrame();
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QString("color: %1; background-color: %1;").arg(AppTheme::colors().divider));
    divider->setFixedHeight(1);
    layout->addSpacing(4);
    layout->addWidget(divider);
    layout->addSpacing(4);

    // === Last scan label ===
    m_lastScanLabel = makeSysLabel("Last scan: —");
    layout->addWidget(m_lastScanLabel);

    layout->addSpacing(4);

    // === Log window toggle ===
    QLabel* logLabel = new QLabel("Event log window:");
    QFont logLabelFont = logLabel->font();
    logLabelFont.setPointSize(10);
    logLabel->setFont(logLabelFont);
    logLabel->setStyleSheet(QString("color: %1; background: transparent;").arg(AppTheme::colors().textSecondary));
    layout->addWidget(logLabel);

    QHBoxLayout* toggleLayout = new QHBoxLayout();
    toggleLayout->setContentsMargins(0, 0, 0, 0);
    toggleLayout->setSpacing(0);

    // Position: 0=left, 1=middle, 2=right
    auto makeToggleButton = [this](const QString& label, int days, int position) -> QPushButton* {
        QPushButton* btn = new QPushButton(label);
        btn->setFixedHeight(32);
        btn->setFixedWidth(66.6); // 200 total width / 3 buttons
        btn->setCheckable(false);
        btn->setProperty("segmentPos", position);

        connect(btn, &QPushButton::clicked, this, [this, days]() {
            if (m_selectedLogDays != days) {
                m_selectedLogDays = days;
                updateLogWindowButtons();
                emit logWindowChanged(days);
            }
        });

        return btn;
    };

    m_log30Button  = makeToggleButton("30d",  30,  0);
    m_log90Button  = makeToggleButton("90d",  90,  1);
    m_log180Button = makeToggleButton("180d", 180, 2);

    toggleLayout->addWidget(m_log30Button);
    toggleLayout->addWidget(m_log90Button);
    toggleLayout->addWidget(m_log180Button);
    toggleLayout->addStretch();

    layout->addLayout(toggleLayout);

    // Note below toggle
    QLabel* toggleNote = new QLabel("Takes effect on next scan");
    QFont noteFont = toggleNote->font();
    noteFont.setPointSize(10);
    noteFont.setItalic(true);
    toggleNote->setFont(noteFont);
    toggleNote->setStyleSheet(QString("color: %1; background: transparent;").arg(AppTheme::colors().textMuted));
    layout->addWidget(toggleNote);

    layout->addSpacing(8);

    // === Rescan button ===
    m_rescanButton = new QPushButton("  Scan Drivers");
    m_rescanButton->setIcon(IconProvider::icon(
        IconProvider::RefreshDot,
        AppTheme::colors().textSecondary,
        20  // Increased from 18
    ));
    m_rescanButton->setIconSize(QSize(20, 20));
    m_rescanButton->setFixedHeight(32);
    m_rescanButton->setStyleSheet(QString(
        "QPushButton {"
        "   background-color: %1;"
        "   color: %2;"
        "   border: 1px solid %3;"
        "   border-radius: 6px;"
        "   padding: 6px 12px;"
        "   font-size: 11px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: %4;"
        "   border-color: %5;"
        "   color: %6;"
        "}"
        "QPushButton:pressed {"
        "   background-color: %7;"
        "}"
    ).arg(AppTheme::colors().bgButtonNeutral, AppTheme::colors().textSecondary,
          AppTheme::colors().borderStrong, AppTheme::colors().bgHover,
          AppTheme::colors().accent, AppTheme::colors().textPrimary,
          AppTheme::colors().bgBase));
    m_rescanButton->setCursor(Qt::PointingHandCursor);
    connect(m_rescanButton, &QPushButton::clicked, this, &DashboardWidget::scanRequested);
    layout->addWidget(m_rescanButton);

    layout->addSpacing(6);

    // === Generate Report button with format menu ===
    m_reportButton = new QPushButton("  Generate Report");
    m_reportButton->setIcon(IconProvider::icon(
        IconProvider::FileDownload,
        AppTheme::colors().accentText,
        20  // Increased from 18
    ));
    m_reportButton->setIconSize(QSize(20, 20));
    m_reportButton->setFixedHeight(32);
    m_reportButton->setStyleSheet(QString(
        "QPushButton {"
        "   background-color: %1;"
        "   color: %2;"
        "   border: 1px solid %3;"
        "   border-radius: 6px;"
        "   padding: 6px 12px;"
        "   font-size: 11px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: %4;"
        "   border-color: %5;"
        "   color: %6;"
        "}"
        "QPushButton:pressed {"
        "   background-color: %7;"
        "}"
        "QPushButton::menu-indicator {"
        "   width: 0px;"  // Hide default dropdown arrow
        "}"
    ).arg(AppTheme::colors().bgButtonActive, AppTheme::colors().accentText,
          AppTheme::colors().borderActive, AppTheme::colors().accentBg,
          AppTheme::colors().accent, AppTheme::colors().textPrimary,
          AppTheme::colors().bgBase));
    m_reportButton->setCursor(Qt::PointingHandCursor);
    
    // Create format menu
    QMenu* reportMenu = new QMenu(m_reportButton);
    reportMenu->setStyleSheet(QString(
        "QMenu {"
        "   background-color: %1;"
        "   border: 1px solid %2;"
        "   border-radius: 0px;"
        "   padding: 6px;"
        "   width: 200px;"
        "}"
        "QMenu::item {"
        "   padding: 10px 12px;"
        "   color: %3;"
        "   border-radius: 4px;"
        "   margin: 2px;"
        "}"
        "QMenu::item:selected {"
        "   background-color: %4;"
        "   color: %5;"
        "}"
    ).arg(AppTheme::colors().bgOverlay, AppTheme::colors().borderStrong,
          AppTheme::colors().textPrimary, AppTheme::colors().accentBg,
          AppTheme::colors().accentText));
    
    QAction* textAction = new QAction(m_reportButton);
    textAction->setText(" Text Report (.txt)");  
    textAction->setIcon(IconProvider::icon(
        IconProvider::FileTypeTxt,
        AppTheme::colors().textPrimary,
        20  // Larger icon
    ));
    reportMenu->addAction(textAction);
    
    QAction* htmlAction = new QAction(m_reportButton);
    htmlAction->setText(" Interactive HTML (.html)");
    htmlAction->setIcon(IconProvider::icon(
        IconProvider::FileTypeHtml,
        AppTheme::colors().textPrimary,
        20  // Larger icon
    ));
    reportMenu->addAction(htmlAction);
    
    connect(textAction, &QAction::triggered, this, [this]() {
        emit reportRequested(ReportFormat::Text);
    });
    connect(htmlAction, &QAction::triggered, this, [this]() {
        emit reportRequested(ReportFormat::Html);
    });
    
    m_reportButton->setMenu(reportMenu);
    layout->addWidget(m_reportButton);

    layout->addStretch();

    // Apply initial toggle button styles
    updateLogWindowButtons();

    return col;
}

// ============================================================================
// Populate
// ============================================================================

void DashboardWidget::populate(const std::vector<DriverInfo>& drivers,
                                const SystemInfo& sysInfo)
{
    // Compute system health from all drivers
    SystemHealthResult result = SystemHealthSummary::compute(drivers);

    // Update score ring
    m_scoreRing->setScore(result.score);

    // Update count labels
    updateCountLabels(result);

    // Update issue list
    m_issueList->setIssues(result.issues);

    // Update system info
    updateSystemInfoLabels(sysInfo);

    // Update last scan time
    QString now = QDateTime::currentDateTime().toString("hh:mm  dd MMM yyyy");
    m_lastScanLabel->setText("Last scan: " + now);
}

// ============================================================================
// Update Helpers
// ============================================================================

void DashboardWidget::updateCountLabels(const SystemHealthResult& result)
{
    m_criticalCountLabel->setText(
        QString("%1 Critical").arg(result.criticalDrivers)
    );
    m_warningCountLabel->setText(
        QString("%1 Warnings").arg(result.warningDrivers)
    );
    m_healthyCountLabel->setText(
        QString("%1 Healthy").arg(result.healthyDrivers)
    );
}

void DashboardWidget::updateSystemInfoLabels(const SystemInfo& sysInfo)
{
    // Combine OS name and display version on one line
    QString osText = QString::fromStdWString(sysInfo.osName);
    if (!sysInfo.displayVersion.empty()) {
        osText += " " + QString::fromStdWString(sysInfo.displayVersion);
    }
    m_osLabel->setText(osText);

    QString buildText = "Build " + QString::fromStdWString(sysInfo.buildNumber);
    if (!sysInfo.ubr.empty() && sysInfo.ubr != L"0") {
        buildText += "." + QString::fromStdWString(sysInfo.ubr);
    }
    m_buildLabel->setText(buildText);
    m_archLabel->setText(QString::fromStdWString(sysInfo.architecture));
}

void DashboardWidget::updateLogWindowButtons()
{
    // Segmented control styled to match rescan/report buttons (height 32, font-size 11px).
    // Buttons share borders: left has rounded left corners, right has rounded right corners,
    // middle has none. Middle and right suppress the left border to avoid double lines.
    auto applyStyle = [](QPushButton* btn, bool active, int pos) {
        QString radius;
        if      (pos == 0) radius = "border-top-left-radius: 6px; border-bottom-left-radius: 6px; border-top-right-radius: 0px; border-bottom-right-radius: 0px;";
        else if (pos == 2) radius = "border-top-right-radius: 6px; border-bottom-right-radius: 6px; border-top-left-radius: 0px; border-bottom-left-radius: 0px;";
        else               radius = "border-radius: 0px;";

        QString leftBorder = (pos == 0) ? "" : "border-left: none;";

        if (active) {
            btn->setStyleSheet(QString(
                "QPushButton {"
                "   background-color: %1;"
                "   color: %2;"
                "   border: 1px solid %3;"
                "   %4 %5"
                "   font-size: 11px;"
                "   font-weight: bold;"
                "   padding: 4px 10px;"
                "}"
            ).arg(AppTheme::colors().bgButtonActive,
                  AppTheme::colors().accentText,
                  AppTheme::colors().borderActive,
                  radius, leftBorder));
        } else {
            btn->setStyleSheet(QString(
                "QPushButton {"
                "   background-color: %1;"
                "   color: %2;"
                "   border: 1px solid %3;"
                "   %4 %5"
                "   font-size: 11px;"
                "   padding: 4px 10px;"
                "}"
                "QPushButton:hover {"
                "   background-color: %6;"
                "   color: %7;"
                "}"
            ).arg(AppTheme::colors().bgButtonNeutral,
                  AppTheme::colors().textSecondary,
                  AppTheme::colors().borderButtonNeutral,
                  radius, leftBorder,
                  AppTheme::colors().bgHover,
                  AppTheme::colors().textPrimary));
        }
    };

    applyStyle(m_log30Button,  m_selectedLogDays == 30,  0);
    applyStyle(m_log90Button,  m_selectedLogDays == 90,  1);
    applyStyle(m_log180Button, m_selectedLogDays == 180, 2);
}


// ============================================================================
// Accessors
// ============================================================================

int DashboardWidget::selectedLogDays() const
{
    return m_selectedLogDays;
}

// ============================================================================
// Public Accessors (added for async scan support)
// ============================================================================

void DashboardWidget::setSelectedLogDays(int days)
{
    m_selectedLogDays = days;
    updateLogWindowButtons();
}

void DashboardWidget::setScanButtonEnabled(bool enabled)
{
    if (m_rescanButton) {
        m_rescanButton->setEnabled(enabled);
        m_rescanButton->setText(enabled ? "  Scan Drivers" : "  Scanning...");
    }
}