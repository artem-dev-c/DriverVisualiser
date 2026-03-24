#include "DayEventsPopup.h"
#include "AppTheme.h"
#include "IconProvider.h"
#include <QLabel>
#include <QDateTime>
#include <QCloseEvent>
#include <QApplication>
#include <QScreen>
#include <QHBoxLayout>
#include <QPainter>
#include <algorithm>
#include <chrono>
#include <unordered_map>

// ============================================================================
// Construction
// ============================================================================

DayEventsPopup::DayEventsPopup(const QDate& date,
                               const std::vector<ErrorLogEntry>& events,
                               QWidget* parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
    , m_date(date)
{
    setAttribute(Qt::WA_DeleteOnClose);

    // Sort chronologically for correct chain gap calculation
    std::vector<ErrorLogEntry> sorted = events;
    std::sort(sorted.begin(), sorted.end(),
        [](const ErrorLogEntry& a, const ErrorLogEntry& b) {
            return a.timestamp < b.timestamp;
        });

    // Split hardware events from SWD background events using category field
    for (const auto& entry : sorted) {
        if (entry.category == ErrorLogEntry::Category::SwdBackground) {
            m_swdEvents.push_back(entry);
        } else {
            // Includes BOTH DeviceMapped AND SystemWide events
            m_events.push_back(entry);
        }
    }

    // Build chains on chronological order (gap calculation requires it)
    buildChains();

    // Reverse m_events and parallel chain vectors for newest-first display
    std::reverse(m_events.begin(), m_events.end());
    std::reverse(m_chainPos.begin(), m_chainPos.end());
    std::reverse(m_chainColors.begin(), m_chainColors.end());

    // Flip First/Last tags after reversal so thread draws correctly
    for (auto& pos : m_chainPos) {
        if      (pos == ChainPos::First) pos = ChainPos::Last;
        else if (pos == ChainPos::Last)  pos = ChainPos::First;
    }
    setupUi();
    populateEvents();
    populateSwdSection();

    m_closeTimer.setSingleShot(true);
    connect(&m_closeTimer, &QTimer::timeout, this, [this]() {
        m_recentlyClosed = false;
    });
}

// ============================================================================
// Chain pre-processing
// ============================================================================

void DayEventsPopup::buildChains()
{
    const int n = static_cast<int>(m_events.size());
    m_chainPos.assign(n, ChainPos::None);
    m_chainColors.assign(n, QString());

    static constexpr int GAP_SECONDS = 30;

    // Valid chain key: must be a full BUS\DEVICE\INSTANCE id (two backslashes min).
    // Two-part IDs like "USB\VID_1234&PID_5678" are shared across all children
    // of a USB composite device and must not be used as chain keys.
    auto isValidChainKey = [](const std::wstring& id) -> bool {
        if (id.empty()) return false;
        std::wstring lower = id;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
        if (lower.size() >= 4 && lower.substr(lower.size() - 4) == L".inf") return false;
        if (lower.find(L"\\driver\\") != std::wstring::npos) return false;
        // Require at least two backslashes
        size_t first = lower.find(L'\\');
        if (first == std::wstring::npos) return false;
        return lower.find(L'\\', first + 1) != std::wstring::npos;
    };

    // Assign a unique chain key per device per continuous session
    std::unordered_map<std::wstring, std::chrono::system_clock::time_point> lastSeen;
    std::unordered_map<std::wstring, int> chainInstance;
    std::vector<std::wstring> eventChainKey(n);

    for (int i = 0; i < n; ++i) {
        const auto& entry = m_events[i];
        if (!entry.deviceInstanceId.has_value()
                || !isValidChainKey(entry.deviceInstanceId.value())) {
            eventChainKey[i] = L"";
            continue;
        }

        std::wstring id = entry.deviceInstanceId.value();
        std::transform(id.begin(), id.end(), id.begin(), ::towlower);

        auto it = lastSeen.find(id);
        if (it != lastSeen.end()) {
            auto gap = std::chrono::duration_cast<std::chrono::seconds>(
                entry.timestamp - it->second).count();
            if (gap > GAP_SECONDS) chainInstance[id]++;
            it->second = entry.timestamp;
        } else {
            lastSeen[id]      = entry.timestamp;
            chainInstance[id] = 0;
        }
        eventChainKey[i] = id + L"_" + std::to_wstring(chainInstance[id]);
    }

    // Count events per chain key
    std::unordered_map<std::wstring, int> chainSize;
    for (int i = 0; i < n; ++i)
        if (!eventChainKey[i].empty())
            chainSize[eventChainKey[i]]++;

    // Locate first/last index per chain
    std::unordered_map<std::wstring, int> chainFirst, chainLast;
    for (int i = 0; i < n; ++i) {
        const auto& key = eventChainKey[i];
        if (key.empty() || chainSize[key] < 2) continue;
        if (!chainFirst.count(key)) chainFirst[key] = i;
        chainLast[key] = i;
    }

    // Assign ChainPos — all chains use the app accent colour
    for (int i = 0; i < n; ++i) {
        const auto& key = eventChainKey[i];
        if (key.empty() || chainSize[key] < 2) {
            m_chainPos[i]    = ChainPos::None;
            m_chainColors[i] = QString();
            continue;
        }
        m_chainColors[i] = AppTheme::colors().accent;
        int first = chainFirst[key], last = chainLast[key];
        if      (i == first) m_chainPos[i] = ChainPos::First;
        else if (i == last)  m_chainPos[i] = ChainPos::Last;
        else                 m_chainPos[i] = ChainPos::Middle;
    }
}

// ============================================================================
// UI Setup
// ============================================================================

void DayEventsPopup::setupUi()
{
    setFixedSize(520, 580);

    setStyleSheet(QString(
        "DayEventsPopup {"
        "   background-color: %1;"
        "   border: 1px solid %2;"
        "   border-radius: 0px;"
        "}"
    ).arg(AppTheme::colors().bgOverlay, AppTheme::colors().borderStrong));

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(1, 1, 1, 1);
    mainLayout->setSpacing(0);

    // ── Header ────────────────────────────────────────────────────────────────
    QWidget* header = new QWidget();
    header->setStyleSheet("background: transparent;");
    QVBoxLayout* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(20, 16, 20, 12);
    headerLayout->setSpacing(6);

    QLabel* titleLabel = new QLabel(m_date.toString("MMMM d, yyyy"));
    {
        QFont f = titleLabel->font();
        f.setPointSize(13);
        f.setWeight(QFont::Bold);
        titleLabel->setFont(f);
        titleLabel->setStyleSheet(QString("color: %1; background: transparent;")
            .arg(AppTheme::colors().textPrimary));
    }
    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(createStatsRow());
    mainLayout->addWidget(header);

    // Thin divider
    QFrame* divider = new QFrame();
    divider->setFixedHeight(1);
    divider->setStyleSheet(QString("background-color: %1;")
        .arg(AppTheme::colors().borderSubtle));
    mainLayout->addWidget(divider);

    // ── Scroll area ───────────────────────────────────────────────────────────
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setStyleSheet(QString(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 0px; }"
        "QScrollBar::handle:vertical { background: %1; border-radius: 3px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: %2; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
    ).arg(AppTheme::colors().borderNormal, AppTheme::colors().borderStrong));

    m_contentWidget = new QWidget();
    m_contentWidget->setStyleSheet("background: transparent;");
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(16, 12, 12, 16);
    m_contentLayout->setSpacing(0);

    m_scrollArea->setWidget(m_contentWidget);
    mainLayout->addWidget(m_scrollArea, 1);
}

