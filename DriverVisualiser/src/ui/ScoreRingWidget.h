#pragma once

#include <QWidget>

/**
 * @class ScoreRingWidget
 * @brief Custom widget that paints a circular health score ring.
 *
 * Displays a score (0-100) as a filled arc on a dark background ring,
 * with the numeric score and a label rendered in the center.
 *
 * Color thresholds (match DriverCardWidget):
 *   >= 80  -> Green  (#27ae60)
 *   >= 50  -> Orange (#f39c12)
 *   <  50  -> Red    (#e74c3c)
 */
class ScoreRingWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ScoreRingWidget(QWidget* parent = nullptr);

    /// Update the displayed score (0-100)
    void setScore(int score);

    /// Suggested size hint for layout
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    /// Returns the arc color for a given score
    QString scoreColor(int score) const;

    int m_score = 100;
};
