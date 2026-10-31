#include "glwaveformwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QSignalBlocker>
#include <cmath>

GLWaveformWidget::GLWaveformWidget(QWidget *parent) : QOpenGLWidget(parent) {
    setMinimumHeight(144);
    setMouseTracking(true);
}

void GLWaveformWidget::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.035f, 0.035f, 0.043f, 1.0f); // #09090b Studio Dark
}

void GLWaveformWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    updateScrollMetrics();
}

void GLWaveformWidget::setAudioBuffer(const QVector<float> &samples, double durationSeconds, int sampleRate) {
    m_samples = samples;
    m_duration = durationSeconds;
    m_sampleRate = sampleRate;
    m_scrollOffset = 0;
    m_selectStartSec = -1.0;
    m_selectEndSec = -1.0;
    updateScrollMetrics();
    update();
}

void GLWaveformWidget::setBpm(int bpm) {
    m_bpm = std::max(1, bpm);
    update();
}

void GLWaveformWidget::setVisibleFraction(float fraction) {
    m_visibleFraction = std::max(0.01f, std::min(1.0f, fraction));
    updateScrollMetrics();
    setPlaybackPosition(m_currentPosition);
    update();
}

void GLWaveformWidget::setScrollOffset(int offset) {
    int clamped = std::max(0, std::min(offset, getMaxScrollOffset()));
    if (m_scrollOffset != clamped) {
        m_scrollOffset = clamped;
        update();
    }
}

int GLWaveformWidget::getTotalWidth() const {
    if (m_visibleFraction <= 0.0f) return width();
    return static_cast<int>(width() / m_visibleFraction);
}

int GLWaveformWidget::getMaxScrollOffset() const {
    return std::max(0, getTotalWidth() - width());
}

void GLWaveformWidget::updateScrollMetrics() {
    emit maxScrollChanged(getMaxScrollOffset(), width());
}

double GLWaveformWidget::snapToTempoTick(double rawSeconds) const {
    if (m_bpm <= 0) return rawSeconds;
    double beatInterval = 60.0 / m_bpm;
    return std::round(rawSeconds / beatInterval) * beatInterval;
}

void GLWaveformWidget::setPlaybackPosition(double seconds) {
    m_currentPosition = seconds;

    if (m_duration > 0) {
        int visibleWidth = width();
        int totalWidth = getTotalWidth();
        int maxScroll = getMaxScrollOffset();

        int playheadX = qRound((m_currentPosition / m_duration) * totalWidth);
        int thresholdX = m_scrollOffset + static_cast<int>(visibleWidth * (2.0 / 3.0));

        // Smooth Auto-Scrolling when playhead reaches 2/3 of viewport
        if (playheadX >= thresholdX && m_scrollOffset < maxScroll) {
            int targetOffset = playheadX - static_cast<int>(visibleWidth * (2.0 / 3.0));
            int newOffset = std::min(maxScroll, targetOffset);

            if (newOffset != m_scrollOffset) {
                m_scrollOffset = newOffset;
                QSignalBlocker blocker(this);
                emit scrollOffsetChanged(m_scrollOffset);
            }
        } else if (playheadX < m_scrollOffset) {
            int newOffset = std::max(0, playheadX - (visibleWidth / 3));
            if (newOffset != m_scrollOffset) {
                m_scrollOffset = newOffset;
                QSignalBlocker blocker(this);
                emit scrollOffsetChanged(m_scrollOffset);
            }
        }
    }

    update();
}

void GLWaveformWidget::setAccentColor(const QColor &color) {
    m_accentColor = color;
    update();
}
void GLWaveformWidget::setPitchShift(float semitones) {
    m_pitchShift = semitones;
    update();
}

void GLWaveformWidget::clearSelection() {
    m_selectStartSec = -1.0;
    m_selectEndSec = -1.0;
    m_isSelecting = false;
    emit selectionChanged(-1.0, -1.0);
    update();
}

void GLWaveformWidget::setChords(const QVector<ChordMarker> &chords) {
    m_chords = chords;
    update();
}

