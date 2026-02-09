#pragma once

#include <QFrame>
#include <QLabel>
#include <QProgressBar>
#include "DriverInfo.h"

class FlagIndicatorWidget;

class DriverCardWidget : public QFrame
{
    Q_OBJECT

public:
    explicit DriverCardWidget(const DriverInfo& driver, QWidget* parent = nullptr);

private:
    void setupUi(const DriverInfo& driver);
    
    // Creates the importance indicator (3 squares)
    QWidget* createImportanceIndicator(DriverImportance importance);
    
    // Creates the health bar (0-100)
    QWidget* createHealthBar(int healthScore);
    
    // Returns color based on importance level
    QString getImportanceColor(DriverImportance importance);
    
    // Returns color based on health score
    QString getHealthColor(int healthScore);
    
    // Returns color based on driver status
    QString getStatusColor(DriverStatus status);

    QLabel* m_nameLabel;
    QLabel* m_versionLabel;
    QLabel* m_manufacturerLabel;
    QLabel* m_statusLabel;
    QLabel* m_driverDateLabel;
    QLabel* m_installDateLabel;
    QProgressBar* m_healthBar;
    QLabel* m_healthPercentLabel;
    FlagIndicatorWidget* m_flagIndicator;
};