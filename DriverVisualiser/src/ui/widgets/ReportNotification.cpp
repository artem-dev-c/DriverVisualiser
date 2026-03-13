#include "ReportNotification.h"
#include "AppTheme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QApplication>
#include <QScreen>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>

ReportNotification::ReportNotification(
    const QString& filePath,
    const QString& reportText,
    QWidget* parent,
    bool isHtml
)
    : QFrame(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_filePath(filePath)
    , m_reportText(reportText)
    , m_isHtml(isHtml)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    // Remove translucent background - we want solid
    setAttribute(Qt::WA_TranslucentBackground, false);
    
    setupUi();
    
    // Auto-hide timer (starts on show())
    m_autoHideTimer = new QTimer(this);
    m_autoHideTimer->setSingleShot(true);
    connect(m_autoHideTimer, &QTimer::timeout, this, &ReportNotification::fadeOut);
    
    // Fade animation timer
    m_fadeTimer = new QTimer(this);
    connect(m_fadeTimer, &QTimer::timeout, this, [this]() {
        if (m_fadingOut) {
            m_opacity -= 0.05;
            if (m_opacity <= 0.0) {
                m_opacity = 0.0;
                m_fadeTimer->stop();
                close();
            }
        } else {
            m_opacity += 0.1;
            if (m_opacity >= 1.0) {
                m_opacity = 1.0;
                m_fadeTimer->stop();
            }
        }
        update();
    });
}

void ReportNotification::setupUi()
{
    // Larger, more comfortable size
    setMinimumSize(500, 130);
    setMaximumWidth(700);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    
    // Main styling (matching DriverInfoPopup)
    setStyleSheet(QString(
        "ReportNotification {"
        "   background-color: %1;"
        "   border: 1px solid %2;"
        "   border-radius: 8px;"
        "}"
    ).arg(AppTheme::colors().bgOverlay, AppTheme::colors().borderStrong));
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 12, 16, 12);
    mainLayout->setSpacing(8);
    
    // === Title ===
    m_titleLabel = new QLabel("✓ Report generated successfully");
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(11);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setStyleSheet(
        QString("color: %1;"
        "background: transparent;"
        "border: none;"
    ).arg(AppTheme::colors().accentText));
    mainLayout->addWidget(m_titleLabel);
    
    // === File path ===
    QFileInfo fileInfo(m_filePath);
    QString fileName = fileInfo.fileName();
    QString folderPath = fileInfo.absolutePath();
    
    // Show filename prominently
    QLabel* fileLabel = new QLabel(fileName);
    QFont fileFont = fileLabel->font();
    fileFont.setPointSize(10);
    fileFont.setBold(true);
    fileLabel->setFont(fileFont);
    fileLabel->setStyleSheet(
        QString("color: %1;"
        "background: transparent;"
        "border: none;"
    ).arg(AppTheme::colors().textPrimary));
    mainLayout->addWidget(fileLabel);
    
    // Show folder path below
    m_pathLabel = new QLabel("Location: " + folderPath);
    QFont pathFont = m_pathLabel->font();
    pathFont.setPointSize(8);
    m_pathLabel->setFont(pathFont);
    m_pathLabel->setStyleSheet(
        QString("color: %1;"
        "background: transparent;"
        "border: none;"
    ).arg(AppTheme::colors().textSecondary));
    m_pathLabel->setWordWrap(true);
    mainLayout->addWidget(m_pathLabel);
    
    mainLayout->addSpacing(4);
    
    // === Action buttons ===
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);
    
    // Copy to Clipboard button (only for text reports, not HTML)
    if (!m_isHtml) {
        m_copyButton = new QPushButton("Copy to Clipboard");
        m_copyButton->setFixedHeight(28);
        m_copyButton->setStyleSheet(QString(
            "QPushButton {"
            "   background-color: %1;"
            "   color: %2;"
            "   border: none;"
            "   border-radius: 4px;"
            "   font-size: 10px;"
            "   font-weight: bold;"
            "   padding: 6px 12px;"
            "}"
            "QPushButton:hover {"
            "   background-color: %3;"
            "}"
            "QPushButton:pressed {"
            "   background-color: %4;"
            "}"
        ).arg(AppTheme::colors().accent, AppTheme::colors().textOnAccent,
              AppTheme::colors().accentDark, AppTheme::colors().accentDarker));
        connect(m_copyButton, &QPushButton::clicked, this, &ReportNotification::copyToClipboard);
        buttonLayout->addWidget(m_copyButton);
    }
    
    // Open Folder button
    m_folderButton = new QPushButton("Open Folder");
    m_folderButton->setFixedHeight(28);
    m_folderButton->setStyleSheet(QString(
        "QPushButton {"
        "   background-color: %1;"
        "   color: %2;"
        "   border: none;"
        "   border-radius: 4px;"
        "   font-size: 10px;"
        "   font-weight: bold;"
        "   padding: 6px 12px;"
        "}"
        "QPushButton:hover {"
        "   background-color: %3;"
        "}"
        "QPushButton:pressed {"
        "   background-color: %4;"
        "}"
    ).arg(AppTheme::colors().bgButtonNeutral, AppTheme::colors().textSecondary,
          AppTheme::colors().bgHover, AppTheme::colors().bgBase));
    connect(m_folderButton, &QPushButton::clicked, this, &ReportNotification::openFolder);
    buttonLayout->addWidget(m_folderButton);
    
    buttonLayout->addStretch();
    
    mainLayout->addLayout(buttonLayout);
}

