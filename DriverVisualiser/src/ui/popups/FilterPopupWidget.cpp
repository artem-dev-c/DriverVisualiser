#include "FilterPopupWidget.h"
#include "AppTheme.h"
#include <QApplication>
#include <QScreen>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QDateTime>
#include <QScrollArea>
#include <QFrame>
#include <QGridLayout>
#include <functional>

// ============================================================================
// Construction
// ============================================================================

FilterPopupWidget::FilterPopupWidget(const std::set<std::wstring>& allManufacturers,
                                     QWidget* parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setupUi(allManufacturers);
}

// ============================================================================
// UI Setup
// ============================================================================

void FilterPopupWidget::setupUi(const std::set<std::wstring>& allManufacturers)
{
    setFixedWidth(340);
    setMaximumHeight(520);

    setStyleSheet(QString(
        "FilterPopupWidget {"
        "   background-color: %1;"
        "   border: 1px solid %2;"
        "   border-radius: 0px;"
        "}"
    ).arg(AppTheme::colors().bgOverlay, AppTheme::colors().borderStrong));

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(1, 1, 1, 1);
    mainLayout->setSpacing(0);

    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(QString(
        "QScrollArea { background-color: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background-color: transparent; }"
    ) + AppTheme::scrollbarStyleSheet());

    QWidget* content = new QWidget();
    content->setStyleSheet("background-color: transparent;");

    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(0);

    // ── Health Status ─────────────────────────────────────────────────────────
    layout->addWidget(createSectionHeader("Health Status", [this]() {
        clearAllChips(m_healthChips);
        emit filtersChanged();
    }));
    layout->addSpacing(8);
    layout->addWidget(createChipRow({"Critical", "Warnings", "Healthy"}, m_healthChips));
    layout->addSpacing(14);

    // Separator
    auto makeSep = [&]() {
        QFrame* sep = new QFrame();
        sep->setFixedHeight(1);
        sep->setStyleSheet(QString("background-color: %1;").arg(AppTheme::colors().borderSubtle));
        return sep;
    };
    layout->addWidget(makeSep());
    layout->addSpacing(14);

    // ── Importance Level ──────────────────────────────────────────────────────
    layout->addWidget(createSectionHeader("Importance Level", [this]() {
        clearAllChips(m_importanceChips);
        emit filtersChanged();
    }));
    layout->addSpacing(8);
    layout->addWidget(createChipRow(
        {"Critical Devices", "Important", "Optional", "Virtual"},
        m_importanceChips));
    layout->addSpacing(14);

    // ── Manufacturer ──────────────────────────────────────────────────────────
    if (!allManufacturers.empty()) {
        layout->addWidget(makeSep());
        layout->addSpacing(14);

        layout->addWidget(createSectionHeader("Manufacturer", [this]() {
            clearAllManufacturers();
            emit filtersChanged();
        }));
        layout->addSpacing(8);

        // Search field
        QLineEdit* mfgSearch = new QLineEdit();
        mfgSearch->setPlaceholderText("Search manufacturers...");
        mfgSearch->setClearButtonEnabled(true);
        mfgSearch->setStyleSheet(QString(
            "QLineEdit {"
            "   background-color: %1;"
            "   border: 1px solid %2;"
            "   border-radius: 6px;"
            "   padding: 6px 10px;"
            "   font-size: 11px;"
            "   color: %3;"
            "}"
            "QLineEdit:focus { border-color: %4; }"
        ).arg(AppTheme::colors().bgInput, AppTheme::colors().borderInput,
              AppTheme::colors().textPrimary, AppTheme::colors().borderFocus));
        layout->addWidget(mfgSearch);
        layout->addSpacing(6);

        // Manufacturer checkboxes container
        m_mfgContainer = new QWidget();
        m_mfgContainer->setStyleSheet("background: transparent;");
        m_mfgLayout = new QVBoxLayout(m_mfgContainer);
        m_mfgLayout->setContentsMargins(0, 0, 0, 0);
        m_mfgLayout->setSpacing(2);

        for (const auto& mfg : allManufacturers) {
            QString label = QString::fromStdWString(mfg);
            QCheckBox* check = createCheckbox(label, false);
            m_manufacturerChecks[mfg] = check;
            m_mfgLayout->addWidget(check);
            connect(check, &QCheckBox::stateChanged, this, &FilterPopupWidget::filtersChanged);
        }

        layout->addWidget(m_mfgContainer);

        connect(mfgSearch, &QLineEdit::textChanged, this, &FilterPopupWidget::onManufacturerSearchChanged);
    }

    layout->addStretch();

    scrollArea->setWidget(content);
    mainLayout->addWidget(scrollArea);
}

// ============================================================================
// Section header with inline Clear link
// ============================================================================