// ============================================================================
// Stats row
// ============================================================================

QWidget* DayEventsPopup::createStatsRow() const
{
    int nCritical = 0, nWarning = 0, nInfo = 0;
    for (const auto& e : m_events) {
        if      (e.level == L"Critical" || e.level == L"Error") nCritical++;
        else if (e.level == L"Warning")                          nWarning++;
        else                                                     nInfo++;
    }

    QWidget* row = new QWidget();
    row->setStyleSheet("background: transparent;");
    QHBoxLayout* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto makeStat = [](const QString& text, const QString& color) -> QLabel* {
        QLabel* lbl = new QLabel(text);
        QFont f = lbl->font(); f.setPointSize(10); lbl->setFont(f);
        lbl->setStyleSheet(QString("color: %1; background: transparent;").arg(color));
        return lbl;
    };
    auto makeSep = [](const QString& color) -> QLabel* {
        QLabel* sep = new QLabel("·");
        QFont f = sep->font(); f.setPointSize(10); sep->setFont(f);
        sep->setStyleSheet(QString("color: %1; background: transparent;").arg(color));
        return sep;
    };

    bool any = false;
    if (nCritical > 0) {
        if (any) layout->addWidget(makeSep(AppTheme::colors().textMuted));
        layout->addWidget(makeStat(QString("%1 Critical").arg(nCritical), "#e74c3c"));
        any = true;
    }
    if (nWarning > 0) {
        if (any) layout->addWidget(makeSep(AppTheme::colors().textMuted));
        layout->addWidget(makeStat(
            QString("%1 Warning%2").arg(nWarning).arg(nWarning > 1 ? "s" : ""), "#f39c12"));
        any = true;
    }
    if (nInfo > 0) {
        if (any) layout->addWidget(makeSep(AppTheme::colors().textMuted));
        layout->addWidget(makeStat(QString("%1 Info").arg(nInfo), "#5dade2"));
        any = true;
    }
    if (!m_swdEvents.empty()) {
        if (any) layout->addWidget(makeSep(AppTheme::colors().textMuted));
        layout->addWidget(makeStat(
            QString("%1 background").arg(m_swdEvents.size()),
            AppTheme::colors().textMuted));
    }
    layout->addStretch();
    return row;
}