QColor GLWaveformWidget::getChordColor(const QString &chord) const {
    QString upper = chord.toUpper();
    if (upper.startsWith("E")) return QColor("#2563eb");
    if (upper.startsWith("G")) return QColor("#059669");
    if (upper.startsWith("D")) return QColor("#d97706");
    if (upper.startsWith("C")) return QColor("#7c3aed");
    if (upper.startsWith("A")) return QColor("#db2777");
    if (upper.startsWith("B")) return QColor("#4f46e5");
    if (upper.startsWith("F")) return QColor("#ea580c");
    return QColor("#0d9488");
}

void GLWaveformWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int visibleWidth = width() - 4;
    int h = height();

    if (m_samples.isEmpty() || m_duration <= 0) {
        painter.setPen(QPen(QColor(161, 161, 170), 1));
        painter.drawRect(0, 0, visibleWidth - 1, h - 1);
        return;
    }

    int totalWidth = getTotalWidth();
    int headerHeight = 32;
    int graphTopY = headerHeight + 4;
    int graphHeight = h - graphTopY - 18;
    int centerY = graphTopY + (graphHeight / 2);
    int maxAmplitudeHeight = (graphHeight / 2) - 4;

    painter.translate(-m_scrollOffset, 0);

    // 1. CHORD HEADER BLOCKS
        painter.setFont(QFont("Monospace", 10, QFont::Bold));
        double totalW = static_cast<double>(getTotalWidth()); // Use double for sub-pixel accuracy

        for (int i = 0; i < m_chords.size(); ++i) {
            double startTime = m_chords[i].timeSeconds;
            double endTime = (i + 1 < m_chords.size()) ? m_chords[i + 1].timeSeconds : m_duration;

            // Calculate exact double X coordinates on the timeline
            double x1 = (startTime / m_duration) * totalW;
            double x2 = (endTime / m_duration) * totalW;
            double boxWidth = std::max(1.0, x2 - x1);

            // Check if visible in current scroll viewport
            if (x2 >= m_scrollOffset && x1 <= m_scrollOffset + visibleWidth) {
                QRectF chordRect(x1, 2.0, boxWidth, static_cast<double>(headerHeight));

                painter.fillRect(chordRect, getChordColor(m_chords[i].chordName));
                painter.setPen(QPen(QColor(255, 255, 255, 180), 1));
                painter.drawRect(chordRect);
                painter.setPen(QColor(255, 255, 255));
                painter.drawText(chordRect, Qt::AlignCenter, m_chords[i].chordName.toUpper());
            }
        }

    // 2. SELECTION OVERLAY
    if (m_selectStartSec >= 0 && m_selectEndSec > m_selectStartSec) {
        int xStart = static_cast<int>((m_selectStartSec / m_duration) * totalWidth);
        int xEnd = static_cast<int>((m_selectEndSec / m_duration) * totalWidth);
        painter.fillRect(QRect(xStart, graphTopY, xEnd - xStart, h - graphTopY), QColor(245, 158, 11, 60));
        painter.setPen(QPen(m_accentColor, 1.5, Qt::DashLine));
        painter.drawRect(QRect(xStart, graphTopY, xEnd - xStart, h - graphTopY));
    }

    // 3. BASELINE AND WAVEFORM LINES (NORMALIZED & FULL HEIGHT)
    painter.setPen(QPen(QColor(63, 63, 70), 1));
    painter.drawLine(0, centerY, totalWidth, centerY);

    int totalSamples = m_samples.size();
    if (totalSamples > 0) {
        // Find maximum peak amplitude across the audio buffer for dynamic height normalization
        float maxAbsAmplitude = 0.0001f;
        for (float sample : m_samples) {
            float absVal = std::abs(sample);
            if (absVal > maxAbsAmplitude) {
                maxAbsAmplitude = absVal;
            }
        }

        QColor waveColor = (m_pitchShift < 0.0f) ? QColor(200, 200, 200) : m_accentColor;
        painter.setPen(QPen(waveColor, 1));

        // Draw min/max peak envelope per pixel column
        for (int x = m_scrollOffset; x < m_scrollOffset + visibleWidth && x < totalWidth; ++x) {
            int sampleStart = static_cast<int>((static_cast<double>(x) / totalWidth) * totalSamples);
            int sampleEnd = static_cast<int>((static_cast<double>(x + 1) / totalWidth) * totalSamples);

            sampleStart = std::max(0, std::min(sampleStart, totalSamples - 1));
            sampleEnd = std::max(sampleStart + 1, std::min(sampleEnd, totalSamples));

            float minAmp = 0.0f;
            float maxAmp = 0.0f;

            for (int i = sampleStart; i < sampleEnd; ++i) {
                float s = m_samples[i];
                if (s < minAmp) minAmp = s;
                if (s > maxAmp) maxAmp = s;
            }

            // Normalize values relative to peak audio amplitude
            float normMin = minAmp / maxAbsAmplitude;
            float normMax = maxAmp / maxAbsAmplitude;

            int y1 = centerY - static_cast<int>(normMax * maxAmplitudeHeight);
            int y2 = centerY - static_cast<int>(normMin * maxAmplitudeHeight);

            if (y1 == y2) {
                y1 -= 1;
                y2 += 1;
            }

            painter.drawLine(x, y1, x, y2);
        }
    }

    // 4. BEAT TICKS
    if (m_bpm > 0) {
        double beatInterval = 60.0 / m_bpm;
        int tickHeight = h / 6;
        int tickY = h - tickHeight;

        painter.setPen(QPen(QColor(0, 255, 255, 120), 1));
        for (double t = 0; t <= m_duration; t += beatInterval) {
            int tickX = static_cast<int>((t / m_duration) * totalWidth);
            if (tickX >= m_scrollOffset - 2 && tickX <= m_scrollOffset + visibleWidth + 2) {
                painter.drawLine(tickX, tickY, tickX, h);
            }
        }
    }

    // 5. TIMELINE TIME LABELS
    painter.setFont(QFont("Monospace", 8));
    painter.setPen(QColor(161, 161, 170));
    for (int sec = 0; sec <= static_cast<int>(m_duration); sec += 10) {
        int secX = static_cast<int>((static_cast<double>(sec) / m_duration) * totalWidth);
        if (secX >= m_scrollOffset - 20 && secX <= m_scrollOffset + visibleWidth + 20) {
            painter.drawText(secX + 3, h - 4, QString("%1s").arg(sec));
        }
    }

    // 6. PLAYHEAD
    if (m_duration > 0) {
        int cursorX = static_cast<int>((m_currentPosition / m_duration) * totalWidth);
        painter.setPen(QPen(QColor(239, 68, 68), 2));
        painter.drawLine(cursorX, 0, cursorX, h);
    }

    // Outer Viewport Border
    painter.translate(m_scrollOffset, 0);
    painter.setPen(QPen(QColor(161, 161, 170), 1));
    painter.drawRect(0, 0, visibleWidth - 1, h - 1);
}

