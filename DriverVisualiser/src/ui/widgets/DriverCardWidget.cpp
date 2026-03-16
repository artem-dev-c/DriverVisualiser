#include "DriverCardWidget.h"
#include "DriverVersionFormatter.h"
#include "DriverStatusFormatter.h"
#include "DriverImportanceEvaluator.h"
#include "DriverDateFormatter.h"
#include "FlagIndicatorWidget.h"
#include "ErrorLogIndicatorWidget.h"
#include "DriverInfoPopup.h"
#include "AppTheme.h"
#include "IconProvider.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QProgressBar>

// ============================================================================
// Construction
// ============================================================================

DriverCardWidget::DriverCardWidget(const DriverInfo& driver, int logDays, QWidget* parent)
    : QFrame(parent)
    , m_logDays(logDays)
    , m_infoPopup(nullptr)
    , m_driver(driver)
{
    setupUi(driver);
}

// ============================================================================
// UI Setup
// ============================================================================

void DriverCardWidget::setupUi(const DriverInfo& driver)
{
    setFrameStyle(QFrame::NoFrame);
    setAutoFillBackground(true);
    applyCardStyle(driver);

    // ── Outer layout: accent bar (left) + card content ───────────────────────
    QHBoxLayout* outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // Left accent bar — colored strip indicating worst health state
    QFrame* accentBar = new QFrame();
    accentBar->setFixedWidth(4);
    accentBar->setStyleSheet(QString(
        "QFrame {"
        "   background-color: %1;"
        "   border-radius: 2px;"
        "}"
    ).arg(getAccentColor(driver)));
    outerLayout->addWidget(accentBar);

    // ── Inner content widget ─────────────────────────────────────────────────
    QWidget* content = new QWidget();
    content->setStyleSheet("background: transparent;");

    QHBoxLayout* contentLayout = new QHBoxLayout(content);
    contentLayout->setContentsMargins(14, 10, 12, 10);
    contentLayout->setSpacing(0);

    // ── 1. Left block: name + importance badge + metadata ────────────────────
    QWidget* leftBlock = new QWidget();
    leftBlock->setStyleSheet("background: transparent;");
    QVBoxLayout* leftLayout = new QVBoxLayout(leftBlock);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(5);

    // ── 1a. Name row: device name + importance badge ──────────────────────────
    QHBoxLayout* nameRow = new QHBoxLayout();
    nameRow->setContentsMargins(0, 0, 0, 0);
    nameRow->setSpacing(8);

    // Device name — use provider if available, else class name
    QString deviceName;
    if (!driver.provider.empty() && driver.provider != L"Unknown") {
        deviceName = QString::fromStdWString(driver.provider);
    } else {
        deviceName = QString::fromStdWString(driver.name);
    }

    m_nameLabel = new QLabel(deviceName);
    QFont nameFont = m_nameLabel->font();
    nameFont.setPointSize(11);
    nameFont.setBold(true);
    m_nameLabel->setFont(nameFont);
    m_nameLabel->setStyleSheet(QString("color: %1; background: transparent;").arg(AppTheme::colors().textPrimary));
    nameRow->addWidget(m_nameLabel);

    // Importance badge (replaces 3-square indicator)
    DriverImportance importance = DriverImportanceEvaluator::evaluate(driver);
    nameRow->addWidget(createImportanceBadge(importance));

    nameRow->addStretch();

    leftLayout->addLayout(nameRow);

    // ── 1b. Metadata row: status pill · version · date ────────────────────────
    QHBoxLayout* metaRow = new QHBoxLayout();
    metaRow->setContentsMargins(0, 0, 0, 0);
    metaRow->setSpacing(0);

    // Status — small pill badge
    QString statusText  = DriverStatusFormatter::statusToString(driver.status);
    QString statusCol   = statusColor(driver.status);
    m_statusLabel = new QLabel(statusText);
    QFont statusFont = m_statusLabel->font();
    statusFont.setPointSize(9);
    statusFont.setBold(true);
    m_statusLabel->setFont(statusFont);
    m_statusLabel->setStyleSheet(QString(
        "QLabel {"
        "   color: %1;"
        "   background-color: transparent;"
        "   border: 1px solid %1;"
        "   border-radius: 4px;"
        "   padding: 1px 7px;"
        "}"
    ).arg(statusCol));
    metaRow->addWidget(m_statusLabel);

    // Separator dots between metadata items
    auto makeSep = []() -> QLabel* {
        QLabel* sep = new QLabel("  ·  ");
        sep->setStyleSheet(QString("color: %1; background: transparent;").arg(AppTheme::colors().textSeparator));
        return sep;
    };

    // Version
    metaRow->addWidget(makeSep());
    m_versionLabel = new QLabel(DriverVersionFormatter::versionToString(driver.version));
    m_versionLabel->setStyleSheet(QString("color: %1; background: transparent; font-size: 9pt;").arg(AppTheme::colors().textSecondary));
    metaRow->addWidget(m_versionLabel);

    // Manufacturer
    metaRow->addWidget(makeSep());
    m_manufacturerLabel = new QLabel(QString::fromStdWString(driver.manufacturer));
    m_manufacturerLabel->setStyleSheet(QString("color: %1; background: transparent; font-size: 9pt;").arg(AppTheme::colors().textSecondary));
    metaRow->addWidget(m_manufacturerLabel);

    // Driver date
    QString driverDateText = DriverDateFormatter::isDriverDateValid(driver.driverDate)
        ? DriverDateFormatter::dateToString(driver.driverDate)
        : "Unknown";
    metaRow->addWidget(makeSep());
    m_driverDateLabel = new QLabel("Driver: " + driverDateText);
    m_driverDateLabel->setStyleSheet(QString("color: %1; background: transparent; font-size: 9pt;").arg(AppTheme::colors().textSecondary));
    metaRow->addWidget(m_driverDateLabel);

    // Install date
    metaRow->addWidget(makeSep());
    m_installDateLabel = new QLabel("Installed: " + DriverDateFormatter::dateToString(driver.installDate));
    m_installDateLabel->setStyleSheet(QString("color: %1; background: transparent; font-size: 9pt;").arg(AppTheme::colors().textSecondary));
    metaRow->addWidget(m_installDateLabel);

    metaRow->addStretch();
    leftLayout->addLayout(metaRow);

    contentLayout->addWidget(leftBlock, 1);  // Stretch to fill

    // ── 2. Health block ───────────────────────────────────────────────────────
    contentLayout->addSpacing(16);
    contentLayout->addWidget(createHealthBlock(driver.healthScore));

    // ── 3. Error log indicator ────────────────────────────────────────────────
    contentLayout->addSpacing(10);
    m_errorLogIndicator = new ErrorLogIndicatorWidget(driver.errorLog, m_logDays);
    contentLayout->addWidget(m_errorLogIndicator);

    // ── 4. Flag indicator ─────────────────────────────────────────────────────
    contentLayout->addSpacing(10);
    m_flagIndicator = new FlagIndicatorWidget(driver.healthFlags);
    contentLayout->addWidget(m_flagIndicator);

    // ── 5. Info button ────────────────────────────────────────────────────────
    contentLayout->addSpacing(8);
    QPushButton* infoButton = new QPushButton();
    infoButton->setIcon(IconProvider::icon(
        IconProvider::InfoCircle,
        AppTheme::colors().textMuted,
        22  // Larger, more visible
    ));
    infoButton->setIconSize(QSize(22, 22));
    infoButton->setFixedSize(28, 28);  // Slightly larger button
    infoButton->setFlat(true);
    infoButton->setStyleSheet(QString(
        "QPushButton {"
        "   border: none;"
        "   background: transparent;"
        "   padding: 3px;"  // Add padding for better click area
        "}"
        "QPushButton:hover {"
        "   background-color: %1;"
        "   border-radius: 4px;"
        "}"
        "QPushButton:pressed {"
        "   background-color: %2;"
        "}"
    ).arg(AppTheme::colors().bgHover, AppTheme::colors().bgBase));
    infoButton->setCursor(Qt::PointingHandCursor);
    infoButton->setToolTip("View technical details");
    connect(infoButton, &QPushButton::clicked, [this, infoButton]() {
        if (!m_infoPopup) {
            m_infoPopup = new DriverInfoPopup(m_driver, nullptr);
        }
        if (m_infoPopup->wasRecentlyClosed()) return;
        if (m_infoPopup->isVisible()) {
            m_infoPopup->hide();
        } else {
            m_infoPopup->showRelativeTo(infoButton);
        }
    });
    contentLayout->addWidget(infoButton);

    outerLayout->addWidget(content);
}