// ============================================================================
// Populate hardware events
// ============================================================================

void DayEventsPopup::populateEvents()
{
    if (m_events.empty()) {
        QLabel* empty = new QLabel("No hardware driver events on this day.");
        QFont f = empty->font(); f.setPointSize(10); empty->setFont(f);
        empty->setStyleSheet(QString("color: %1; background: transparent;")
            .arg(AppTheme::colors().textMuted));
        empty->setAlignment(Qt::AlignCenter);
        m_contentLayout->addSpacing(20);
        m_contentLayout->addWidget(empty);
        m_contentLayout->addSpacing(20);
        return;
    }

    // Build render order: chained events are grouped together (all events of a
    // chain rendered consecutively), positioned where the chain's first event
    // falls chronologically. Non-chained events keep their chronological slot.
    const int n = static_cast<int>(m_events.size());
    std::vector<bool> rendered(n, false);
    std::vector<int> renderOrder;
    renderOrder.reserve(n);

    // Group chained events by their First index
    std::unordered_map<int, std::vector<int>> chainGroups;  // firstIdx -> [all indices]

    for (int i = 0; i < n; ++i) {
        if (m_chainPos[i] == ChainPos::First) {
            chainGroups[i] = { i };
        } else if (m_chainPos[i] == ChainPos::Middle || m_chainPos[i] == ChainPos::Last) {
            // Find the First for this chain — same deviceInstanceId scanning backward
            const auto& myId = m_events[i].deviceInstanceId;
            for (int j = i - 1; j >= 0; --j) {
                if (m_chainPos[j] == ChainPos::First
                        && m_events[j].deviceInstanceId == myId) {
                    chainGroups[j].push_back(i);
                    break;
                }
            }
        }
    }

    for (int i = 0; i < n; ++i) {
        if (rendered[i]) continue;

        if (m_chainPos[i] == ChainPos::First) {
            // Render the entire chain as a consecutive block
            for (int idx : chainGroups[i]) {
                renderOrder.push_back(idx);
                rendered[idx] = true;
            }
        } else if (m_chainPos[i] == ChainPos::None) {
            renderOrder.push_back(i);
            rendered[i] = true;
        }
        // Middle/Last already rendered as part of their chain's First block
    }

    for (int idx : renderOrder)
        m_contentLayout->addWidget(createEventCard(idx));

    m_contentLayout->addSpacing(8);
}

// ============================================================================
// Thread marker — painted via eventFilter
// ============================================================================

QWidget* DayEventsPopup::createThreadMarker(ChainPos pos, const QString& color) const
{
    static constexpr int MARKER_WIDTH = 20;
    static constexpr int DOT_RADIUS   = 4;

    QWidget* marker = new QWidget();
    marker->setFixedWidth(MARKER_WIDTH);
    marker->setStyleSheet("background: transparent;");
    marker->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    marker->setProperty("chainColor", color);
    marker->setProperty("chainPos",   static_cast<int>(pos));
    marker->setProperty("dotRadius",  DOT_RADIUS);
    marker->installEventFilter(const_cast<DayEventsPopup*>(this));
    return marker;
}

