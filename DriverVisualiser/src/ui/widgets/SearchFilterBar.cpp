#include "SearchFilterBar.h"
#include "AppTheme.h"
#include "IconProvider.h"
#include <QIcon>

SearchFilterBar::SearchFilterBar(QWidget* parent)
    : QWidget(parent)
    , m_filterPopup(nullptr)
{
    setupUi();
    
    // Debounce timer for search (300ms)
    m_searchDebounceTimer = new QTimer(this);
    m_searchDebounceTimer->setSingleShot(true);
    m_searchDebounceTimer->setInterval(300);
    connect(m_searchDebounceTimer, &QTimer::timeout, this, [this]() {
        emit searchChanged(m_searchBox->text());
    });
    
    connect(m_searchBox, &QLineEdit::textChanged, this, &SearchFilterBar::onSearchTextChanged);
    connect(m_filterButton, &QPushButton::clicked, this, &SearchFilterBar::onFilterButtonClicked);
}

void SearchFilterBar::setupUi()
{
    // Main horizontal layout - search and filter on same row
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    
    // === Search Box - Clean, no icon ===
    m_searchBox = new QLineEdit();
    m_searchBox->setPlaceholderText("Search by manufacturer, name, device class...");
    m_searchBox->setClearButtonEnabled(true);
    m_searchBox->setStyleSheet(QString(
        "QLineEdit {"
        "   background-color: %1; "  // Slightly more opaque
        "   border: 1px solid %2; "
        "   border-radius: 10px; "                     // More rounded
        "   padding: 12px 18px; "                      // More padding
        "   font-size: 12px; "
        "   color: %3; "
        "}"
        "QLineEdit:focus {"
        "   background-color: %4; "               // Solid on focus
        "   border: 1px solid %5; "
        "}"
        "QLineEdit::placeholder {"
        "   color: %6; "
        "   font-style: italic; "
        "}"
    ).arg(AppTheme::colors().bgInput, AppTheme::colors().borderInput,
          AppTheme::colors().textPrimary, AppTheme::colors().bgInputFocus,
          AppTheme::colors().borderFocus, AppTheme::colors().textPlaceholder));
    layout->addWidget(m_searchBox, 1);  // Stretch to fill space
    
    // === Filter Button - Match sections style ===
    m_filterButton = new QPushButton("  Filters");
    m_filterButton->setIcon(IconProvider::icon(
        IconProvider::Settings,
        AppTheme::colors().textSecondary,
        20  // Good visible size
    ));
    m_filterButton->setIconSize(QSize(20, 20));
    m_filterButton->setStyleSheet(QString(
        "QPushButton {"
        "   background-color: %1; "
        "   border: 1px solid %2; "
        "   border-radius: 10px; "                     // Match search bar
        "   padding: 12px 24px; "                      // Match search bar padding
        "   font-size: 12px; "
        "   color: %3; "
        "   font-weight: bold; "
        "}"
        "QPushButton:hover {"
        "   background-color: %4; "
        "   border-color: %5; "
        "   color: %6; "
        "}"
        "QPushButton:pressed {"
        "   background-color: %7; "
        "}"
    ).arg(AppTheme::colors().bgElevated, AppTheme::colors().borderNormal,
          AppTheme::colors().textSecondary, AppTheme::colors().bgHover,
          AppTheme::colors().accent, AppTheme::colors().textPrimary,
          AppTheme::colors().bgBase));
    m_filterButton->setFixedWidth(140);  // Slightly wider for icon
    layout->addWidget(m_filterButton);
}

void SearchFilterBar::onSearchTextChanged()
{
    // Restart debounce timer
    m_searchDebounceTimer->start();
}

void SearchFilterBar::onFilterButtonClicked()
{
    // Create popup if not exists
    if (!m_filterPopup) {
        m_filterPopup = new FilterPopupWidget(m_manufacturers, this);
        connect(m_filterPopup, &FilterPopupWidget::filtersChanged,
                this, [this]() {
                    updateFilterBadge();
                    emit filtersChanged();
                });
    }
    
    // Toggle popup
    if (m_filterPopup->isVisible()) {
        m_filterPopup->hide();
    } else if (!m_filterPopup->wasRecentlyClosed()) {
        m_filterPopup->showRelativeTo(m_filterButton);
    }
}

QString SearchFilterBar::getSearchText() const
{
    return m_searchBox->text();
}

FilterPopupWidget::FilterState SearchFilterBar::getFilterState() const
{
    if (m_filterPopup) {
        return m_filterPopup->getFilterState();
    }
    return FilterPopupWidget::FilterState();  // Default: all checked
}

void SearchFilterBar::setManufacturers(const std::set<std::wstring>& manufacturers)
{
    m_manufacturers = manufacturers;
    if (m_filterPopup) {
        delete m_filterPopup;
        m_filterPopup = nullptr;
    }
    updateFilterBadge();
}

void SearchFilterBar::updateFilterBadge()
{
    int count = m_filterPopup ? m_filterPopup->activeFilterCount() : 0;
    if (count > 0) {
        m_filterButton->setText(QString("  Filters · %1").arg(count));
        m_filterButton->setStyleSheet(m_filterButton->styleSheet()
            .replace(AppTheme::colors().textSecondary, AppTheme::colors().accentText));
        // Restyle button to show active state
        m_filterButton->setStyleSheet(QString(
            "QPushButton {"
            "   background-color: %1;"
            "   border: 1px solid %2;"
            "   border-radius: 10px;"
            "   padding: 12px 24px;"
            "   font-size: 12px;"
            "   color: %3;"
            "   font-weight: bold;"
            "}"
            "QPushButton:hover {"
            "   background-color: %4;"
            "   border-color: %5;"
            "}"
        ).arg(AppTheme::colors().bgButtonActive,
              AppTheme::colors().borderActive,
              AppTheme::colors().accentText,
              AppTheme::colors().accentBg,
              AppTheme::colors().accent));
    } else {
        m_filterButton->setText("  Filters");
        m_filterButton->setStyleSheet(QString(
            "QPushButton {"
            "   background-color: %1;"
            "   border: 1px solid %2;"
            "   border-radius: 10px;"
            "   padding: 12px 24px;"
            "   font-size: 12px;"
            "   color: %3;"
            "   font-weight: bold;"
            "}"
            "QPushButton:hover {"
            "   background-color: %4;"
            "   border-color: %5;"
            "   color: %6;"
            "}"
            "QPushButton:pressed { background-color: %7; }"
        ).arg(AppTheme::colors().bgElevated,
              AppTheme::colors().borderNormal,
              AppTheme::colors().textSecondary,
              AppTheme::colors().bgHover,
              AppTheme::colors().accent,
              AppTheme::colors().textPrimary,
              AppTheme::colors().bgBase));
    }
}