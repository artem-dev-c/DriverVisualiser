#include "FilterPopupWidget.h"
#include "AppTheme.h"
#include <QApplication>
#include <QScreen>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QDateTime>
#include <QScrollArea>

FilterPopupWidget::FilterPopupWidget(const std::set<std::wstring>& allManufacturers,
                                     QWidget* parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    
    setupUi(allManufacturers);
}

void FilterPopupWidget::setupUi(const std::set<std::wstring>& allManufacturers)
{
    setMinimumWidth(280);
    setMaximumWidth(320);
    setMaximumHeight(500);  // Increased for scrolling
    
    // Popup styling - match DriverInfoPopup exactly
    setStyleSheet(QString(
        "FilterPopupWidget {"
        "   background-color: %1;"
        "   border: 1px solid %2;"
        "   border-radius: 0px;"
        "}"
    ).arg(AppTheme::colors().bgOverlay, AppTheme::colors().borderStrong));
    
    // Use scroll area for long manufacturer lists
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(1, 1, 1, 1);  // Small margin for border outline
    mainLayout->setSpacing(0);
    
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(QString(
        "QScrollArea {"
        "   background-color: transparent;"
        "   border: none;"
        "   border-radius: 0px;"
        "}"
        "QScrollArea > QWidget > QWidget {"
        "   background-color: transparent;"
        "}"
    ) + AppTheme::scrollbarStyleSheet());
    
    QWidget* contentWidget = new QWidget();
    contentWidget->setStyleSheet("background-color: transparent;");
    
    QVBoxLayout* layout = new QVBoxLayout(contentWidget);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);
    
    // === Health Status Section ===
    layout->addWidget(createSection("Health Status"));
    m_criticalCheck = createCheckbox("Critical Issues", false);
    m_warningsCheck = createCheckbox("Warnings", false);
    m_healthyCheck = createCheckbox("Healthy", false);
    layout->addWidget(m_criticalCheck);
    layout->addWidget(m_warningsCheck);
    layout->addWidget(m_healthyCheck);
    
    layout->addSpacing(6);
    
    // === Importance Level Section ===
    layout->addWidget(createSection("Importance Level"));
    m_criticalDevicesCheck = createCheckbox("Critical Devices", false);
    m_importantDevicesCheck = createCheckbox("Important Devices", false);
    m_optionalDevicesCheck = createCheckbox("Optional Devices", false);
    m_virtualDevicesCheck = createCheckbox("Virtual Devices", false);
    layout->addWidget(m_criticalDevicesCheck);
    layout->addWidget(m_importantDevicesCheck);
    layout->addWidget(m_optionalDevicesCheck);
    layout->addWidget(m_virtualDevicesCheck);
    
    layout->addSpacing(6);
    
    // === Manufacturer Section - SHOW ALL ===
    if (!allManufacturers.empty()) {
        layout->addWidget(createSection("Manufacturer"));
        
        // Show ALL manufacturers (no limit)
        for (const auto& mfg : allManufacturers) {
            QString label = QString::fromStdWString(mfg);
            QCheckBox* check = createCheckbox(label, false);
            m_manufacturerChecks[mfg] = check;
            layout->addWidget(check);
        }
    }
    
    layout->addSpacing(8);
    
    // === Buttons row ===
    QHBoxLayout* buttonsLayout = new QHBoxLayout();
    buttonsLayout->setSpacing(8);
    
    // Clear All button
    QPushButton* clearAllBtn = new QPushButton("Clear All");
    clearAllBtn->setFixedHeight(32);
    clearAllBtn->setStyleSheet(QString(
        "QPushButton {"
        "   background-color: %1;"
        "   color: %2;"
        "   border: 1px solid %3;"
        "   border-radius: 4px;"
        "   padding: 6px 12px;"
        "   font-size: 11px;"
        "}"
        "QPushButton:hover {"
        "   background-color: %4;"
        "   color: %5;"
        "}"
        "QPushButton:pressed {"
        "   background-color: %6;"
        "}"
    ).arg(AppTheme::colors().bgButtonNeutral, AppTheme::colors().textSecondary,
          AppTheme::colors().borderStrong, AppTheme::colors().bgHover,
          AppTheme::colors().textPrimary, AppTheme::colors().bgBase));
    connect(clearAllBtn, &QPushButton::clicked, this, [this]() {
        // Block signals
        m_criticalCheck->blockSignals(true);
        m_warningsCheck->blockSignals(true);
        m_healthyCheck->blockSignals(true);
        m_criticalDevicesCheck->blockSignals(true);
        m_importantDevicesCheck->blockSignals(true);
        m_optionalDevicesCheck->blockSignals(true);
        m_virtualDevicesCheck->blockSignals(true);
        for (auto& [mfg, check] : m_manufacturerChecks) {
            check->blockSignals(true);
        }
        
        // Uncheck all
        m_criticalCheck->setChecked(false);
        m_warningsCheck->setChecked(false);
        m_healthyCheck->setChecked(false);
        m_criticalDevicesCheck->setChecked(false);
        m_importantDevicesCheck->setChecked(false);
        m_optionalDevicesCheck->setChecked(false);
        m_virtualDevicesCheck->setChecked(false);
        for (auto& [mfg, check] : m_manufacturerChecks) {
            check->setChecked(false);
        }
        
        // Unblock signals
        m_criticalCheck->blockSignals(false);
        m_warningsCheck->blockSignals(false);
        m_healthyCheck->blockSignals(false);
        m_criticalDevicesCheck->blockSignals(false);
        m_importantDevicesCheck->blockSignals(false);
        m_optionalDevicesCheck->blockSignals(false);
        m_virtualDevicesCheck->blockSignals(false);
        for (auto& [mfg, check] : m_manufacturerChecks) {
            check->blockSignals(false);
        }
        
        emit filtersChanged();
    });
    buttonsLayout->addWidget(clearAllBtn);
    
    // Select All button
    QPushButton* selectAllBtn = new QPushButton("Select All");
    selectAllBtn->setFixedHeight(32);
    selectAllBtn->setStyleSheet(QString(
        "QPushButton {"
        "   background-color: %1;"
        "   color: %2;"
        "   border: 1px solid %3;"
        "   border-radius: 4px;"
        "   padding: 6px 12px;"
        "   font-size: 11px;"
        "}"
        "QPushButton:hover {"
        "   background-color: %4;"
        "   color: %5;"
        "}"
        "QPushButton:pressed {"
        "   background-color: %6;"
        "}"
    ).arg(AppTheme::colors().bgButtonNeutral, AppTheme::colors().textSecondary,
          AppTheme::colors().borderStrong, AppTheme::colors().bgHover,
          AppTheme::colors().textPrimary, AppTheme::colors().bgBase));
    connect(selectAllBtn, &QPushButton::clicked, this, [this]() {
        // Block signals to prevent multiple filter updates
        m_criticalCheck->blockSignals(true);
        m_warningsCheck->blockSignals(true);
        m_healthyCheck->blockSignals(true);
        m_criticalDevicesCheck->blockSignals(true);
        m_importantDevicesCheck->blockSignals(true);
        m_optionalDevicesCheck->blockSignals(true);
        m_virtualDevicesCheck->blockSignals(true);
        for (auto& [mfg, check] : m_manufacturerChecks) {
            check->blockSignals(true);
        }
        
        // Check all
        m_criticalCheck->setChecked(true);
        m_warningsCheck->setChecked(true);
        m_healthyCheck->setChecked(true);
        m_criticalDevicesCheck->setChecked(true);
        m_importantDevicesCheck->setChecked(true);
        m_optionalDevicesCheck->setChecked(true);
        m_virtualDevicesCheck->setChecked(true);
        for (auto& [mfg, check] : m_manufacturerChecks) {
            check->setChecked(true);
        }
        
        // Unblock signals
        m_criticalCheck->blockSignals(false);
        m_warningsCheck->blockSignals(false);
        m_healthyCheck->blockSignals(false);
        m_criticalDevicesCheck->blockSignals(false);
        m_importantDevicesCheck->blockSignals(false);
        m_optionalDevicesCheck->blockSignals(false);
        m_virtualDevicesCheck->blockSignals(false);
        for (auto& [mfg, check] : m_manufacturerChecks) {
            check->blockSignals(false);
        }
        
        // Emit ONCE
        emit filtersChanged();
    });
    buttonsLayout->addWidget(selectAllBtn);
    
    layout->addLayout(buttonsLayout);
    
    layout->addStretch();
    
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);
    
    // Connect all checkboxes
    auto connectCheck = [this](QCheckBox* check) {
        connect(check, &QCheckBox::stateChanged, this, &FilterPopupWidget::filtersChanged);
    };
    
    connectCheck(m_criticalCheck);
    connectCheck(m_warningsCheck);
    connectCheck(m_healthyCheck);
    connectCheck(m_criticalDevicesCheck);
    connectCheck(m_importantDevicesCheck);
    connectCheck(m_optionalDevicesCheck);
    connectCheck(m_virtualDevicesCheck);
    for (auto& [mfg, check] : m_manufacturerChecks) {
        connectCheck(check);
    }
}

