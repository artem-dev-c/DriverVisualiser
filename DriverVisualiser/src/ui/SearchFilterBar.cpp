#include "SearchFilterBar.h"
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
    m_searchBox->setStyleSheet(
        "QLineEdit {"
        "   background-color: rgba(43, 43, 43, 0.7); "  // Slightly more opaque
        "   border: 1px solid #3d3d3d; "
        "   border-radius: 10px; "                     // More rounded
        "   padding: 12px 18px; "                      // More padding
        "   font-size: 12px; "
        "   color: #ffffff; "
        "}"
        "QLineEdit:focus {"
        "   background-color: #2b2b2b; "               // Solid on focus
        "   border: 1px solid #5dade2; "
        "}"
        "QLineEdit::placeholder {"
        "   color: #888888; "
        "   font-style: italic; "
        "}"
    );
    layout->addWidget(m_searchBox, 1);  // Stretch to fill space
    
    // === Filter Button - Match sections style ===
    m_filterButton = new QPushButton("⚙ Filters");
    m_filterButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #2b2b2b; "
        "   border: 1px solid #3d3d3d; "
        "   border-radius: 10px; "                     // Match search bar
        "   padding: 12px 24px; "                      // Match search bar padding
        "   font-size: 12px; "
        "   color: #cccccc; "
        "   font-weight: bold; "
        "}"
        "QPushButton:hover {"
        "   background-color: #353535; "
        "   border-color: #5dade2; "
        "   color: #ffffff; "
        "}"
        "QPushButton:pressed {"
        "   background-color: #1e1e1e; "
        "}"
    );
    m_filterButton->setFixedWidth(130);
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
                this, &SearchFilterBar::filtersChanged);
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
    
    // Recreate popup with new manufacturers
    if (m_filterPopup) {
        delete m_filterPopup;
        m_filterPopup = nullptr;
    }
}