// ============================================================================
// Event card
// ============================================================================

QWidget* DayEventsPopup::createEventCard(int eventIndex)
{
    const ErrorLogEntry& entry = m_events[eventIndex];
    ChainPos             pos   = m_chainPos[eventIndex];
    const QString&       color = m_chainColors[eventIndex];

    QWidget* row = new QWidget();
    row->setStyleSheet("background: transparent;");
    QHBoxLayout* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 2, 0, 2);
    rowLayout->setSpacing(6);

    rowLayout->addWidget(createThreadMarker(pos, color));

    QFrame* card = new QFrame();
    card->setObjectName("eventCard");
    card->setStyleSheet(QString(
        "QFrame#eventCard { background-color: %1; border: 1px solid %2; border-radius: 6px; }"
        "QFrame#eventCard QLabel { border: none; }"
    ).arg(AppTheme::colors().bgCard, AppTheme::colors().borderNormal));

    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(12, 9, 12, 9);
    cardLayout->setSpacing(4);

    // Top row: timestamp · level · event id
    QHBoxLayout* topRow = new QHBoxLayout();
    topRow->setSpacing(8);
    topRow->setContentsMargins(0, 0, 0, 0);

    // Timestamp — monospace, prominent
    QLabel* timeLabel = new QLabel(formatTime(entry));
    {
        QFont f = timeLabel->font();
        f.setPointSize(11); f.setWeight(QFont::Bold);
        f.setFamily("Consolas, Courier New, monospace");
        timeLabel->setFont(f);
        timeLabel->setStyleSheet(QString("color: %1; background: transparent;")
            .arg(AppTheme::colors().textValue));
    }
    topRow->addWidget(timeLabel);

    // Level — coloured badge with background, matching ErrorLogPopupWidget style
    QLabel* levelLabel = new QLabel(QString::fromStdWString(entry.level));
    {
        QFont f = levelLabel->font();
        f.setPointSize(9); f.setWeight(QFont::Bold);
        levelLabel->setFont(f);
        QString bgColor;
        if (entry.level == L"Critical" || entry.level == L"Error")
            bgColor = "rgba(231, 76, 60, 0.2)";
        else if (entry.level == L"Warning")
            bgColor = "rgba(243, 156, 18, 0.2)";
        else
            bgColor = "rgba(93, 173, 226, 0.2)";
        levelLabel->setStyleSheet(QString(
            "color: %1; background-color: %2;"
            "border-radius: 4px; padding: 3px 8px;"
        ).arg(getSeverityColor(entry.level), bgColor));
    }
    topRow->addWidget(levelLabel);

    // Event ID — muted
    if (entry.eventId != 0) {
        QLabel* idLabel = new QLabel(QString("Event %1").arg(entry.eventId));
        QFont f = idLabel->font(); f.setPointSize(9); idLabel->setFont(f);
        idLabel->setStyleSheet(QString("color: %1; background: transparent;")
            .arg(AppTheme::colors().textMuted));
        topRow->addWidget(idLabel);
    }
    topRow->addStretch();
    cardLayout->addLayout(topRow);

    // System-wide badge (for events without device mapping)
    if (entry.category == ErrorLogEntry::Category::SystemWide) {
        QHBoxLayout* badgeRow = new QHBoxLayout();
        badgeRow->setSpacing(6);
        badgeRow->setContentsMargins(0, 0, 0, 0);
        
        // Info icon
        QLabel* iconLabel = new QLabel();
        iconLabel->setPixmap(IconProvider::icon(IconProvider::InfoCircle, 
                                                AppTheme::colors().textMuted, 14)
                             .pixmap(14, 14));
        iconLabel->setStyleSheet("background: transparent;");
        badgeRow->addWidget(iconLabel);
        
        // Badge text
        QLabel* badgeLabel = new QLabel("System-wide event");
        QFont f = badgeLabel->font(); 
        f.setPointSize(9); 
        f.setItalic(true);
        badgeLabel->setFont(f);
        badgeLabel->setStyleSheet(QString("color: %1; background: transparent;")
            .arg(AppTheme::colors().textMuted));
        badgeRow->addWidget(badgeLabel);
        badgeRow->addStretch();
        
        cardLayout->addLayout(badgeRow);
    }
    // Driver name (only for DeviceMapped events)
    else if (entry.driverName.has_value() && !entry.driverName.value().empty()) {
        QLabel* nameLabel = new QLabel(QString::fromStdWString(entry.driverName.value()));
        QFont f = nameLabel->font(); f.setPointSize(10); f.setWeight(QFont::DemiBold);
        nameLabel->setFont(f);
        nameLabel->setStyleSheet(QString("color: %1; background: transparent;")
            .arg(AppTheme::colors().textPrimary));
        cardLayout->addWidget(nameLabel);
    }

    // Message
    if (!entry.message.empty()) {
        QString msg = QString::fromStdWString(entry.message);
        if (msg.length() > 300) msg = msg.left(297) + "...";
        QLabel* msgLabel = new QLabel(msg);
        msgLabel->setWordWrap(true);
        QFont f = msgLabel->font(); f.setPointSize(10); msgLabel->setFont(f);
        msgLabel->setStyleSheet(QString("color: %1; background: transparent;")
            .arg(AppTheme::colors().textSecondary));
        cardLayout->addWidget(msgLabel);
    }

    rowLayout->addWidget(card, 1);
    return row;
}

