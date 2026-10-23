#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QAudioSink>
#include <QAudioDecoder>
#include <QSlider>
#include <QScrollBar>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QVector>
#include <QBuffer>
#include <QGroupBox>
#include <QByteArray>
#include <QTimer>
#include <soundtouch/SoundTouch.h>
#include "glwaveformwidget.h"
#include "fretboardwidget.h"
#include "knobwidget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() = default;

private slots:
    void openAudioFile();
    void togglePlayPause();
    void stopAudio();
    void updateAudioEffects();
    void updatePosition(qint64 position);
    void handleAudioBufferReady();
    void handleDecoderFinished();
    void handleSelectionChanged(double startSec, double endSec);
    void onTimelineSliderChanged(int value);
    void openRecentFile();
    void clearRecentFiles();
    void showAboutDialog();

private:
    void setupMenuBar();
    void setupUi();
    void loadAudioFile(const QString &filePath); // Added missing declaration
    void addRecentFile(const QString &filePath);
    void updateRecentFileActions();
    void loadLastOpenedFile();
    QIcon createAppIcon();

    QVector<ChordMarker> analyzeAudioChords(const QVector<float> &samples, double duration, int sampleRate, double hopSizeSec, int fftSize);
    int detectBpm(const QVector<float> &samples, int sampleRate);
    QByteArray processAudioWithSoundTouch(float tempo, float pitch);
    void updatePreprocessedAudio();

    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
    QAudioSink *m_audioSink = nullptr;
    QAudioDecoder *m_audioDecoder = nullptr;
    QBuffer *m_audioBufferDevice = nullptr;
    QTimer *m_playbackTimer = nullptr;

    GLWaveformWidget *m_waveformWidget = nullptr;
    FretboardWidget *m_fretboardWidget = nullptr;
    QScrollBar *m_waveformScrollBar = nullptr;

    QPushButton *m_playPauseBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QPushButton *m_repeatBtn = nullptr;
    QLabel *m_timeDisplay = nullptr;
    QLabel *m_bpmValueLabel = nullptr;
    QLabel *m_speedValueLabel = nullptr;
    QLabel *m_pitchValueLabel = nullptr;
    QLabel *m_volumeValueLabel = nullptr;
    QLabel *m_timelineValueLabel = nullptr;
    QLabel *m_hopSizeLabel = nullptr; // Added missing label declaration
    QLabel *m_fftSizeLabel = nullptr; // Added missing label declaration

    KnobWidget *m_speedKnob = nullptr;
    KnobWidget *m_pitchKnob = nullptr;
    KnobWidget *m_volumeKnob = nullptr;
    QSlider *m_timelineSlider = nullptr;

    QGroupBox *m_chordExtractionGroup = nullptr;
    QComboBox *m_hopSizeCombo = nullptr;
    QComboBox *m_fftSizeCombo = nullptr;

    QGroupBox *m_fingeringGroup = nullptr;
    QPushButton *m_fingeringToggleBtn = nullptr;
    QLabel *m_fingeringLabel = nullptr;

    QMenu *m_recentFilesMenu = nullptr;
    static const int MaxRecentFiles = 5;
    QAction *m_recentFileActions[MaxRecentFiles];
    QAction *m_clearRecentAction = nullptr;

    QVector<float> m_decodedSamples;
    QByteArray m_preprocessedPcmData;
    QVector<ChordMarker> m_extractedChords;
    const int m_fftSizes[6] = {512, 1024, 2048, 4096, 8192, 16384};

    double m_loopStartSec = -1.0;
    double m_loopEndSec = -1.0;
    double m_currentPositionSec = 0.0;
    bool m_isPaused = false;
    qint64 m_playbackStartTimeMs = 0;
};

#endif // MAINWINDOW_H
