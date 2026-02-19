#include "ScoreRingWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QRectF>
#include <QFont>
#include <QColor>

ScoreRingWidget::ScoreRingWidget(QWidget* parent)
    : QWidget(parent)
    , m_score(100)
{
    // Fixed size — parent layout controls placement
    setFixedSize(sizeHint());
    setAttribute(Qt::WA_TranslucentBackground);
}

void ScoreRingWidget::setScore(int score)
{
    m_score = std::clamp(score, 0, 100);
    update();  // Trigger repaint
}

QSize ScoreRingWidget::sizeHint() const
{
    return QSize(180, 180);
}

void ScoreRingWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();

    // Ring geometry
    const int ringThickness = 14;
    const int margin = ringThickness / 2 + 2;
    QRectF ringRect(margin, margin, w - margin * 2, h - margin * 2);

    // === Background ring (dark track) ===
    QPen trackPen(QColor("#2d2d2d"), ringThickness, Qt::SolidLine, Qt::FlatCap);
    painter.setPen(trackPen);
    painter.drawEllipse(ringRect);

    // === Score arc ===
    // Qt arc: 0° is 3 o'clock, angles are counter-clockwise in 1/16th degrees
    // We want to start at 12 o'clock (90°) and go clockwise
    const int startAngle = 90 * 16;   // 12 o'clock
    const int spanAngle  = -static_cast<int>((m_score / 100.0) * 360.0 * 16);

    QPen arcPen(QColor(scoreColor(m_score)), ringThickness, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(arcPen);

    if (m_score > 0) {
        painter.drawArc(ringRect, startAngle, spanAngle);
    }

    // === Center score number ===
    QFont scoreFont = painter.font();
    scoreFont.setPointSize(32);
    scoreFont.setBold(true);
    painter.setFont(scoreFont);
    painter.setPen(QColor("#ffffff"));

    // Draw score percentage centered
    QString scoreText = QString::number(m_score) + "%";
    QRectF centerRect(0, 0, w, h - 16);  // Slightly above center to leave room for label
    painter.drawText(centerRect, Qt::AlignCenter, scoreText);

    // === Center sub-label ("Health") ===
    QFont labelFont = painter.font();
    labelFont.setPointSize(11);
    labelFont.setBold(false);
    painter.setFont(labelFont);
    painter.setPen(QColor("#888888"));

    QRectF labelRect(0, h / 2 + 14, w, h / 2);
    painter.drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop, "Health");
}

QString ScoreRingWidget::scoreColor(int score) const
{
    // Match DriverCardWidget::getHealthColor thresholds exactly
    if (score >= 80) {
        return "#27ae60";  // Green
    } else if (score >= 50) {
        return "#f39c12";  // Orange
    } else {
        return "#e74c3c";  // Red
    }
}