// ============================================================================
// Card Styling
// ============================================================================

void DriverCardWidget::applyCardStyle(const DriverInfo& driver)
{
    QString bg     = getCardBackground(driver);
    QString accent = getAccentColor(driver);

    // Healthy cards: near-invisible border so they recede visually
    // Warning cards: soften orange border opacity (-15%)
    // Critical cards: full red border to demand attention
    QString borderColor;
    bool hasIssue = !driver.healthFlags.empty() ||
                    driver.status == DriverStatus::Error;

    if (!hasIssue) {
        borderColor = AppTheme::colors().borderSubtle;
    } else if (accent == AppTheme::colors().warning) {
        borderColor = AppTheme::colors().warningBorder;   // orange at ~55% — was 100%
    } else {
        borderColor = accent;
    }

    setStyleSheet(QString(
        "DriverCardWidget {"
        "   background-color: %1;"
        "   border: 1px solid %2;"
        "   border-left: none;"
        "   border-radius: 10px;"
        "}"
    ).arg(bg, borderColor));
}

// ============================================================================
// Sub-widget Builders
// ============================================================================

QWidget* DriverCardWidget::createImportanceBadge(DriverImportance importance)
{
    QString label = importanceLabel(importance);
    QString color = importanceColor(importance);

    QLabel* badge = new QLabel(label);
    QFont f = badge->font();
    f.setPointSize(8);
    f.setBold(true);
    badge->setFont(f);
    badge->setStyleSheet(QString(
        "QLabel {"
        "   color: %1;"
        "   background-color: transparent;"
        "   border: 1px solid %1;"
        "   border-radius: 4px;"
        "   padding: 1px 7px;"
        "}"
    ).arg(color));

    return badge;
}