void ReportNotification::show(int durationMs)
{
    // Set comfortable size for long paths
    setMinimumWidth(450);
    setMaximumWidth(650);
    
    positionBottomRight();
    
    // Skip fade animation - just show immediately (fixes lag)
    m_opacity = 1.0;
    m_fadingOut = false;
    QWidget::show();
    update();
    
    // Schedule auto-hide
    m_autoHideTimer->start(durationMs);
}

void ReportNotification::positionBottomRight()
{
    if (!parentWidget()) return;
    
    // Position in bottom-right corner of parent window
    QPoint parentBottomRight = parentWidget()->mapToGlobal(
        QPoint(parentWidget()->width(), parentWidget()->height())
    );
    
    int x = parentBottomRight.x() - width() - 20;   // 20px margin from right
    int y = parentBottomRight.y() - height() - 20;  // 20px margin from bottom
    
    // Make sure it's on screen
    QScreen* screen = QApplication::screenAt(QPoint(x, y));
    if (screen) {
        QRect screenRect = screen->availableGeometry();
        
        if (x < screenRect.left()) {
            x = screenRect.left() + 20;
        }
        if (x + width() > screenRect.right()) {
            x = screenRect.right() - width() - 20;
        }
        if (y < screenRect.top()) {
            y = screenRect.top() + 20;
        }
        if (y + height() > screenRect.bottom()) {
            y = screenRect.bottom() - height() - 20;
        }
    }
    
    move(x, y);
}

void ReportNotification::fadeIn()
{
    m_fadingOut = false;
    m_fadeTimer->start(16);
}

void ReportNotification::fadeOut()
{
    // Just close immediately - skip animation to avoid lag
    close();
}

void ReportNotification::copyToClipboard()
{
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(m_reportText);
    
    // Visual feedback
    m_copyButton->setText("✓ Copied!");
    m_copyButton->setStyleSheet(
        "QPushButton {"
        "   background-color: %1;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 4px;"
        "   font-size: 10px;"
        "   font-weight: bold;"
        "   padding: 6px 12px;"
        "}"
    );
    
    // Reset after 1 second
    QTimer::singleShot(1000, this, [this]() {
        if (m_copyButton) {
            m_copyButton->setText("Copy to Clipboard");
            m_copyButton->setStyleSheet(QString(
                "QPushButton {"
                "   background-color: %1;"
                "   color: %2;"
                "   border: none;"
                "   border-radius: 4px;"
                "   font-size: 10px;"
                "   font-weight: bold;"
                "   padding: 6px 12px;"
                "}"
                "QPushButton:hover {"
                "   background-color: %3;"
                "}"
                "QPushButton:pressed {"
                "   background-color: %4;"
                "}"
            ).arg(AppTheme::colors().accent, AppTheme::colors().textOnAccent,
                  AppTheme::colors().accentDark, AppTheme::colors().accentDarker));
        }
    });
}

void ReportNotification::openFolder()
{
    QFileInfo fileInfo(m_filePath);
    QString folderPath = fileInfo.absolutePath();
    
    // Open folder in file explorer
    QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
    
    // Close notification after opening folder
    fadeOut();
}

void ReportNotification::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setOpacity(m_opacity);
    QFrame::paintEvent(event);
}