// MOUSE INTERACTION & MOUSE-WHEEL ZOOM
void GLWaveformWidget::wheelEvent(QWheelEvent *event) {
    float zoomFactor = (event->angleDelta().y() > 0) ? 0.85f : 1.15f;
    setVisibleFraction(m_visibleFraction * zoomFactor);
    event->accept();
}

void GLWaveformWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && m_duration > 0) {
        m_isSelecting = true;
        int absoluteX = event->pos().x() + m_scrollOffset;
        double rawTime = (static_cast<double>(absoluteX) / getTotalWidth()) * m_duration;

        m_selectStartSec = snapToTempoTick(rawTime);
        m_selectEndSec = m_selectStartSec;

        // Notify MainWindow so a point selection clears any previous loop range
        emit selectionChanged(m_selectStartSec, m_selectEndSec);
        emit seekRequested(m_selectStartSec);
        update();
    }
}

void GLWaveformWidget::mouseMoveEvent(QMouseEvent *event) {
    if (m_isSelecting && m_duration > 0) {
        int absoluteX = event->pos().x() + m_scrollOffset;
        double rawTime = (static_cast<double>(absoluteX) / getTotalWidth()) * m_duration;

        double snappedTime = snapToTempoTick(rawTime);
        if (snappedTime >= m_selectStartSec) {
            m_selectEndSec = snappedTime;
        } else {
            m_selectEndSec = m_selectStartSec;
            m_selectStartSec = snappedTime;
        }

        emit selectionChanged(m_selectStartSec, m_selectEndSec);
        update();
    }
}

void GLWaveformWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_isSelecting = false;
    }
}