QWidget* FilterPopupWidget::createSectionHeader(const QString& title,
                                                 std::function<void()> onClear)
{
    QWidget* row = new QWidget();
    row->setStyleSheet("background: transparent;");
    QHBoxLayout* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    QLabel* titleLabel = new QLabel(title);
    {
        QFont f = titleLabel->font();
        f.setPointSize(11);
        f.setWeight(QFont::DemiBold);
        titleLabel->setFont(f);
        titleLabel->setStyleSheet(QString("color: %1; background: transparent;")
            .arg(AppTheme::colors().textPrimary));
    }
    hl->addWidget(titleLabel);
    hl->addStretch();

    QPushButton* clearBtn = new QPushButton("Clear");
    {
        QFont f = clearBtn->font();
        f.setPointSize(10);
        clearBtn->setFont(f);
        clearBtn->setFlat(true);
        clearBtn->setCursor(Qt::PointingHandCursor);
        clearBtn->setStyleSheet(QString(
            "QPushButton { color: %1; background: transparent; border: none; padding: 0px; }"
            "QPushButton:hover { color: %2; }"
        ).arg(AppTheme::colors().textMuted, AppTheme::colors().accent));
    }
    connect(clearBtn, &QPushButton::clicked, this, [onClear]() { onClear(); });
    hl->addWidget(clearBtn);

    return row;
}

// ============================================================================
// Chip row
// ============================================================================

QWidget* FilterPopupWidget::createChipRow(const QStringList& labels,
                                           std::vector<QPushButton*>& chips)
{
    QWidget* row = new QWidget();
    row->setStyleSheet("background: transparent;");

    // Use a grid that wraps after every 3 items so chips always fit within
    // the popup width without horizontal overflow. For ≤3 labels a single
    // row is used; for 4 labels a 2×2 grid is used.
    const int cols = (labels.size() <= 3) ? labels.size() : 2;
    QGridLayout* grid = new QGridLayout(row);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);

    for (int i = 0; i < labels.size(); ++i) {
        QPushButton* chip = createChip(labels[i]);
        // Chips expand horizontally to fill their grid cell evenly
        chip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        chips.push_back(chip);
        grid->addWidget(chip, i / cols, i % cols);
        connect(chip, &QPushButton::clicked, this, [this, chip]() {
            chip->setProperty("active", !chip->property("active").toBool());
            updateChipStyle(chip);
            emit filtersChanged();
        });
    }
    return row;
}

QPushButton* FilterPopupWidget::createChip(const QString& label)
{
    QPushButton* chip = new QPushButton(label);
    chip->setProperty("active", false);
    chip->setCursor(Qt::PointingHandCursor);
    {
        QFont f = chip->font();
        f.setPointSize(10);
        chip->setFont(f);
    }
    updateChipStyle(chip);
    return chip;
}

void FilterPopupWidget::updateChipStyle(QPushButton* chip)
{
    bool active = chip->property("active").toBool();
    if (active) {
        chip->setStyleSheet(QString(
            "QPushButton {"
            "   background-color: %1;"
            "   color: %2;"
            "   border: 1px solid %3;"
            "   border-radius: 6px;"
            "   padding: 4px 12px;"
            "   font-weight: bold;"
            "}"
            "QPushButton:hover { background-color: %4; }"
        ).arg(AppTheme::colors().bgButtonActive,
              AppTheme::colors().accentText,
              AppTheme::colors().borderActive,
              AppTheme::colors().accentBg));
    } else {
        chip->setStyleSheet(QString(
            "QPushButton {"
            "   background-color: %1;"
            "   color: %2;"
            "   border: 1px solid %3;"
            "   border-radius: 6px;"
            "   padding: 4px 12px;"
            "}"
            "QPushButton:hover {"
            "   background-color: %4;"
            "   color: %5;"
            "   border-color: %6;"
            "}"
        ).arg(AppTheme::colors().bgButtonNeutral,
              AppTheme::colors().textSecondary,
              AppTheme::colors().borderNormal,
              AppTheme::colors().bgHover,
              AppTheme::colors().textPrimary,
              AppTheme::colors().borderStrong));
    }
}

// ============================================================================
// Manufacturer checkbox
// ============================================================================

QCheckBox* FilterPopupWidget::createCheckbox(const QString& label, bool checked)
{
    QCheckBox* check = new QCheckBox(label);
    check->setChecked(checked);
    check->setStyleSheet(QString(
        "QCheckBox { color: %1; font-size: 11px; spacing: 8px; background: transparent; }"
        "QCheckBox::indicator { width: 15px; height: 15px; border: 1px solid %2; border-radius: 3px; background-color: %3; }"
        "QCheckBox::indicator:checked { background-color: %4; border-color: %4; }"
        "QCheckBox:hover { color: %5; }"
    ).arg(AppTheme::colors().textSecondary, AppTheme::colors().borderStrong,
          AppTheme::colors().bgCodeBlock, AppTheme::colors().accent,
          AppTheme::colors().textPrimary));
    return check;
}

