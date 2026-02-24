#include "DashboardWidget.h"
#include "ScoreRingWidget.h"
#include "IssueListWidget.h"
#include "SystemHealthSummary.h"

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
    setStyleSheet(
        "DashboardWidget {"
        "   background-color: #2b2b2b;"
        "   border: 1px solid #3d3d3d;"
        "   border-radius: 16px;"
        "}"
    );

    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(20, 18, 20, 18);
    mainLayout->setSpacing(0);

    // === Left column: score ring + counts ===
    mainLayout->addWidget(buildScoreColumn());

    // Vertical divider
    QFrame* divider1 = new QFrame();
    divider1->setFrameShape(QFrame::VLine);
    divider1->setStyleSheet("color: #3d3d3d; background-color: #3d3d3d;");
    divider1->setFixedWidth(1);
    mainLayout->addSpacing(20);
    mainLayout->addWidget(divider1);
    mainLayout->addSpacing(20);

    // === Center column: issue list ===
    mainLayout->addWidget(buildIssueColumn(), 1);  // Stretch to fill

    // Vertical divider
    QFrame* divider2 = new QFrame();
    divider2->setFrameShape(QFrame::VLine);
    divider2->setStyleSheet("color: #3d3d3d; background-color: #3d3d3d;");
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

    m_criticalCountLabel = makeCountLabel("— Critical", "#e74c3c");
    m_warningCountLabel  = makeCountLabel("— Warnings", "#f39c12");
    m_healthyCountLabel  = makeCountLabel("— Healthy",  "#27ae60");

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
        label->setStyleSheet("color: #bbbbbb; background: transparent;");
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
    divider->setStyleSheet("color: #3d3d3d; background-color: #3d3d3d;");
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
    logLabel->setStyleSheet("color: #888888; background: transparent;");
    layout->addWidget(logLabel);

    QHBoxLayout* toggleLayout = new QHBoxLayout();
    toggleLayout->setContentsMargins(0, 0, 0, 0);
    toggleLayout->setSpacing(4);

    auto makeToggleButton = [this](const QString& label, int days) -> QPushButton* {
        QPushButton* btn = new QPushButton(label);
        btn->setFixedHeight(28);
        btn->setFixedWidth(40);
        btn->setCheckable(false);

        connect(btn, &QPushButton::clicked, this, [this, days]() {
            if (m_selectedLogDays != days) {
                m_selectedLogDays = days;
                updateLogWindowButtons();
                emit logWindowChanged(days);
            }
        });

        return btn;
    };

    m_log7Button  = makeToggleButton("7d",  7);
    m_log30Button = makeToggleButton("30d", 30);
    m_log90Button = makeToggleButton("90d", 90);

    toggleLayout->addWidget(m_log7Button);
    toggleLayout->addWidget(m_log30Button);
    toggleLayout->addWidget(m_log90Button);
    toggleLayout->addStretch();

    layout->addLayout(toggleLayout);

    // Note below toggle
    QLabel* toggleNote = new QLabel("Takes effect on next scan");
    QFont noteFont = toggleNote->font();
    noteFont.setPointSize(10);
    noteFont.setItalic(true);
    toggleNote->setFont(noteFont);
    toggleNote->setStyleSheet("color: #555555; background: transparent;");
    layout->addWidget(toggleNote);

    layout->addSpacing(8);

    // === Rescan button ===
    m_rescanButton = new QPushButton("⟳  Scan Drivers");
    m_rescanButton->setFixedHeight(32);
    m_rescanButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #3a3a3a;"
        "   color: #cccccc;"
        "   border: 1px solid #555555;"
        "   border-radius: 6px;"
        "   padding: 6px 12px;"
        "   font-size: 11px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: #454545;"
        "   border-color: #5dade2;"
        "   color: #ffffff;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #2a2a2a;"
        "}"
    );
    m_rescanButton->setCursor(Qt::PointingHandCursor);
    connect(m_rescanButton, &QPushButton::clicked, this, &DashboardWidget::scanRequested);
    layout->addWidget(m_rescanButton);

    layout->addSpacing(6);

    // === Generate Report button with format menu ===
    m_reportButton = new QPushButton("📄  Generate Report");
    m_reportButton->setFixedHeight(32);
    m_reportButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #2d4356;"
        "   color: #5dade2;"
        "   border: 1px solid #3d5a6e;"
        "   border-radius: 6px;"
        "   padding: 6px 12px;"
        "   font-size: 11px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: #3a5670;"
        "   border-color: #5dade2;"
        "   color: #ffffff;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #1e2d3a;"
        "}"
        "QPushButton::menu-indicator {"
        "   width: 0px;"  // Hide default dropdown arrow
        "}"
    );
    m_reportButton->setCursor(Qt::PointingHandCursor);
    
    // Create format menu
    QMenu* reportMenu = new QMenu(m_reportButton);
    reportMenu->setStyleSheet(
        "QMenu {"
        "   background-color: #2b2b2b;"
        "   border: 1px solid #3d5a6e;"
        "   border-radius: 4px;"
        "   padding: 4px;"
        "}"
        "QMenu::item {"
        "   padding: 6px 20px;"
        "   color: #e0e0e0;"
        "}"
        "QMenu::item:selected {"
        "   background-color: #3a5670;"
        "   color: #5dade2;"
        "}"
    );
    
    QAction* textAction = reportMenu->addAction("📄 Text Report (.txt)");
    QAction* htmlAction = reportMenu->addAction("🌐 Interactive HTML (.html)");
    
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
    // Common inactive style
    const QString inactiveStyle =
        "QPushButton {"
        "   background-color: #3a3a3a;"
        "   color: #888888;"
        "   border: 1px solid #4a4a4a;"
        "   border-radius: 4px;"
        "   font-size: 10px;"
        "   padding: 2px 6px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #454545;"
        "   color: #cccccc;"
        "}";

    // Active style (selected day)
    const QString activeStyle =
        "QPushButton {"
        "   background-color: #1a3a5c;"
        "   color: #5dade2;"
        "   border: 1px solid #5dade2;"
        "   border-radius: 4px;"
        "   font-size: 10px;"
        "   font-weight: bold;"
        "   padding: 2px 6px;"
        "}";

    m_log7Button->setStyleSheet( m_selectedLogDays == 7   ? activeStyle : inactiveStyle);
    m_log30Button->setStyleSheet(m_selectedLogDays == 30  ? activeStyle : inactiveStyle);
    m_log90Button->setStyleSheet(m_selectedLogDays == 90  ? activeStyle : inactiveStyle);
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
        m_rescanButton->setText(enabled ? "⟳  Scan Drivers" : "⟳  Scanning...");
    }
}