QWidget* FilterPopupWidget::createSection(const QString& title)
{
    QLabel* header = new QLabel(title);
    header->setStyleSheet(QString(
        "color: %1;"
        "font-weight: bold;"
        "font-size: 12px;"
        "padding-top: 4px;"
        "padding-bottom: 4px;"
    ).arg(AppTheme::colors().accentText));
    return header;
}

QCheckBox* FilterPopupWidget::createCheckbox(const QString& label, bool checked)
{
    QCheckBox* check = new QCheckBox(label);
    check->setChecked(checked);
    check->setStyleSheet(QString(
        "QCheckBox {"
        "   color: %1;"
        "   font-size: 11px;"
        "   spacing: 8px;"
        "}"
        "QCheckBox::indicator {"
        "   width: 16px;"
        "   height: 16px;"
        "   border: 1px solid %2;"
        "   border-radius: 3px;"
        "   background-color: %3;"
        "}"
        "QCheckBox::indicator:checked {"
        "   background-color: %4;"
        "   border-color: %4;"
        "}"
        "QCheckBox:hover {"
        "   color: %5;"
        "}"
    ).arg(AppTheme::colors().textSecondary, AppTheme::colors().borderStrong,
          AppTheme::colors().bgCodeBlock, AppTheme::colors().accent,
          AppTheme::colors().textPrimary));
    return check;
}

