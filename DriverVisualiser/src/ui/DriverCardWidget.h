#pragma once

#include <QFrame>
#include <QLabel>
#include "DriverInfo.h"

class DriverCardWidget : public QFrame
{
    Q_OBJECT

public:
    explicit DriverCardWidget(const DriverInfo& driver, QWidget* parent = nullptr);

private:
    void setupUi(const DriverInfo& driver);
    
    // Creates the importance indicator (3 squares)
    QWidget* createImportanceIndicator(DriverImportance importance);
    
    // Returns color based on importance level
    QString getImportanceColor(DriverImportance importance);
    
    // Returns color based on driver status
    QString getStatusColor(DriverStatus status);

    QLabel* m_nameLabel;
    QLabel* m_versionLabel;
    QLabel* m_manufacturerLabel;
    QLabel* m_statusLabel;
    QLabel* m_faultScoreLabel;
};
