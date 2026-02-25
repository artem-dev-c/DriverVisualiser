#pragma once

#include <QFrame>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include "DriverInfo.h"

class FlagIndicatorWidget;
class DriverInfoPopup;
class ErrorLogIndicatorWidget;

/// ============================================================================
/// DriverCardWidget
/// ============================================================================
/// Displays a single driver entry inside a CategorySectionWidget.
///
/// Layout (horizontal):
///   [Accent bar] | [Left: name + badge + metadata] | [Health score] | [ErrorLog] | [Flags] | [Info btn]
///
/// Visual design:
///   - Left-edge accent bar colored by worst health state
///   - Card background tinted very subtly for critical/warning drivers
///   - Importance replaced with text badge (Critical / Important / Optional / Virtual)
///   - Health score displayed as bold percentage + mini progress arc, not a tiny bar
///   - All rounded corners at 10px (matching search bar / section headers at 10-16px)
///   - Consistent dark-theme palette: #2b2b2b base, #3d3d3d borders
/// ============================================================================
class DriverCardWidget : public QFrame
{
    Q_OBJECT

public:
    explicit DriverCardWidget(const DriverInfo& driver, int logDays = 7, QWidget* parent = nullptr);

private:
    void setupUi(const DriverInfo& driver);
    void applyCardStyle(const DriverInfo& driver);

    /// Creates the text importance badge (replaces 3-square indicator)
    QWidget* createImportanceBadge(DriverImportance importance);

    /// Creates the health score block (large % + slim bar)
    QWidget* createHealthBlock(int healthScore);

    /// Returns the accent/tint color for a driver's worst health state
    QString getAccentColor(const DriverInfo& driver) const;

    /// Returns card background tint (very subtle) for critical/warning
    QString getCardBackground(const DriverInfo& driver) const;

    /// Returns color for importance level
    static QString importanceColor(DriverImportance importance);

    /// Returns color for health score
    static QString healthColor(int score);

    /// Returns color for driver status
    static QString statusColor(DriverStatus status);

    /// Returns display label for importance
    static QString importanceLabel(DriverImportance importance);

    // ── Widgets ──────────────────────────────────────────────────────────────
    QLabel*               m_nameLabel          = nullptr;
    QLabel*               m_versionLabel       = nullptr;
    QLabel*               m_manufacturerLabel  = nullptr;
    QLabel*               m_statusLabel        = nullptr;
    QLabel*               m_driverDateLabel    = nullptr;
    QLabel*               m_installDateLabel   = nullptr;
    QProgressBar*         m_healthBar          = nullptr;
    QLabel*               m_healthPercentLabel = nullptr;
    FlagIndicatorWidget*  m_flagIndicator      = nullptr;
    ErrorLogIndicatorWidget* m_errorLogIndicator = nullptr;
    DriverInfoPopup*      m_infoPopup          = nullptr;

    // ── Data ─────────────────────────────────────────────────────────────────
    DriverInfo m_driver;
    int        m_logDays = 7;
};