QWidget* DriverCardWidget::createHealthBlock(int score)
{
    QWidget* block = new QWidget();
    block->setFixedWidth(80);
    block->setStyleSheet("background: transparent;");

    QVBoxLayout* layout = new QVBoxLayout(block);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->setAlignment(Qt::AlignCenter);

    // Large bold percentage
    m_healthPercentLabel = new QLabel(QString("%1%").arg(score));
    QFont pctFont = m_healthPercentLabel->font();
    pctFont.setPointSize(15);
    pctFont.setBold(true);
    m_healthPercentLabel->setFont(pctFont);
    m_healthPercentLabel->setAlignment(Qt::AlignCenter);
    m_healthPercentLabel->setStyleSheet(
        QString("color: %1; background: transparent;").arg(healthColor(score))
    );
    layout->addWidget(m_healthPercentLabel);

    // Slim progress bar — dark track, colored fill
    m_healthBar = new QProgressBar();
    m_healthBar->setRange(0, 100);
    m_healthBar->setValue(score);
    m_healthBar->setTextVisible(false);
    m_healthBar->setFixedHeight(6);
    m_healthBar->setFixedWidth(64);
    m_healthBar->setStyleSheet(QString(
        "QProgressBar {"
        "   border: none;"
        "   border-radius: 3px;"
        "   background-color: %1;"
        "}"
        "QProgressBar::chunk {"
        "   background-color: %2;"
        "   border-radius: 3px;"    // fully rounded — matches half of 6px height
        "}"
    ).arg(AppTheme::colors().progressTrack, healthColor(score)));
    layout->addWidget(m_healthBar, 0, Qt::AlignCenter);

    // "Health" sub-label
    QLabel* subLabel = new QLabel("Health");
    QFont subFont = subLabel->font();
    subFont.setPointSize(8);
    subLabel->setFont(subFont);
    subLabel->setAlignment(Qt::AlignCenter);
    subLabel->setStyleSheet(QString("color: %1; background: transparent;").arg(AppTheme::colors().textMuted));
    layout->addWidget(subLabel);

    return block;
}