FilterPopupWidget::FilterState FilterPopupWidget::getFilterState() const
{
    FilterState state;
    
    // Health
    state.showCritical = m_criticalCheck->isChecked();
    state.showWarnings = m_warningsCheck->isChecked();
    state.showHealthy = m_healthyCheck->isChecked();
    
    // Importance
    state.showCriticalDevices = m_criticalDevicesCheck->isChecked();
    state.showImportantDevices = m_importantDevicesCheck->isChecked();
    state.showOptionalDevices = m_optionalDevicesCheck->isChecked();
    state.showVirtualDevices = m_virtualDevicesCheck->isChecked();
    
    // Manufacturers
    for (const auto& [mfg, check] : m_manufacturerChecks) {
        if (check->isChecked()) {
            state.selectedManufacturers.insert(mfg);
        }
    }
    
    return state;
}

void FilterPopupWidget::setFilterState(const FilterState& state)
{
    m_criticalCheck->setChecked(state.showCritical);
    m_warningsCheck->setChecked(state.showWarnings);
    m_healthyCheck->setChecked(state.showHealthy);
    
    m_criticalDevicesCheck->setChecked(state.showCriticalDevices);
    m_importantDevicesCheck->setChecked(state.showImportantDevices);
    m_optionalDevicesCheck->setChecked(state.showOptionalDevices);
    m_virtualDevicesCheck->setChecked(state.showVirtualDevices);
    
    for (auto& [mfg, check] : m_manufacturerChecks) {
        bool selected = state.selectedManufacturers.count(mfg) > 0;
        check->setChecked(selected);
    }
}

void FilterPopupWidget::showRelativeTo(QWidget* anchor)
{
    if (!anchor) return;
    
    // Position below and to the right of anchor
    QPoint pos = anchor->mapToGlobal(QPoint(anchor->width() - width(), anchor->height() + 4));
    
    // Adjust if would go off screen
    QScreen* screen = QApplication::screenAt(pos);
    if (screen) {
        QRect screenRect = screen->availableGeometry();
        
        if (pos.x() + width() > screenRect.right()) {
            pos.setX(screenRect.right() - width() - 10);
        }
        if (pos.x() < screenRect.left()) {
            pos.setX(screenRect.left() + 10);
        }
        if (pos.y() + height() > screenRect.bottom()) {
            pos.setY(anchor->mapToGlobal(QPoint(0, 0)).y() - height() - 4);
        }
    }
    
    move(pos);
    show();
    setFocus();
}

bool FilterPopupWidget::wasRecentlyClosed() const
{
    // Return true if closed within last 150ms
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    return (now - m_closeTime) < 150;
}

void FilterPopupWidget::focusOutEvent(QFocusEvent* event)
{
    // Qt::Popup flag handles closing on click-outside automatically
    // We don't need to do anything here
    QFrame::focusOutEvent(event);
}

void FilterPopupWidget::hideEvent(QHideEvent* event)
{
    Q_UNUSED(event);
    m_closeTime = QDateTime::currentMSecsSinceEpoch();
    QFrame::hideEvent(event);
}

bool FilterPopupWidget::event(QEvent* event)
{
    // Hide on escape key
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            hide();
            return true;
        }
    }
    return QFrame::event(event);
}