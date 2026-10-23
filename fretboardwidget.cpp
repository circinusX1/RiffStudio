#include "fretboardwidget.h"
#include <QPainter>

FretboardWidget::FretboardWidget(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(320);
    initShapes();
}

void FretboardWidget::initShapes() {
    // Strings ordered: Low E, A, D, G, B, High E
    m_shapes["G"]  = { {"3","2","0","0","3","3"}, {"3","5","5","x","x","x"} };
    m_shapes["D"]  = { {"x","x","0","2","3","2"}, {"x","5","7","7","x","x"} };
    m_shapes["EM"] = { {"0","2","2","0","0","0"}, {"0","2","2","x","x","x"} };
    m_shapes["C"]  = { {"x","3","2","0","1","0"}, {"x","3","5","5","x","x"} };
    m_shapes["AM"] = { {"x","0","2","2","1","0"}, {"5","7","7","x","x","x"} };
    m_shapes["A"]  = { {"x","0","2","2","2","0"}, {"5","7","7","x","x","x"} };
    m_shapes["E"]  = { {"0","2","2","1","0","0"}, {"0","2","2","x","x","x"} };
    m_shapes["BM"] = { {"x","2","4","4","3","2"}, {"2","4","4","x","x","x"} };
    m_shapes["F"]  = { {"1","3","3","2","1","1"}, {"1","3","3","x","x","x"} };
}

void FretboardWidget::setChord(const QString &currentChord, const QString &nextChord) {
    m_currentChord = currentChord.trimmed();
    m_nextChord = nextChord.trimmed();
    update();
}

void FretboardWidget::setViewMode(bool isTab) {
    m_viewTabs = isTab;
    update();
}

void FretboardWidget::setFingeringMode(bool isPower) {
    m_powerMode = isPower;
    update();
}

QVector<QString> FretboardWidget::getActiveShape(const QString &chord) {
    if (chord.isEmpty()) return m_shapes["G"].stdShape;

    // Normalize chord string format (e.g. "Em" -> "EM", "Gmaj" -> "G")
    QString cleanChord = chord.toUpper().trimmed();
    cleanChord.remove("MAJ").remove("MIN");

    if (m_shapes.contains(cleanChord)) {
        Shape s = m_shapes.value(cleanChord);
        return m_powerMode ? s.powerShape : s.stdShape;
    }

    // Fallback search by root note prefix (e.g., "C#m" -> "C")
    for (auto it = m_shapes.constBegin(); it != m_shapes.constEnd(); ++it) {
        if (cleanChord.startsWith(it.key())) {
            return m_powerMode ? it.value().powerShape : it.value().stdShape;
        }
    }

    return m_shapes["G"].stdShape;
}

void FretboardWidget::drawFretboard(QPainter &painter, const QRect &bounds, const QString &chordName, bool isMini) {
    QVector<QString> shape = getActiveShape(chordName);

    int numFrets = 6;
    int startX = bounds.left() + (isMini ? 15 : 40);
    int endX = bounds.right() - (isMini ? 15 : 40);
    int fretW = (endX - startX) / numFrets;

    int topY = bounds.top() + (isMini ? 15 : 24); // Adjusted top margin for main label
    int bottomY = bounds.bottom() - (isMini ? 20 : 25);
    int stringSpacing = (bottomY - topY) / 5;

    if (isMini) {
        // Draw Mini Preview Frame
        painter.setBrush(QColor(18, 18, 22));
        painter.setPen(QPen(QColor(161, 161, 170), 1));
        painter.drawRoundedRect(bounds, 6, 6);

        // Mini Header Title
        painter.setFont(QFont("Monospace", 8, QFont::Bold));
        painter.setPen(QColor(245, 158, 11));
        painter.drawText(bounds.adjusted(6, 2, -6, 0), Qt::AlignTop | Qt::AlignLeft, "NEXT: " + chordName.toUpper());
    } else {
        // ========================================================
        // BOLD CURRENT CHORD NAME ABOVE MAIN FRETBOARD
        // ========================================================
        painter.setFont(QFont("Monospace", 14, QFont::Bold));
        painter.setPen(QColor(245, 158, 11)); // Amber Highlight Color

        QString chordTitle = chordName.isEmpty() ? "CHORD: --" : "CHORD: " + chordName.toUpper();
        painter.drawText(QRect(startX, bounds.top() - 2, endX - startX, 22), Qt::AlignLeft | Qt::AlignVCenter, chordTitle);
    }

    // Draw Vertical Frets and Fret Numbers
    painter.setFont(QFont("Monospace", isMini ? 7 : 10, QFont::Bold));
    for (int f = 0; f <= numFrets; ++f) {
        int fx = startX + (f * fretW);
        painter.setPen(QPen(f == 0 ? QColor(212, 212, 216) : QColor(82, 82, 91), f == 0 ? (isMini ? 2 : 4) : 1.5));
        painter.drawLine(fx, topY, fx, bottomY);

        if (f < numFrets) {
            int numX = fx + (fretW / 2);
            painter.setPen(QColor(161, 161, 170));
            painter.drawText(QRect(numX - 10, bottomY + 2, 20, 18), Qt::AlignCenter, QString::number(f + 1));
        }
    }

    // Draw Horizontal Strings and Active Fingering Dots
    for (int s = 0; s < 6; ++s) {
        int sy = topY + (s * stringSpacing);
        painter.setPen(QPen(QColor(161, 161, 170), isMini ? 1 : (1 + s * 0.4)));
        painter.drawLine(startX, sy, endX, sy);

        QString fretStr = shape[5 - s];
        if (fretStr != "x" && fretStr != "0") {
            int fNum = fretStr.toInt();
            if (fNum <= numFrets) {
                int mx = startX + ((fNum - 1) * fretW) + (fretW / 2);
                int dotRadius = isMini ? 4 : 10;

                painter.setBrush(isMini ? QColor(16, 185, 129) : QColor(217, 119, 6));
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(QPoint(mx, sy), dotRadius, dotRadius);
            }
        }
    }
}

void FretboardWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(9, 9, 11));

    int w = width();
    int h = height();

    // Main Center Fretboard
    int mainHeight = h * 0.8;
    int mainTopY = h * 0.15;
    QRect mainBounds(0, mainTopY, w, mainHeight);

    drawFretboard(painter, mainBounds, m_currentChord, false);

    // Top-Right Mini Fretboard (Next Chord)
    int miniW = w / 4;
    int miniH = 80;
    QRect miniBounds(w - miniW - 12, 8, miniW, miniH);

    if (!m_nextChord.isEmpty()) {
        drawFretboard(painter, miniBounds, m_nextChord, true);
    }
}
