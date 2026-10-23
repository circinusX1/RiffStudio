#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QVector>
#include <QColor>

struct ChordMarker {
    double timeSeconds;
    QString chordName;
};

class GLWaveformWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit GLWaveformWidget(QWidget *parent = nullptr);
    ~GLWaveformWidget() override = default;

    void setAudioBuffer(const QVector<float> &samples, double durationSeconds, int sampleRate);
    void setPlaybackPosition(double seconds);
    void setChords(const QVector<ChordMarker> &chords);
    void setPitchShift(float semitones);
    void setAccentColor(const QColor &color);
    void setBpm(int bpm);
    void setVisibleFraction(float fraction);
    void setScrollOffset(int offset);

    int getTotalWidth() const;
    int getMaxScrollOffset() const;
    double getDuration() const { return m_duration; }

signals:
    void seekRequested(double timeSeconds);
    void selectionChanged(double startSeconds, double endSeconds);
    void scrollOffsetChanged(int newOffset);
    void maxScrollChanged(int maxScroll, int pageStep);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    QVector<float> m_samples;
    QVector<ChordMarker> m_chords;
    double m_duration = 0.0;
    int m_sampleRate = 44100;
    int m_bpm = 120;
    float m_pitchShift = 0.0f;
    QColor m_accentColor = QColor(245, 158, 11);
    double m_currentPosition = 0.0;

    float m_visibleFraction = 0.1f; // X Time Zoom Factor (0.1 = 10% visible)
    int m_scrollOffset = 0;

    // Selection Tracking
    bool m_isSelecting = false;
    double m_selectStartSec = -1.0;
    double m_selectEndSec = -1.0;

    QColor getChordColor(const QString &chord) const;
    void updateScrollMetrics();
    double snapToTempoTick(double rawSeconds) const;
};
