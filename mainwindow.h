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
#include <QStatusBar>
#include <QProgressBar>
#include <QResizeEvent>
#include <soundtouch/SoundTouch.h>
#include "glwaveformwidget.h"
#include "fretboardwidget.h"
#include "knobwidget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() = default;

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

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
    void loadAudioFile(const QString &filePath);
    void addRecentFile(const QString &filePath);
    void updateRecentFileActions();
    void loadLastOpenedFile();
    QIcon createAppIcon();

    QVector<ChordMarker> analyzeAudioChords(const QVector<float> &samples, double duration, int sampleRate, double hopSizeSec, int fftSize);
    int detectBpm(const QVector<float> &samples, int sampleRate);
    QByteArray processAudioWithSoundTouch(float tempo, float pitch);
    void updatePreprocessedAudio();
    void updatePreprocessedAudioAsync();
    void applyTheme(bool cyan);
    void saveSettings();
    void loadSettings();

    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
    QAudioSink *m_audioSink = nullptr;
    QAudioDecoder *m_audioDecoder = nullptr;
    QBuffer *m_audioBufferDevice = nullptr;
    QTimer *m_playbackTimer = nullptr;
    QStatusBar *m_statusBar = nullptr;
    QProgressBar *m_processBar = nullptr;
    QPushButton *m_themeToggleBtn = nullptr;
    bool m_cyanTheme = false;

    GLWaveformWidget *m_waveformWidget = nullptr;
    FretboardWidget *m_fretboardWidget = nullptr;
    QScrollBar *m_waveformScrollBar = nullptr;

    QPushButton *m_playPauseBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QPushButton *m_repeatBtn = nullptr;
    QLabel *m_timeDisplay = nullptr;
    QLabel *m_speedValueLabel = nullptr;
    QLabel *m_pitchValueLabel = nullptr;
    QLabel *m_volumeValueLabel = nullptr;
    QLabel *m_timelineValueLabel = nullptr;
    QLabel *m_hopSizeLabel = nullptr;
    QLabel *m_fftSizeLabel = nullptr;

    QPushButton *m_speedButtons[7] = {nullptr};
    float m_currentSpeed = 1.0f;
    QPushButton *m_pitchButtons[9] = {nullptr};
    float m_currentPitch = 0.0f;
    KnobWidget *m_volumeKnob = nullptr;
    QSlider *m_timelineSlider = nullptr;

    QGroupBox *m_tempoGroup = nullptr;
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
    bool m_processing = false;
    bool m_resumeAfterProcess = false;
    qint64 m_playbackStartTimeMs = 0;
};

#endif // MAINWINDOW_H