// ============================================================================
// Color & Label Helpers
// ============================================================================

QString DriverCardWidget::getAccentColor(const DriverInfo& driver) const
{
    // Derive worst state from health flags and status
    bool hasCritical = false;
    bool hasWarning  = false;

    for (const auto& flag : driver.healthFlags) {
        if (flag.id == L"NOT_STARTED_BY_DESIGN" ||
            flag.id == L"DRIVER_NOT_LOADED_BY_DESIGN" ||
            flag.id == L"SYSTEM_CRITICAL") {
            continue;
        }
        if (flag.severity == HealthFlagSeverity::Critical) hasCritical = true;
        if (flag.severity == HealthFlagSeverity::Warning)  hasWarning  = true;
    }

    if (driver.status == DriverStatus::Error) hasCritical = true;

    if (hasCritical) return AppTheme::colors().critical;
    if (hasWarning)  return AppTheme::colors().warning;
    if (driver.healthScore < 80) return AppTheme::colors().caution;

    return AppTheme::colors().borderSubtle;   // Healthy: nearly invisible, just structural
}

QString DriverCardWidget::getCardBackground(const DriverInfo& driver) const
{
    QString accent = getAccentColor(driver);

    if (accent == AppTheme::colors().critical) return "rgba(231, 76, 60, 0.05)";
    if (accent == AppTheme::colors().warning)  return "rgba(243, 156, 18, 0.03)";   // reduced from 0.04
    if (accent == AppTheme::colors().caution)  return "rgba(241, 196, 15, 0.03)";

    return AppTheme::colors().bgCard;   // Standard healthy card background
}

QString DriverCardWidget::importanceColor(DriverImportance importance)
{
    switch (importance) {
        case DriverImportance::Critical:  return AppTheme::colors().critical;
        case DriverImportance::Important: return AppTheme::colors().warning;
        case DriverImportance::Optional:  return AppTheme::colors().healthy;
        default:                          return AppTheme::colors().accent;   // Virtual / unknown → blue
    }
}

QString DriverCardWidget::importanceLabel(DriverImportance importance)
{
    switch (importance) {
        case DriverImportance::Critical:  return "Critical";
        case DriverImportance::Important: return "Important";
        case DriverImportance::Optional:  return "Optional";
        default:                          return "Virtual";
    }
}

QString DriverCardWidget::healthColor(int score)
{
    if (score >= 80) return AppTheme::colors().healthy;
    if (score >= 50) return AppTheme::colors().warning;
    return AppTheme::colors().critical;
}

QString DriverCardWidget::statusColor(DriverStatus status)
{
    switch (status) {
        case DriverStatus::Ok:         return AppTheme::colors().healthy;
        case DriverStatus::Error:      return AppTheme::colors().critical;
        case DriverStatus::NotStarted: return AppTheme::colors().warning;
        case DriverStatus::Disabled:   return AppTheme::colors().textDisabled;
        default:                       return AppTheme::colors().textMuted;
    }
}