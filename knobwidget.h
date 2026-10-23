#pragma once

#include <QWidget>
#include <QDial>
#include <QPainter>
#include <QtMath>

class KnobWidget : public QDial {
    Q_OBJECT
public:
    QColor m_accentColor = QColor("#f59e0b");

public:
    explicit KnobWidget(QWidget *parent = nullptr) : QDial(parent) {
        setFocusPolicy(Qt::StrongFocus);
    }

public:
    void setAccentColor(const QColor &color) { m_accentColor = color; update(); }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        int sz = qMin(width(), height()) - 24; // Leave margin for tick marks
        QRectF rect((width() - sz) / 2.0, (height() - sz) / 2.0, sz, sz);
        QPointF center = rect.center();

        // 7. DRAW TICK-MARKS AROUND KNOB PERIMETER
        int numTicks = 11;
        double startAngle = 240.0;
        double endAngle = -60.0;
        double currentValAngle = 240.0 - (value() - minimum()) * 300.0 / (maximum() - minimum());
        QColor tickColor = isEnabled() ? m_accentColor : QColor("#52525b");
        painter.setPen(QPen(tickColor, 1.5, Qt::SolidLine, Qt::RoundCap));

        for (int i = 0; i < numTicks; ++i) {
            double tickAngle = startAngle - i * (startAngle - endAngle) / (numTicks - 1);
            double rad = qDegreesToRadians(tickAngle);

            double innerR = (sz / 2.0) + 3;
            double outerR = (sz / 2.0) + 8;

            QPointF innerPt(center.x() + innerR * qCos(rad), center.y() - innerR * qSin(rad));
            QPointF outerPt(center.x() + outerR * qCos(rad), center.y() - outerR * qSin(rad));

            // Highlight ticks up to the active knob value angle
            if (tickAngle >= currentValAngle) {
                painter.setPen(QPen(tickColor, 2)); // Amber active tick
            } else {
                painter.setPen(QPen(QColor(63, 63, 70), 1.5));  // Muted inactive tick
            }
            painter.drawLine(innerPt, outerPt);
        }

        // Metallic Rim
        QLinearGradient rimGrad(rect.topLeft(), rect.bottomRight());
        rimGrad.setColorAt(0.0, QColor(60, 60, 65));
        rimGrad.setColorAt(1.0, QColor(15, 15, 20));
        painter.setBrush(rimGrad);
        painter.setPen(QPen(QColor(10, 10, 12), 2));
        painter.drawEllipse(rect);

        // Inner Dial Face
        QRectF innerRect = rect.adjusted(6, 6, -6, -6);
        QRadialGradient faceGrad(innerRect.center(), innerRect.width() / 2.0);
        faceGrad.setColorAt(0.0, QColor(45, 45, 50));
        faceGrad.setColorAt(1.0, QColor(20, 20, 24));
        painter.setBrush(faceGrad);
        painter.setPen(QPen(QColor(80, 80, 85), 1));
        painter.drawEllipse(innerRect);

        // Pointer Indicator
        double rad = qDegreesToRadians(currentValAngle);
        QPointF pointerEnd(center.x() + (sz / 2.8) * qCos(rad), center.y() - (sz / 2.8) * qSin(rad));

        painter.setPen(QPen(tickColor, 3, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(center, pointerEnd);
    }
};