// ============================================================================
// Clear helpers
// ============================================================================

void FilterPopupWidget::clearAllChips(std::vector<QPushButton*>& chips)
{
    for (auto* chip : chips) {
        chip->setProperty("active", false);
        updateChipStyle(chip);
    }
}

void FilterPopupWidget::clearAllManufacturers()
{
    for (auto& [mfg, check] : m_manufacturerChecks) {
        check->blockSignals(true);
        check->setChecked(false);
        check->blockSignals(false);
    }
}

void FilterPopupWidget::onManufacturerSearchChanged(const QString& text)
{
    QString lower = text.toLower();
    for (auto& [mfg, check] : m_manufacturerChecks) {
        bool visible = lower.isEmpty() ||
                       QString::fromStdWString(mfg).toLower().contains(lower);
        check->setVisible(visible);
    }
}

// ============================================================================
// Filter state
// ============================================================================

int FilterPopupWidget::activeFilterCount() const
{
    int count = 0;
    for (auto* chip : m_healthChips)
        if (chip->property("active").toBool()) ++count;
    for (auto* chip : m_importanceChips)
        if (chip->property("active").toBool()) ++count;
    for (const auto& [mfg, check] : m_manufacturerChecks)
        if (check->isChecked()) ++count;
    return count;
}

FilterPopupWidget::FilterState FilterPopupWidget::getFilterState() const
{
    FilterState state;

    // Health chips: Critical=0, Warnings=1, Healthy=2
    if (m_healthChips.size() >= 3) {
        state.showCritical = m_healthChips[0]->property("active").toBool();
        state.showWarnings = m_healthChips[1]->property("active").toBool();
        state.showHealthy  = m_healthChips[2]->property("active").toBool();
    }

    // Importance chips: Critical=0, Important=1, Optional=2, Virtual=3
    if (m_importanceChips.size() >= 4) {
        state.showCriticalDevices  = m_importanceChips[0]->property("active").toBool();
        state.showImportantDevices = m_importanceChips[1]->property("active").toBool();
        state.showOptionalDevices  = m_importanceChips[2]->property("active").toBool();
        state.showVirtualDevices   = m_importanceChips[3]->property("active").toBool();
    }

    for (const auto& [mfg, check] : m_manufacturerChecks)
        if (check->isChecked())
            state.selectedManufacturers.insert(mfg);

    return state;
}

void FilterPopupWidget::setFilterState(const FilterState& state)
{
    if (m_healthChips.size() >= 3) {
        m_healthChips[0]->setProperty("active", state.showCritical);
        m_healthChips[1]->setProperty("active", state.showWarnings);
        m_healthChips[2]->setProperty("active", state.showHealthy);
        for (auto* c : m_healthChips) updateChipStyle(c);
    }
    if (m_importanceChips.size() >= 4) {
        m_importanceChips[0]->setProperty("active", state.showCriticalDevices);
        m_importanceChips[1]->setProperty("active", state.showImportantDevices);
        m_importanceChips[2]->setProperty("active", state.showOptionalDevices);
        m_importanceChips[3]->setProperty("active", state.showVirtualDevices);
        for (auto* c : m_importanceChips) updateChipStyle(c);
    }
    for (auto& [mfg, check] : m_manufacturerChecks)
        check->setChecked(state.selectedManufacturers.count(mfg) > 0);
}

// ============================================================================
// Show / Close
// ============================================================================

void FilterPopupWidget::showRelativeTo(QWidget* anchor)
{
    if (!anchor) return;

    QPoint pos = anchor->mapToGlobal(QPoint(anchor->width() - width(), anchor->height() + 4));

    QScreen* screen = QApplication::screenAt(pos);
    if (screen) {
        QRect screenRect = screen->availableGeometry();
        if (pos.x() + width() > screenRect.right())
            pos.setX(screenRect.right() - width() - 10);
        if (pos.x() < screenRect.left())
            pos.setX(screenRect.left() + 10);
        if (pos.y() + height() > screenRect.bottom())
            pos.setY(anchor->mapToGlobal(QPoint(0, 0)).y() - height() - 4);
    }

    move(pos);
    show();
    setFocus();
}

bool FilterPopupWidget::wasRecentlyClosed() const
{
    return (QDateTime::currentMSecsSinceEpoch() - m_closeTime) < 150;
}

void FilterPopupWidget::focusOutEvent(QFocusEvent* event)
{
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
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            hide();
            return true;
        }
    }
    return QFrame::event(event);
}