// ============================================================================
// SWD section
// ============================================================================

void DayEventsPopup::populateSwdSection()
{
    if (m_swdEvents.empty()) return;

    QWidget* swdHeader = new QWidget();
    swdHeader->setStyleSheet(QString(
        "QWidget { background-color: %1; border-radius: 4px; }"
        "QWidget:hover { background-color: %2; }"
    ).arg(AppTheme::colors().bgElevated, AppTheme::colors().bgHover));
    swdHeader->setCursor(Qt::PointingHandCursor);

    QHBoxLayout* hl = new QHBoxLayout(swdHeader);
    hl->setContentsMargins(10, 8, 10, 8);
    hl->setSpacing(8);

    QLabel* chevron = new QLabel();
    chevron->setFixedSize(16, 16);
    chevron->setPixmap(IconProvider::icon(IconProvider::ChevronRight,
        AppTheme::colors().textMuted, 16).pixmap(16, 16));
    chevron->setStyleSheet("background: transparent;");
    hl->addWidget(chevron);

    QLabel* title = new QLabel(
        QString("Software background activity (%1 events)").arg(m_swdEvents.size()));
    { QFont f = title->font(); f.setPointSize(10); title->setFont(f);
      title->setStyleSheet(QString("color: %1; background: transparent;")
          .arg(AppTheme::colors().textSecondary)); }
    hl->addWidget(title);
    hl->addStretch();

    m_contentLayout->addSpacing(4);
    m_contentLayout->addWidget(swdHeader);

    m_swdContent = new QWidget();
    m_swdContent->setStyleSheet("background: transparent;");
    m_swdContent->setVisible(false);

    QVBoxLayout* swdLayout = new QVBoxLayout(m_swdContent);
    swdLayout->setContentsMargins(10, 4, 4, 4);
    swdLayout->setSpacing(0);
    for (const auto& entry : m_swdEvents)
        swdLayout->addWidget(createSwdRow(entry));

    m_contentLayout->addWidget(m_swdContent);
    m_contentLayout->addStretch();

    swdHeader->setProperty("chevron",    QVariant::fromValue(static_cast<QObject*>(chevron)));
    swdHeader->setProperty("swdContent", QVariant::fromValue(static_cast<QObject*>(m_swdContent)));
    swdHeader->installEventFilter(this);
}

// ============================================================================
// SWD compact row
// ============================================================================

QWidget* DayEventsPopup::createSwdRow(const ErrorLogEntry& entry) const
{
    QWidget* row = new QWidget();
    row->setStyleSheet("background: transparent;");
    QHBoxLayout* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(0);

    QString text = formatTime(entry);
    if (entry.eventId != 0)
        text += QString("  ·  Event %1").arg(entry.eventId);
    if (entry.driverName.has_value() && !entry.driverName.value().empty())
        text += "  ·  " + QString::fromStdWString(entry.driverName.value());

    QLabel* label = new QLabel(text);
    QFont f = label->font(); f.setPointSize(9);
    f.setFamily("Consolas, Courier New, monospace");
    label->setFont(f);
    label->setStyleSheet(QString("color: %1; background: transparent;")
        .arg(AppTheme::colors().textMuted));
    layout->addWidget(label);
    layout->addStretch();
    return row;
}

