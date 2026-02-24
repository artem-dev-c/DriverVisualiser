#pragma once

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QString>
#include <functional>

/**
 * @class ReportNotification
 * @brief Bottom-right toast notification for report generation success
 *
 * Displays a temporary notification with:
 * - Success message
 * - File path or status
 * - Action buttons (Copy to Clipboard, Open Folder)
 * - Auto-dismiss after timeout
 *
 * Styling matches existing DriverInfoPopup aesthetic:
 * - Dark theme (#2b2b2b background)
 * - Blue accent colors (#5dade2)
 * - Smooth fade-in/out animations
 * - Positioned bottom-right of parent window
 */
class ReportNotification : public QFrame
{
    Q_OBJECT

public:
    /**
     * @brief Create a report success notification
     * @param filePath Full path to the saved report file
     * @param reportText The generated report content (for clipboard copy)
     * @param parent Parent widget (typically MainWindow)
     * @param isHtml True if HTML report (hides copy button)
     */
    explicit ReportNotification(
        const QString& filePath,
        const QString& reportText,
        QWidget* parent = nullptr,
        bool isHtml = false
    );

    /**
     * @brief Show the notification with fade-in animation
     * @param durationMs How long to show before auto-dismiss (default: 3000ms)
     */
    void show(int durationMs = 3000);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void setupUi();
    void positionBottomRight();
    void fadeIn();
    void fadeOut();

    /// Copy report text to clipboard and show visual feedback
    void copyToClipboard();

    /// Open the folder containing the report file
    void openFolder();

    QString m_filePath;
    QString m_reportText;
    bool m_isHtml = false;  // True if HTML report (hides copy button)
    
    QLabel*      m_titleLabel   = nullptr;
    QLabel*      m_pathLabel    = nullptr;
    QPushButton* m_copyButton   = nullptr;
    QPushButton* m_folderButton = nullptr;
    
    QTimer* m_autoHideTimer = nullptr;
    QTimer* m_fadeTimer     = nullptr;
    
    qreal m_opacity = 0.0;
    bool  m_fadingOut = false;
};