// ============================================================================
// Event filter — thread marker paint + SWD toggle
// ============================================================================

bool DayEventsPopup::eventFilter(QObject* obj, QEvent* ev)
{
    // Thread marker paint
    if (ev->type() == QEvent::Paint) {
        QWidget* w = qobject_cast<QWidget*>(obj);
        if (w && w->property("chainPos").isValid()) {
            auto pos      = static_cast<ChainPos>(w->property("chainPos").toInt());
            QString color = w->property("chainColor").toString();
            int dotR      = w->property("dotRadius").toInt();

            if (pos == ChainPos::None || color.isEmpty())
                return false;

            QPainter painter(w);
            painter.setRenderHint(QPainter::Antialiasing);

            int cx = w->width() / 2;
            int cy = w->height() / 2;

            QColor lineColor(color);
            lineColor.setAlpha(160);
            painter.setPen(QPen(lineColor, 1.5));
            painter.setBrush(Qt::NoBrush);

            if (pos == ChainPos::First || pos == ChainPos::Middle)
                painter.drawLine(cx, cy, cx, w->height());
            if (pos == ChainPos::Last || pos == ChainPos::Middle)
                painter.drawLine(cx, 0, cx, cy);

            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(color));
            painter.drawEllipse(QPoint(cx, cy), dotR, dotR);
            return true;
        }
    }

    // SWD toggle
    if (ev->type() == QEvent::MouseButtonRelease) {
        QLabel*  chevron    = qobject_cast<QLabel*>(
            qvariant_cast<QObject*>(obj->property("chevron")));
        QWidget* swdContent = qobject_cast<QWidget*>(
            qvariant_cast<QObject*>(obj->property("swdContent")));
        if (chevron && swdContent) {
            bool nowVisible = !swdContent->isVisible();
            swdContent->setVisible(nowVisible);
            auto iconType = nowVisible ? IconProvider::ChevronDown : IconProvider::ChevronRight;
            chevron->setPixmap(IconProvider::icon(iconType,
                AppTheme::colors().textMuted, 16).pixmap(16, 16));
            return true;
        }
    }

    return QFrame::eventFilter(obj, ev);
}

// ============================================================================
// Helpers
// ============================================================================

QString DayEventsPopup::formatTime(const ErrorLogEntry& entry) const
{
    auto timeT = std::chrono::system_clock::to_time_t(entry.timestamp);
    return QDateTime::fromSecsSinceEpoch(timeT).toString("HH:mm:ss");
}

QString DayEventsPopup::getSeverityColor(const std::wstring& level) const
{
    if (level == L"Critical" || level == L"Error") return "#e74c3c";
    if (level == L"Warning")                        return "#f39c12";
    return "#5dade2";
}

QString DayEventsPopup::getSeverityEmoji(const std::wstring& level) const
{
    if (level == L"Critical" || level == L"Error") return "🔴";
    if (level == L"Warning")                        return "🟠";
    return "🔵";
}

// ============================================================================
// Show / Close
// ============================================================================

void DayEventsPopup::showRelativeTo(QWidget* anchor)
{
    if (!anchor) { show(); return; }

    QPoint anchorPos = anchor->mapToGlobal(QPoint(0, 0));
    QScreen* screen  = QApplication::screenAt(anchorPos);
    QRect sg         = screen ? screen->geometry() : QRect();

    int popupX = anchorPos.x() + anchor->width() + 10;
    int popupY = anchorPos.y();

    if (popupX + width() > sg.right())
        popupX = anchorPos.x() - width() - 10;
    if (popupY + height() > sg.bottom())
        popupY = sg.bottom() - height() - 20;
    if (popupY < sg.top())
        popupY = sg.top() + 20;

    move(popupX, popupY);
    show();
    raise();
}

void DayEventsPopup::closeEvent(QCloseEvent* event)
{
    m_recentlyClosed = true;
    m_closeTimer.start(300);
    QFrame::closeEvent(event);
}

bool DayEventsPopup::wasRecentlyClosed() const
{
    return m_recentlyClosed;
}