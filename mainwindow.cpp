#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMenuBar>
#include <QMenu>
#include <QSettings>
#include <QCloseEvent>
#include <QFileInfo>
#include <QStyle>
#include <QApplication>
#include <QMediaDevices>
#include <QAudioFormat>
#include <QAudioDevice>
#include <QDebug>
#include <QThread>
#include <QMetaObject>
#include <cmath>
#include <complex>
#include <valarray>
#include <algorithm>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPainterPath>
#include <QPainter>
#include <essentia/essentia.h>
#include <essentia/algorithmfactory.h>

// ========================================================
// RADIX-2 FFT IMPLEMENTATION
// ========================================================
static void fftProcess(std::valarray<std::complex<double>>& x) {
    const size_t N = x.size();
    if (N <= 1) return;

    std::valarray<std::complex<double>> even = x[std::slice(0, N / 2, 2)];
    std::valarray<std::complex<double>> odd  = x[std::slice(1, N / 2, 2)];

    fftProcess(even);
    fftProcess(odd);

    for (size_t k = 0; k < N / 2; ++k) {
        std::complex<double> t = std::polar(1.0, -2 * M_PI * k / N) * odd[k];
        x[k]       = even[k] + t;
        x[k + N/2] = even[k] - t;
    }
}

// ========================================================
// ADVANCED ESSENTIA CHORD EXTRACTION ENGINE
// ========================================================
QVector<ChordMarker> MainWindow::analyzeAudioChords(const QVector<float> &samples,
                                                    double duration,
                                                    int sampleRate,
                                                    double hopSizeSec,
                                                    int fftSize)
{
    QVector<ChordMarker> markers;
    if (samples.isEmpty() || duration <= 0 || sampleRate <= 0) return markers;

    using namespace essentia;
    using namespace essentia::standard;

    std::vector<float> audioBuffer(samples.begin(), samples.end());

    int frameSize = fftSize;
    int hopSizeSamples = static_cast<int>(hopSizeSec * sampleRate);
    if (hopSizeSamples < 1) hopSizeSamples = sampleRate;

    Algorithm* fc = AlgorithmFactory::create("FrameCutter",
                                             "frameSize", frameSize,
                                             "hopSize", hopSizeSamples);
    Algorithm* w  = AlgorithmFactory::create("Windowing",
                                             "type", "hann");
    Algorithm* spec = AlgorithmFactory::create("Spectrum");
    Algorithm* peaks = AlgorithmFactory::create("SpectralPeaks",
                                                "sampleRate", static_cast<float>(sampleRate),
                                                "maxFrequency", 2093.0f,
                                                "minFrequency", 65.0f);
    Algorithm* hpcp = AlgorithmFactory::create("HPCP",
                                               "sampleRate", static_cast<float>(sampleRate),
                                               "maxFrequency", 2093.0f,
                                               "minFrequency", 65.0f);

    std::vector<float> frame, windowedFrame, spectrum;
    std::vector<float> peakFrequencies, peakMagnitudes;
    std::vector<float> hpcpFrame;
    std::vector<std::vector<float>> hpcpSequence;

    fc->input("signal").set(audioBuffer);
    fc->output("frame").set(frame);

    w->input("frame").set(frame);
    w->output("frame").set(windowedFrame);

    spec->input("frame").set(windowedFrame);
    spec->output("spectrum").set(spectrum);

    peaks->input("spectrum").set(spectrum);
    peaks->output("frequencies").set(peakFrequencies);
    peaks->output("magnitudes").set(peakMagnitudes);

    hpcp->input("frequencies").set(peakFrequencies);
    hpcp->input("magnitudes").set(peakMagnitudes);
    hpcp->output("hpcp").set(hpcpFrame);

    while (true) {
        fc->compute();
        if (frame.empty()) break;

        w->compute();
        spec->compute();
        peaks->compute();
        hpcp->compute();

        hpcpSequence.push_back(hpcpFrame);
    }

    const QStringList rootNotes = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    struct ChordTemplate {
        QString name;
        double profile[12];
    };

    QVector<ChordTemplate> templates;
    for (int root = 0; root < 12; ++root) {
        ChordTemplate maj;
        maj.name = rootNotes[root];
        std::fill(std::begin(maj.profile), std::end(maj.profile), 0.0);
        maj.profile[root] = 1.0;
        maj.profile[(root + 4) % 12] = 0.8;
        maj.profile[(root + 7) % 12] = 0.5;
        templates.append(maj);

        ChordTemplate min;
        min.name = rootNotes[root] + "m";
        std::fill(std::begin(min.profile), std::end(min.profile), 0.0);
        min.profile[root] = 1.0;
        min.profile[(root + 3) % 12] = 0.8;
        min.profile[(root + 7) % 12] = 0.5;
        templates.append(min);
    }

    QString lastChord = "";
    for (size_t i = 0; i < hpcpSequence.size(); ++i) {
        const std::vector<float>& chroma = hpcpSequence[i];
        if (chroma.size() < 12) continue;

        QString bestChord = "";
        double maxCorrelation = -1.0;

        for (const auto &tpl : templates) {
            double dotProduct = 0.0, normA = 0.0, normB = 0.0;

            for (int c = 0; c < 12; ++c) {
                dotProduct += chroma[c] * tpl.profile[c];
                normA += chroma[c] * chroma[c];
                normB += tpl.profile[c] * tpl.profile[c];
            }

            double similarity = (normA > 0 && normB > 0) ? (dotProduct / (std::sqrt(normA) * std::sqrt(normB))) : 0.0;

            if (similarity > maxCorrelation) {
                maxCorrelation = similarity;
                bestChord = tpl.name;
            }
        }

        double timeSec = (i * hopSizeSamples) / static_cast<double>(sampleRate);

        if (bestChord != lastChord && !bestChord.isEmpty()) {
            markers.append({ timeSec, bestChord });
            lastChord = bestChord;
        }
    }

    delete fc;
    delete w;
    delete spec;
    delete peaks;
    delete hpcp;

    return markers;
}

// ========================================================
// TEMPO (BPM) DETECTION ALGORITHM
// ========================================================
int MainWindow::detectBpm(const QVector<float> &samples, int sampleRate) {
    if (samples.size() < sampleRate * 5) return 120;

    int hopSize = sampleRate / 100;
    QVector<float> energyEnvelope;

    for (int i = 0; i < samples.size() - hopSize; i += hopSize) {
        float energy = 0.0f;
        for (int j = 0; j < hopSize; ++j) {
            energy += std::abs(samples[i + j]);
        }
        energyEnvelope.append(energy);
    }

    int minLag = static_cast<int>((60.0 / 180.0) * 100);
    int maxLag = static_cast<int>((60.0 / 60.0) * 100);

    int bestLag = minLag;
    float maxCorr = -1.0f;

    for (int lag = minLag; lag <= maxLag; ++lag) {
        float corr = 0.0f;
        for (int i = 0; i < energyEnvelope.size() - lag; ++i) {
            corr += energyEnvelope[i] * energyEnvelope[i + lag];
        }
        if (corr > maxCorr) {
            maxCorr = corr;
            bestLag = lag;
        }
    }

    int detectedBpm = qRound(60.0 / (bestLag / 100.0));
    return std::max(60, std::min(180, detectedBpm));
}

// ========================================================
// SOUNDTOUCH PRE-PROCESSING HELPER
// ========================================================
QByteArray MainWindow::processAudioWithSoundTouch(float tempo, float pitch) {
    if (m_decodedSamples.isEmpty()) return QByteArray();

    soundtouch::SoundTouch st;
    st.setSampleRate(44100);
    st.setChannels(1);
    st.setTempo(tempo);
    st.setPitchSemiTones(pitch);

    st.putSamples(m_decodedSamples.constData(), m_decodedSamples.size());
    st.flush();

    QByteArray pcmData;
    int bufferSize = 4096;
    std::vector<float> floatBuffer(bufferSize);
    int received = 0;

    do {
        received = st.receiveSamples(floatBuffer.data(), bufferSize);
        if (received > 0) {
            int bytesToWrite = received * sizeof(qint16);
            pcmData.resize(pcmData.size() + bytesToWrite);
            qint16 *outPtr = reinterpret_cast<qint16*>(pcmData.data() + pcmData.size() - bytesToWrite);

            for (int i = 0; i < received; ++i) {
                float sample = std::max(-1.0f, std::min(1.0f, floatBuffer[i]));
                outPtr[i] = static_cast<qint16>(sample * 32767.0f);
            }
        }
    } while (received > 0);

    return pcmData;
}

void MainWindow::updatePreprocessedAudio() {
    // Synchronous version kept for initial load
    float tempo = m_currentSpeed;
    float pitchSt = m_currentPitch;
    m_preprocessedPcmData = processAudioWithSoundTouch(tempo, pitchSt);
}


void MainWindow::applyTheme(bool cyan) {
    QString accent = cyan ? "#22d3ee" : "#f59e0b";
    QString accentDark = cyan ? "#06b6d4" : "#d97706";
    QColor accentColor = cyan ? QColor(34, 211, 238) : QColor(245, 158, 11);

    // Status bar
    if (m_statusBar && !m_processing)
        m_statusBar->setStyleSheet(QString("QStatusBar { background-color: #09090b; color: %1; font-family: monospace; font-size: 12px; }").arg(accent));

    // Processing progress bar
    if (m_processBar) {
        m_processBar->setStyleSheet(QString(
            "QProgressBar { border: none; background-color: #09090b; }"
            "QProgressBar::chunk { background-color: %1; }"
        ).arg(accent));
    }

    // Labels
    if (m_speedValueLabel) m_speedValueLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 16px; font-family: monospace;").arg(accent));
    if (m_pitchValueLabel) m_pitchValueLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 16px; font-family: monospace;").arg(accent));
    if (m_volumeValueLabel) m_volumeValueLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 22px; font-family: monospace;").arg(accent));
    if (m_fingeringLabel) m_fingeringLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 11px; font-family: monospace;").arg(accent));
    if (m_timeDisplay) m_timeDisplay->setStyleSheet(QString("background-color: #000000; color: %1; font-family: monospace; font-size: 14px; padding: 4px 12px; border-radius: 4px;").arg(accent));
    if (m_timelineValueLabel) m_timelineValueLabel->setStyleSheet(QString("color: %1; font-family: monospace; font-weight: bold; font-size: 11px;").arg(accent));

    // Speed / Pitch buttons
    QString btnStyle = QString(
        "QPushButton { background-color: #18181b; color: %1; border: 1px solid #3f3f46; border-radius: 4px; font-weight: bold; font-size: 12px; min-width: 42px; min-height: 32px; }"
        "QPushButton:checked { background-color: %2; color: #09090b; }"
        "QPushButton:disabled { background-color: #27272a; color: #52525b; border-color: #3f3f46; }"
    ).arg(accent, accentDark);

    for (int i = 0; i < 7; ++i)
        if (m_speedButtons[i]) m_speedButtons[i]->setStyleSheet(btnStyle);
    for (int i = 0; i < 9; ++i)
        if (m_pitchButtons[i]) m_pitchButtons[i]->setStyleSheet(btnStyle);

    // Fingering toggle
    if (m_fingeringToggleBtn)
        m_fingeringToggleBtn->setStyleSheet(QString(
            "QPushButton { background-color: #18181b; color: %1; border: 1px solid #3f3f46; border-radius: 4px; font-weight: bold; font-size: 11px; } "
            "QPushButton:checked { background-color: %2; color: #09090b; }"
        ).arg(accent, accentDark));

    // Combos
    QString comboStyle = QString(
        "QComboBox { background-color: #18181b; color: %1; border: 1px solid #3f3f46; border-radius: 4px; padding: 3px 8px; font-family: monospace; font-weight: bold; font-size: 11px; } "
        "QComboBox::drop-down { border: none; } "
        "QComboBox QAbstractItemView { background-color: #18181b; color: %1; selection-background-color: #3f3f46; }"
    ).arg(accent);
    if (m_hopSizeCombo) m_hopSizeCombo->setStyleSheet(comboStyle);
    if (m_fftSizeCombo) m_fftSizeCombo->setStyleSheet(comboStyle);

    // Waveform accent
    if (m_waveformWidget) m_waveformWidget->setAccentColor(accentColor);
    if (m_fretboardWidget) m_fretboardWidget->setAccentColor(accentColor);
    if (m_volumeKnob) m_volumeKnob->setAccentColor(accentColor);

    // Force repaint
    update();
}

void MainWindow::updatePreprocessedAudioAsync() {
    if (m_decodedSamples.isEmpty()) return;
    if (m_processing) return; // already running

    // If currently playing, pause and grey out play button; remember to resume after processing
    m_resumeAfterProcess = false;
    if (m_audioSink && m_audioSink->state() == QAudio::ActiveState) {
        m_audioSink->stop();
        if (m_playbackTimer) m_playbackTimer->stop();

        qint64 elapsedMs = m_playbackStartTimeMs + (m_audioSink->processedUSecs() / 1000);
        m_currentPositionSec = elapsedMs / 1000.0;
        m_isPaused = true;
        m_resumeAfterProcess = true;

        if (m_playPauseBtn) {
            m_playPauseBtn->setEnabled(false);
            m_playPauseBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        }
    }

    m_processing = true;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    if (m_playPauseBtn) {
        m_playPauseBtn->setEnabled(false);
    }
    if (m_processBar) {
        m_processBar->setVisible(true);
        m_processBar->setRange(0, 0); // busy
        m_processBar->raise();
    }
    if (m_statusBar) {
        m_statusBar->setStyleSheet("QStatusBar { background-color: #09090b; color: #ffffff; font-family: monospace; font-size: 12px; }");
        m_statusBar->showMessage("Processing chords, will play new tempo / pitch as soon as processing is done...");
    }

    float tempo = m_currentSpeed;
    float pitchSt = m_currentPitch;
    QVector<float> samplesCopy = m_decodedSamples; // copy for thread safety

    QThread* thread = QThread::create([this, tempo, pitchSt, samplesCopy]() {
        // Run SoundTouch in background
        soundtouch::SoundTouch st;
        st.setSampleRate(44100);
        st.setChannels(1);
        st.setTempo(tempo);
        st.setPitchSemiTones(pitchSt);
        st.putSamples(samplesCopy.constData(), samplesCopy.size());
        st.flush();

        QByteArray pcmData;
        int bufferSize = 4096;
        std::vector<float> floatBuffer(bufferSize);
        int received = 0;
        do {
            received = st.receiveSamples(floatBuffer.data(), bufferSize);
            if (received > 0) {
                int bytesToWrite = received * sizeof(qint16);
                pcmData.resize(pcmData.size() + bytesToWrite);
                qint16 *outPtr = reinterpret_cast<qint16*>(pcmData.data() + pcmData.size() - bytesToWrite);
                for (int i = 0; i < received; ++i) {
                    float sample = std::max(-1.0f, std::min(1.0f, floatBuffer[i]));
                    outPtr[i] = static_cast<qint16>(sample * 32767.0f);
                }
            }
        } while (received > 0);

        // Deliver result on main thread
        QMetaObject::invokeMethod(this, [this, pcmData]() {
            m_preprocessedPcmData = pcmData;
            m_processing = false;
            QApplication::restoreOverrideCursor();
            if (m_processBar) {
                m_processBar->setVisible(false);
            }

            // Re-analyze chords on the pitch-shifted audio and update the waveform
            if (!pcmData.isEmpty() && m_waveformWidget) {
                QVector<float> pitchedSamples;
                pitchedSamples.reserve(pcmData.size() / sizeof(qint16));
                const qint16* src = reinterpret_cast<const qint16*>(pcmData.constData());
                int count = pcmData.size() / sizeof(qint16);
                for (int i = 0; i < count; ++i)
                    pitchedSamples.append(src[i] / 32768.0f);

                double totalDurationSec = pitchedSamples.size() / 44100.0;
                QString hopStr = m_hopSizeCombo ? m_hopSizeCombo->currentText() : "1s";
                hopStr.remove('s');
                double hopSec = hopStr.toDouble();
                int fftVal = m_fftSizes[m_fftSizeCombo ? m_fftSizeCombo->currentIndex() : 4];

                if (m_statusBar) m_statusBar->showMessage("Extracting chords for new pitch...");
                m_extractedChords = analyzeAudioChords(pitchedSamples, totalDurationSec, 44100, hopSec, fftVal);
                m_waveformWidget->setChords(m_extractedChords);
                if (m_waveformWidget) m_waveformWidget->setPitchShift(m_currentPitch);
            }

            if (m_statusBar) { m_statusBar->setStyleSheet("QStatusBar { background-color: #09090b; color: #f59e0b; font-family: monospace; font-size: 12px; }"); m_statusBar->showMessage("Ready..."); }

            // Re-enable play button
            if (m_playPauseBtn) {
                m_playPauseBtn->setEnabled(true);
            }

            // If we were playing before processing, reload buffer, restore position and resume
            if (m_resumeAfterProcess) {
                m_resumeAfterProcess = false;
                // togglePlayPause will recreate the buffer from m_preprocessedPcmData
                // and seek to the saved m_currentPositionSec (accounting for tempo)
                togglePlayPause();
            }
        }, Qt::QueuedConnection);
    });

    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

// ========================================================
// MAIN WINDOW IMPLEMENTATION
// ========================================================
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);

    setupMenuBar();
    setupUi();
    setWindowIcon(createAppIcon());

    m_playbackTimer = new QTimer(this);
        connect(m_playbackTimer, &QTimer::timeout, [this]() {
            if (m_audioSink && m_audioSink->state() == QAudio::ActiveState) {
                // Ensure audioBufferDevice exists and is open before querying
                qint64 elapsedMs = m_playbackStartTimeMs + (m_audioSink->processedUSecs() / 1000);
                updatePosition(elapsedMs);
            }
        });
    loadSettings();
    loadLastOpenedFile();
}

void MainWindow::setupMenuBar() {
    QMenu *fileMenu = menuBar()->addMenu("&File");

    QAction *openAction = fileMenu->addAction("&File Open");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openAudioFile);

    m_recentFilesMenu = fileMenu->addMenu("Recent &Files");
    for (int i = 0; i < MaxRecentFiles; ++i) {
        m_recentFileActions[i] = new QAction(this);
        m_recentFileActions[i]->setVisible(false);
        connect(m_recentFileActions[i], &QAction::triggered, this, &MainWindow::openRecentFile);
        m_recentFilesMenu->addAction(m_recentFileActions[i]);
    }

    m_recentFilesMenu->addSeparator();
    m_clearRecentAction = m_recentFilesMenu->addAction("Clear Recent List");
    connect(m_clearRecentAction, &QAction::triggered, this, &MainWindow::clearRecentFiles);

    updateRecentFileActions();

    fileMenu->addSeparator();
    QAction *exitAction = fileMenu->addAction("E&xit");
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *aboutMenu = menuBar()->addMenu("&About");
    QAction *aboutAction = aboutMenu->addAction("&About WebRiffStudio");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);
}

void MainWindow::setupUi() {
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(12, 8, 12, 12);

    QGroupBox *waveformGroup = new QGroupBox(this);
    QVBoxLayout *waveLayout = new QVBoxLayout(waveformGroup);

    m_waveformWidget = new GLWaveformWidget(this);
    m_waveformScrollBar = new QScrollBar(Qt::Horizontal, this);
    m_waveformScrollBar->setRange(0, 0);

    waveLayout->addWidget(m_waveformWidget);
    waveLayout->addWidget(m_waveformScrollBar);

    QHBoxLayout *sliderLayout = new QHBoxLayout();
    QLabel *sliderTitle = new QLabel("View Fraction:", this);
    sliderTitle->setStyleSheet("color: #a1a1aa; font-weight: bold; font-size: 11px;");

    m_timelineSlider = new QSlider(Qt::Horizontal, this);
    m_timelineSlider->setRange(10, 100);
    m_timelineSlider->setInvertedAppearance(true);
    m_timelineSlider->setValue(10);
    m_timelineSlider->setTickPosition(QSlider::TicksBelow);
    m_timelineSlider->setTickInterval(10);
    m_timelineSlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 6px; background: #27272a; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #f59e0b; width: 14px; margin: -4px 0; border-radius: 7px; }"
    );

    m_timelineValueLabel = new QLabel("0.1 (10%)", this);
    m_timelineValueLabel->setStyleSheet("color: #f59e0b; font-family: monospace; font-weight: bold; font-size: 11px;");
    m_timelineValueLabel->setFixedWidth(80);

    sliderLayout->addWidget(sliderTitle);
    sliderLayout->addWidget(m_timelineSlider);
    sliderLayout->addWidget(m_timelineValueLabel);

    waveLayout->addLayout(sliderLayout);
    mainLayout->addWidget(waveformGroup);

    connect(m_waveformWidget, &GLWaveformWidget::maxScrollChanged, [this](int maxScroll, int pageStep) {
        m_waveformScrollBar->setRange(0, maxScroll);
        m_waveformScrollBar->setPageStep(pageStep);
        m_waveformScrollBar->setEnabled(maxScroll > 0);
    });

    connect(m_waveformScrollBar, &QScrollBar::valueChanged, m_waveformWidget, &GLWaveformWidget::setScrollOffset);
    connect(m_waveformWidget, &GLWaveformWidget::scrollOffsetChanged, m_waveformScrollBar, &QScrollBar::setValue);

    connect(m_waveformWidget, &GLWaveformWidget::seekRequested, [this](double timeSeconds) {
        double totalSec = m_player ? (m_player->duration() / 1000.0) : 0.0;
        if (totalSec <= 0 && !m_decodedSamples.isEmpty()) {
            totalSec = m_decodedSamples.size() / 44100.0;
        }

        m_currentPositionSec = qMax(0.0, qMin(timeSeconds, totalSec));
        m_isPaused = false;
        qint64 posMs = static_cast<qint64>(m_currentPositionSec * 1000);

        if (m_audioSink && m_audioSink->state() == QAudio::ActiveState) {
            m_audioSink->stop();
            if (m_playbackTimer) m_playbackTimer->stop();

            float tempo = m_currentSpeed;
            double effectiveSec = m_currentPositionSec * tempo;
            qint64 byteOffset = static_cast<qint64>(effectiveSec * 44100.0 * sizeof(qint16));
            byteOffset = (byteOffset / sizeof(qint16)) * sizeof(qint16);
            byteOffset = qMax(static_cast<qint64>(0), qMin(byteOffset, static_cast<qint64>(m_preprocessedPcmData.size())));

            if (m_audioBufferDevice) {
                m_audioBufferDevice->seek(byteOffset);
            }
            m_playbackStartTimeMs = static_cast<qint64>(m_currentPositionSec * 1000);

            m_audioSink->start(m_audioBufferDevice);
            if (m_playbackTimer) m_playbackTimer->start(50);
        } else {
            updatePosition(posMs);
        }
    });

    connect(m_waveformWidget, &GLWaveformWidget::selectionChanged, this, &MainWindow::handleSelectionChanged);
    connect(m_timelineSlider, &QSlider::valueChanged, this, &MainWindow::onTimelineSliderChanged);

    QHBoxLayout *controlsLayout = new QHBoxLayout();

    m_tempoGroup = new QGroupBox("TEMPO --", this);
    QVBoxLayout *tempoLayout = new QVBoxLayout(m_tempoGroup);

    QHBoxLayout *speedBtnLayout = new QHBoxLayout();
    speedBtnLayout->setSpacing(4);
    const QString speedLabels[] = {"0.2", "0.4", "0.6", "0.8", "1", "1.2", "1.4"};
    const float speedValues[] = {0.2f, 0.4f, 0.6f, 0.8f, 1.0f, 1.2f, 1.4f};

    QString btnStyle =
        "QPushButton { background-color: #18181b; color: #f59e0b; border: 1px solid #3f3f46; border-radius: 4px; font-weight: bold; font-size: 12px; min-width: 42px; min-height: 32px; }"
        "QPushButton:checked { background-color: #d97706; color: #09090b; }"
        "QPushButton:disabled { background-color: #27272a; color: #52525b; border-color: #3f3f46; }";

    for (int i = 0; i < 7; ++i) {
        m_speedButtons[i] = new QPushButton(speedLabels[i], this);
        m_speedButtons[i]->setCheckable(true);
        m_speedButtons[i]->setStyleSheet(btnStyle);
        if (i == 4) m_speedButtons[i]->setChecked(true); // default 1.0
        speedBtnLayout->addWidget(m_speedButtons[i]);

        connect(m_speedButtons[i], &QPushButton::clicked, this, [this, i, speedValues]() {
            for (int j = 0; j < 7; ++j)
                m_speedButtons[j]->setChecked(j == i);
            m_currentSpeed = speedValues[i];
            m_speedValueLabel->setText(QString("Speed: %1x").arg(speedValues[i], 0, 'f', 1));
            updateAudioEffects();
        });
    }

    m_speedValueLabel = new QLabel("Speed: 1.0x", this);
    m_speedValueLabel->setAlignment(Qt::AlignCenter);
    m_speedValueLabel->setStyleSheet("color: #f59e0b; font-weight: bold; font-size: 16px; font-family: monospace;");

    tempoLayout->addStretch();
    tempoLayout->addLayout(speedBtnLayout);
    tempoLayout->addWidget(m_speedValueLabel);
    tempoLayout->addStretch();

    m_fretboardWidget = new FretboardWidget(this);

    QGroupBox *pitchGroup = new QGroupBox("PITCH", this);
    QVBoxLayout *pitchLayout = new QVBoxLayout(pitchGroup);

    QHBoxLayout *pitchBtnLayout = new QHBoxLayout();
    pitchBtnLayout->setSpacing(4);
    const QString pitchLabels[] = {"-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4"};
    const float pitchValues[] = {-4.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f};

    QString pitchBtnStyle =
        "QPushButton { background-color: #18181b; color: #f59e0b; border: 1px solid #3f3f46; border-radius: 4px; font-weight: bold; font-size: 12px; min-width: 42px; min-height: 32px; }"
        "QPushButton:checked { background-color: #d97706; color: #09090b; }"
        "QPushButton:disabled { background-color: #27272a; color: #52525b; border-color: #3f3f46; }";

    for (int i = 0; i < 9; ++i) {
        m_pitchButtons[i] = new QPushButton(pitchLabels[i], this);
        m_pitchButtons[i]->setCheckable(true);
        m_pitchButtons[i]->setStyleSheet(pitchBtnStyle);
        if (i == 4) m_pitchButtons[i]->setChecked(true); // default 0
        pitchBtnLayout->addWidget(m_pitchButtons[i]);

        connect(m_pitchButtons[i], &QPushButton::clicked, this, [this, i, pitchValues]() {
            for (int j = 0; j < 9; ++j)
                m_pitchButtons[j]->setChecked(j == i);
            m_currentPitch = pitchValues[i];
            QString sign = (pitchValues[i] > 0) ? "+" : "";
            m_pitchValueLabel->setText(QString("Pitch: %1%2 st").arg(sign).arg(pitchValues[i], 0, 'f', 1));
            if (m_waveformWidget) m_waveformWidget->setPitchShift(m_currentPitch);
            updateAudioEffects();
        });
    }

    m_pitchValueLabel = new QLabel("Pitch: 0.0 st", this);
    m_pitchValueLabel->setAlignment(Qt::AlignCenter);
    m_pitchValueLabel->setStyleSheet("color: #f59e0b; font-weight: bold; font-size: 16px; font-family: monospace;");

    pitchLayout->addStretch();
    pitchLayout->addLayout(pitchBtnLayout);
    pitchLayout->addWidget(m_pitchValueLabel);
    pitchLayout->addStretch();

    controlsLayout->addWidget(m_tempoGroup, 1);
    controlsLayout->addWidget(m_fretboardWidget, 2);
    controlsLayout->addWidget(pitchGroup, 1);

    mainLayout->addLayout(controlsLayout);

    QHBoxLayout *bottomControlBar = new QHBoxLayout();

    m_fingeringGroup = new QGroupBox("FINGERING", this);
    m_fingeringGroup->setStyleSheet(
        "QGroupBox { border: 1.5px solid #3f3f46; border-radius: 6px; margin-top: 6px; font-weight: bold; font-size: 10px; color: #a1a1aa; } "
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }"
    );

    QVBoxLayout *fingeringLayout = new QVBoxLayout(m_fingeringGroup);
    m_fingeringLabel = new QLabel("Mode: Chord", this);
    m_fingeringLabel->setAlignment(Qt::AlignCenter);
    m_fingeringLabel->setStyleSheet("color: #f59e0b; font-weight: bold; font-size: 11px; font-family: monospace;");

    m_fingeringToggleBtn = new QPushButton("Chord", this);
    m_fingeringToggleBtn->setCheckable(true);
    m_fingeringToggleBtn->setChecked(false);
    m_fingeringToggleBtn->setFixedSize(140, 36);
    m_fingeringToggleBtn->setStyleSheet(
        "QPushButton { background-color: #18181b; color: #f59e0b; border: 1px solid #3f3f46; border-radius: 4px; font-weight: bold; font-size: 11px; } "
        "QPushButton:checked { background-color: #d97706; color: #09090b; }"
    );

    fingeringLayout->addWidget(m_fingeringLabel);
    fingeringLayout->addWidget(m_fingeringToggleBtn, 0, Qt::AlignCenter);

    connect(m_fingeringToggleBtn, &QPushButton::toggled, [this](bool checked) {
        m_fingeringToggleBtn->setText(checked ? "Power" : "Chord");
        m_fingeringLabel->setText(checked ? "Mode: Power" : "Mode: Chord");
        m_fretboardWidget->setFingeringMode(checked);
    });

    QVBoxLayout *volumeLayout = new QVBoxLayout();
    m_volumeValueLabel = new QLabel("Volume: 80%", this);
    m_volumeValueLabel->setAlignment(Qt::AlignCenter);
    m_volumeValueLabel->setStyleSheet("color: #f59e0b; font-weight: bold; font-size: 22px; font-family: monospace;");

    m_volumeKnob = new KnobWidget(this);
    m_volumeKnob->setRange(0, 100);
    m_volumeKnob->setValue(80);
    m_volumeKnob->setFixedSize(70, 70);

    volumeLayout->addWidget(m_volumeValueLabel);
    volumeLayout->addWidget(m_volumeKnob, 0, Qt::AlignCenter);

    m_chordExtractionGroup = new QGroupBox("CHORD EXTRACTION", this);
    m_chordExtractionGroup->setStyleSheet(
        "QGroupBox { border: 1.5px solid #3f3f46; border-radius: 6px; margin-top: 6px; font-weight: bold; font-size: 10px; color: #a1a1aa; } "
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }"
    );

    QHBoxLayout *chordControlsLayout = new QHBoxLayout(m_chordExtractionGroup);
    QString comboStyle =
        "QComboBox { background-color: #18181b; color: #f59e0b; border: 1px solid #3f3f46; border-radius: 4px; padding: 3px 8px; font-family: monospace; font-weight: bold; font-size: 11px; } "
        "QComboBox::drop-down { border: none; } "
        "QComboBox QAbstractItemView { background-color: #18181b; color: #f59e0b; selection-background-color: #3f3f46; }";

    QVBoxLayout *hopLayout = new QVBoxLayout();
    m_hopSizeLabel = new QLabel("Hop Size:", this);
    m_hopSizeLabel->setAlignment(Qt::AlignCenter);
    m_hopSizeLabel->setStyleSheet("color: #a1a1aa; font-weight: bold; font-size: 10px;");

    m_hopSizeCombo = new QComboBox(this);
    m_hopSizeCombo->addItems({"0.5s", "1s", "1.5s", "2s"});
    m_hopSizeCombo->setCurrentIndex(1);
    m_hopSizeCombo->setStyleSheet(comboStyle);
    m_hopSizeCombo->setFixedWidth(80);

    hopLayout->addWidget(m_hopSizeLabel);
    hopLayout->addWidget(m_hopSizeCombo, 0, Qt::AlignCenter);

    QVBoxLayout *fftLayout = new QVBoxLayout();
    m_fftSizeLabel = new QLabel("FFT Size:", this);
    m_fftSizeLabel->setAlignment(Qt::AlignCenter);
    m_fftSizeLabel->setStyleSheet("color: #a1a1aa; font-weight: bold; font-size: 10px;");

    m_fftSizeCombo = new QComboBox(this);
    m_fftSizeCombo->addItems({"512", "1024", "2048", "4096", "8192", "16384"});
    m_fftSizeCombo->setCurrentIndex(4);
    m_fftSizeCombo->setStyleSheet(comboStyle);
    m_fftSizeCombo->setFixedWidth(80);

    fftLayout->addWidget(m_fftSizeLabel);
    fftLayout->addWidget(m_fftSizeCombo, 0, Qt::AlignCenter);

    chordControlsLayout->addLayout(hopLayout);
    chordControlsLayout->addLayout(fftLayout);

    bottomControlBar->addStretch();
    bottomControlBar->addWidget(m_fingeringGroup, 1);
    bottomControlBar->addSpacing(16);
    bottomControlBar->addLayout(volumeLayout, 0);
    bottomControlBar->addSpacing(16);
    bottomControlBar->addWidget(m_chordExtractionGroup, 1);
    bottomControlBar->addStretch();

    QVBoxLayout *centerTransportLayout = new QVBoxLayout();
    QHBoxLayout *transportButtonsLayout = new QHBoxLayout();

    m_playPauseBtn = new QPushButton(this);
    m_stopBtn = new QPushButton(this);
    m_repeatBtn = new QPushButton(this);

    m_playPauseBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_stopBtn->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    m_repeatBtn->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));

    m_repeatBtn->setCheckable(true);
    m_repeatBtn->setEnabled(false);

    m_playPauseBtn->setFixedSize(96, 36);
    m_stopBtn->setFixedSize(96, 36);
    m_repeatBtn->setFixedSize(96, 36);

    m_timeDisplay = new QLabel("0:00 / 0:00", this);
    m_timeDisplay->setStyleSheet("background-color: #000000; color: #f59e0b; font-family: monospace; font-size: 14px; padding: 4px 12px; border-radius: 4px;");

    transportButtonsLayout->addStretch();
    transportButtonsLayout->addWidget(m_playPauseBtn);
    transportButtonsLayout->addWidget(m_stopBtn);
    transportButtonsLayout->addWidget(m_repeatBtn);
    transportButtonsLayout->addWidget(m_timeDisplay);
    transportButtonsLayout->addStretch();

    centerTransportLayout->addLayout(bottomControlBar);
    centerTransportLayout->addLayout(transportButtonsLayout);
    mainLayout->addLayout(centerTransportLayout);

    setCentralWidget(centralWidget);

    connect(m_playPauseBtn, &QPushButton::clicked, this, &MainWindow::togglePlayPause);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::stopAudio);





    connect(m_volumeKnob, &QDial::valueChanged, [this](int val) {
        float vol = val / 100.0f;
        if (m_audioSink) m_audioSink->setVolume(vol);
        m_volumeValueLabel->setText(QString("Volume: %1%").arg(val));
    });

    auto reanalyzeChords = [this]() {
        QString hopStr = m_hopSizeCombo->currentText();
        hopStr.remove('s');
        double hopSec = hopStr.toDouble();
        int fftVal = m_fftSizes[m_fftSizeCombo->currentIndex()];

        if (!m_decodedSamples.isEmpty()) {
            double totalDurationSec = m_decodedSamples.size() / 44100.0;
            if (m_statusBar) m_statusBar->showMessage("Extracting chords, please wait...");
            m_extractedChords = analyzeAudioChords(m_decodedSamples, totalDurationSec, 44100, hopSec, fftVal);
            if (m_statusBar) { m_statusBar->setStyleSheet("QStatusBar { background-color: #09090b; color: #f59e0b; font-family: monospace; font-size: 12px; }"); m_statusBar->showMessage("Ready..."); }
            m_waveformWidget->setChords(m_extractedChords);
                if (m_waveformWidget) m_waveformWidget->setPitchShift(m_currentPitch);
        }
    };

    connect(m_hopSizeCombo, &QComboBox::currentIndexChanged, reanalyzeChords);
    connect(m_fftSizeCombo, &QComboBox::currentIndexChanged, reanalyzeChords);

    // Status bar
    m_statusBar = new QStatusBar(this);
    m_statusBar->setStyleSheet("QStatusBar { background-color: #09090b; color: #f59e0b; font-family: monospace; font-size: 12px; }");
    m_statusBar->showMessage("Ready...");

    m_themeToggleBtn = new QPushButton("ORANGE", this);
    m_themeToggleBtn->setCheckable(true);
    m_themeToggleBtn->setFixedSize(80, 22);
    m_themeToggleBtn->setStyleSheet(
        "QPushButton { background-color: #f59e0b; color: #09090b; border: none; border-radius: 4px; font-weight: bold; font-size: 10px; }"
        "QPushButton:checked { background-color: #22d3ee; color: #09090b; }"
    );
    m_statusBar->addPermanentWidget(m_themeToggleBtn);
    connect(m_themeToggleBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_cyanTheme = checked;
        m_themeToggleBtn->setText(checked ? "CYAN" : "ORANGE");
        applyTheme(checked);
    });

    setStatusBar(m_statusBar);

    // 2-pixel processing progress bar under the status bar
    m_processBar = new QProgressBar(this);
    m_processBar->setFixedHeight(2);
    m_processBar->setTextVisible(false);
    m_processBar->setRange(0, 0); // indeterminate / busy indicator
    m_processBar->setStyleSheet(
        "QProgressBar { border: none; background-color: #09090b; }"
        "QProgressBar::chunk { background-color: #f59e0b; }"
    );
    m_processBar->setVisible(false);
    m_processBar->raise();
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (m_processBar) {
        // Position the 2px bar at the very bottom of the window, under the status bar
        m_processBar->setGeometry(0, height() - 2, width(), 2);
    }
}

void MainWindow::updatePosition(qint64 position) {
    double currentSec = position / 1000.0;
    double totalSec = m_decodedSamples.isEmpty() ? 0.0 : (m_decodedSamples.size() / 44100.0);

    // Fixed: Only trigger loop when currentSec reaches m_loopEndSec (removed currentSec < m_loopStartSec check)
    if (m_repeatBtn->isChecked() && m_loopStartSec >= 0 && m_loopEndSec > m_loopStartSec) {
        if (currentSec >= m_loopEndSec) {
            m_currentPositionSec = m_loopStartSec;
            qint64 posMs = static_cast<qint64>(m_loopStartSec * 1000);
            if (m_audioSink && m_audioSink->state() == QAudio::ActiveState) {
                m_audioSink->stop();
                float tempo = m_currentSpeed;
                double effectiveSec = m_loopStartSec * tempo;
                qint64 byteOffset = static_cast<qint64>(effectiveSec * 44100.0 * sizeof(qint16));
                byteOffset = (byteOffset / sizeof(qint16)) * sizeof(qint16);
                if (m_audioBufferDevice) m_audioBufferDevice->seek(byteOffset);
                m_playbackStartTimeMs = posMs;
                m_audioSink->start(m_audioBufferDevice);
            }
            currentSec = m_loopStartSec;
        }
    }

     m_waveformWidget->setPlaybackPosition(currentSec);

    if (!m_extractedChords.isEmpty()) {
        QString currentChord = m_extractedChords.first().chordName;
        QString nextChord = (m_extractedChords.size() > 1) ? m_extractedChords[1].chordName : "";

        for (int i = 0; i < m_extractedChords.size(); ++i) {
            double chordStart = m_extractedChords[i].timeSeconds;
            double chordEnd = (i + 1 < m_extractedChords.size()) ? m_extractedChords[i + 1].timeSeconds : totalSec;

            if (currentSec >= chordStart && currentSec < chordEnd) {
                currentChord = m_extractedChords[i].chordName;
                nextChord = (i + 1 < m_extractedChords.size()) ? m_extractedChords[i + 1].chordName : "";
                break;
            }
        }
        m_fretboardWidget->setChord(currentChord, nextChord);
    }

    int curM = static_cast<int>(currentSec) / 60;
    int curS = static_cast<int>(currentSec) % 60;
    int totM = static_cast<int>(totalSec) / 60;
    int totS = static_cast<int>(totalSec) % 60;

    m_timeDisplay->setText(QString("%1:%2 / %3:%4")
                           .arg(curM, 2, 10, QChar('0'))
                           .arg(curS, 2, 10, QChar('0'))
                           .arg(totM, 2, 10, QChar('0'))
                           .arg(totS, 2, 10, QChar('0')));
}

void MainWindow::handleSelectionChanged(double startSec, double endSec) {
    if (endSec > startSec + 0.1) {
        m_loopStartSec = startSec;
        m_loopEndSec = endSec;
        m_repeatBtn->setEnabled(true);
    } else {
        m_loopStartSec = -1.0;
        m_loopEndSec = -1.0;
        //m_repeatBtn->setChecked(false);
        m_repeatBtn->setEnabled(false);
    }
}

void MainWindow::onTimelineSliderChanged(int value) {
    float fraction = value / 100.0f;
    m_timelineValueLabel->setText(QString("%1 (%2%)").arg(fraction, 0, 'f', 1).arg(value));
    m_waveformWidget->setVisibleFraction(fraction);
}

void MainWindow::togglePlayPause() {
    if (m_decodedSamples.isEmpty()) return;

    if (m_audioSink && m_audioSink->state() == QAudio::ActiveState) {
        m_audioSink->stop();
        if (m_playbackTimer) m_playbackTimer->stop();

        qint64 elapsedMs = m_playbackStartTimeMs + (m_audioSink->processedUSecs() / 1000);
        m_currentPositionSec = elapsedMs / 1000.0;
        m_isPaused = true;

        for (int i = 0; i < 7; ++i) if (m_speedButtons[i]) m_speedButtons[i]->setEnabled(true);
        for (int i = 0; i < 9; ++i) if (m_pitchButtons[i]) m_pitchButtons[i]->setEnabled(true);



        m_playPauseBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        return;
    }

    float tempo = m_currentSpeed;
    float pitchSt = m_currentPitch;

    QAudioDevice defaultDeviceInfo = QMediaDevices::defaultAudioOutput();
    QAudioFormat format = defaultDeviceInfo.preferredFormat();
    if (format.sampleRate() <= 0) format.setSampleRate(44100);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    if (!m_audioSink) {
        m_audioSink = new QAudioSink(defaultDeviceInfo, format, this);
    } else {
        m_audioSink->stop();
    }

    if (m_volumeKnob) {
        m_audioSink->setVolume(m_volumeKnob->value() / 100.0f);
    }

    if (m_preprocessedPcmData.isEmpty()) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        m_preprocessedPcmData = processAudioWithSoundTouch(tempo, pitchSt);
        QApplication::restoreOverrideCursor();
    }

    if (m_preprocessedPcmData.isEmpty()) return;

    if (m_audioBufferDevice) {
        m_audioBufferDevice->close();
        delete m_audioBufferDevice;
    }
    m_audioBufferDevice = new QBuffer(this);
    m_audioBufferDevice->setData(m_preprocessedPcmData);
    if (!m_audioBufferDevice->open(QIODevice::ReadOnly)) return;

    qint64 byteOffset = 0;
    if (m_isPaused) {
        double effectiveSec = m_currentPositionSec * tempo;
        byteOffset = static_cast<qint64>(effectiveSec * 44100.0 * sizeof(qint16));
    } else {
        if (m_loopStartSec >= 0 && m_loopEndSec > m_loopStartSec) {
            m_currentPositionSec = m_loopStartSec;
        } else if (m_currentPositionSec < 0.0) {
            m_currentPositionSec = 0.0;
        }
        double effectiveSec = m_currentPositionSec * tempo;
        byteOffset = static_cast<qint64>(effectiveSec * 44100.0 * sizeof(qint16));
    }

    byteOffset = (byteOffset / sizeof(qint16)) * sizeof(qint16);
    byteOffset = qMax(static_cast<qint64>(0), qMin(byteOffset, static_cast<qint64>(m_preprocessedPcmData.size())));

    m_audioBufferDevice->seek(byteOffset);
    m_playbackStartTimeMs = static_cast<qint64>(m_currentPositionSec * 1000);

    m_audioSink->start(m_audioBufferDevice);

    if (m_audioSink->error() == QAudio::NoError) {
        m_isPaused = false;
        if (m_playbackTimer) m_playbackTimer->start(50);

        // Speed/Pitch buttons stay enabled during playback
        QString greyStyle = "QSlider::handle:horizontal { background: #52525b; } QSlider::sub-page:horizontal { background: #52525b; }";



        m_playPauseBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    }
}

void MainWindow::stopAudio() {
    if (m_playbackTimer) {
        m_playbackTimer->stop();
    }
    if (m_audioSink) {
        m_audioSink->stop();
    }
    if (m_audioBufferDevice) {
        m_audioBufferDevice->close();
    }
    m_isPaused = false;
    m_currentPositionSec = 0.0;
    m_resumeAfterProcess = false;

    for (int i = 0; i < 7; ++i) if (m_speedButtons[i]) m_speedButtons[i]->setEnabled(true);
    for (int i = 0; i < 9; ++i) if (m_pitchButtons[i]) m_pitchButtons[i]->setEnabled(true);

    if (m_playPauseBtn) {
        m_playPauseBtn->setEnabled(true);
        m_playPauseBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    }
    updatePosition(0);
}

void MainWindow::updateAudioEffects() {
    // Offload heavy SoundTouch processing to background thread
    updatePreprocessedAudioAsync();
}

void MainWindow::openAudioFile() {
    QString fileName = QFileDialog::getOpenFileName(this, "File Open", "", "Audio Files (*.mp3 *.wav *.ogg *.flac)");
    if (!fileName.isEmpty()) {
        loadAudioFile(fileName);
    }
}

void MainWindow::loadAudioFile(const QString &filePath) {
    stopAudio();
    QApplication::setOverrideCursor(Qt::WaitCursor);

    m_player->setSource(QUrl::fromLocalFile(filePath));
    m_decodedSamples.clear();
    m_preprocessedPcmData.clear();

    if (!m_audioDecoder) {
        m_audioDecoder = new QAudioDecoder(this);
        connect(m_audioDecoder, &QAudioDecoder::bufferReady, this, &MainWindow::handleAudioBufferReady);
        connect(m_audioDecoder, &QAudioDecoder::finished, this, &MainWindow::handleDecoderFinished);
    }

    m_audioDecoder->setSource(QUrl::fromLocalFile(filePath));
    m_audioDecoder->start();

    addRecentFile(filePath);
}

void MainWindow::handleAudioBufferReady() {
    QAudioBuffer buffer = m_audioDecoder->read();
    if (!buffer.isValid()) return;

    const QAudioFormat format = buffer.format();
    int sampleCount = buffer.sampleCount();

    if (format.sampleFormat() == QAudioFormat::Int16) {
        const qint16 *data = buffer.constData<qint16>();
        for (int i = 0; i < sampleCount; i += format.channelCount()) {
            m_decodedSamples.append(data[i] / 32768.0f);
        }
    } else if (format.sampleFormat() == QAudioFormat::Float) {
        const float *data = buffer.constData<float>();
        for (int i = 0; i < sampleCount; i += format.channelCount()) {
            m_decodedSamples.append(data[i]);
        }
    }
}

void MainWindow::handleDecoderFinished() {
    double totalDurationSec = m_decodedSamples.size() / 44100.0;

    int detectedBpm = detectBpm(m_decodedSamples, 44100);
    m_tempoGroup->setTitle(QString("TEMPO %1").arg(detectedBpm));
    m_waveformWidget->setBpm(detectedBpm);

    QString hopStr = m_hopSizeCombo->currentText();
    hopStr.remove('s');
    double hopSec = hopStr.toDouble();
    int fftVal = m_fftSizes[m_fftSizeCombo->currentIndex()];

    if (m_statusBar) m_statusBar->showMessage("Extracting chords, please wait...");
            m_extractedChords = analyzeAudioChords(m_decodedSamples, totalDurationSec, 44100, hopSec, fftVal);
            if (m_statusBar) { m_statusBar->setStyleSheet("QStatusBar { background-color: #09090b; color: #f59e0b; font-family: monospace; font-size: 12px; }"); m_statusBar->showMessage("Ready..."); }

    m_waveformWidget->setAudioBuffer(m_decodedSamples, totalDurationSec, 44100);
    m_waveformWidget->setChords(m_extractedChords);
                if (m_waveformWidget) m_waveformWidget->setPitchShift(m_currentPitch);

    updatePreprocessedAudio();
    QApplication::restoreOverrideCursor();
}

void MainWindow::addRecentFile(const QString &filePath) {
    QSettings settings("WebRiffStudio", "WebRiffStudio");
    QStringList files = settings.value("recentFileList").toStringList();

    files.removeAll(filePath);
    files.prepend(filePath);
    while (files.size() > MaxRecentFiles) {
        files.removeLast();
    }

    settings.setValue("recentFileList", files);
    updateRecentFileActions();
}

void MainWindow::updateRecentFileActions() {
    QSettings settings("WebRiffStudio", "WebRiffStudio");
    QStringList files = settings.value("recentFileList").toStringList();

    int numRecentFiles = qMin(files.size(), (int)MaxRecentFiles);

    for (int i = 0; i < numRecentFiles; ++i) {
        QString text = QString("&%1 %2").arg(i + 1).arg(QFileInfo(files[i]).fileName());
        m_recentFileActions[i]->setText(text);
        m_recentFileActions[i]->setData(files[i]);
        m_recentFileActions[i]->setVisible(true);
    }

    for (int j = numRecentFiles; j < MaxRecentFiles; ++j) {
        m_recentFileActions[j]->setVisible(false);
    }

    m_recentFilesMenu->setEnabled(numRecentFiles > 0);
}

void MainWindow::openRecentFile() {
    QAction *action = qobject_cast<QAction *>(sender());
    if (action) {
        QString filePath = action->data().toString();
        if (QFileInfo::exists(filePath)) {
            loadAudioFile(filePath);
        } else {
            QSettings settings("WebRiffStudio", "WebRiffStudio");
            QStringList files = settings.value("recentFileList").toStringList();
            files.removeAll(filePath);
            settings.setValue("recentFileList", files);
            updateRecentFileActions();
        }
    }
}

void MainWindow::clearRecentFiles() {
    QSettings settings("WebRiffStudio", "WebRiffStudio");
    settings.remove("recentFileList");
    updateRecentFileActions();
}

void MainWindow::loadLastOpenedFile() {
    QSettings settings("WebRiffStudio", "WebRiffStudio");
    QStringList files = settings.value("recentFileList").toStringList();

    if (!files.isEmpty()) {
        QString lastFile = files.first();
        if (QFileInfo::exists(lastFile)) {
            loadAudioFile(lastFile);
        }
    }
}

QIcon MainWindow::createAppIcon() {
    QPixmap pixmap(128, 128);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath pickPath;
    pickPath.moveTo(64, 120);
    pickPath.cubicTo(10, 80, 8, 20, 64, 12);
    pickPath.cubicTo(120, 20, 118, 80, 64, 120);

    QLinearGradient grad(0, 0, 128, 128);
    grad.setColorAt(0.0, QColor(245, 158, 11));
    grad.setColorAt(1.0, QColor(180, 83, 9));

    painter.setBrush(grad);
    painter.setPen(QPen(QColor(251, 191, 36), 3));
    painter.drawPath(pickPath);

    painter.setBrush(QColor(18, 18, 22));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(64, 58), 34, 34);

    painter.setPen(QPen(QColor(245, 158, 11), 3, Qt::SolidLine, Qt::RoundCap));
    int heights[] = {12, 24, 40, 20, 32, 16};
    int xPos[] = {44, 52, 60, 68, 76, 84};
    for (int i = 0; i < 6; ++i) {
        painter.drawLine(xPos[i], 58 - heights[i] / 2, xPos[i], 58 + heights[i] / 2);
    }

    return QIcon(pixmap);
}

void MainWindow::showAboutDialog() {
    QDialog aboutDialog(this);
    aboutDialog.setWindowTitle("About WebRiffStudio");
    aboutDialog.setFixedSize(360, 220);
    aboutDialog.setStyleSheet("background-color: #09090b; color: #f4f4f5;");

    QVBoxLayout *layout = new QVBoxLayout(&aboutDialog);
    layout->setContentsMargins(20, 20, 20, 20);

    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *iconLabel = new QLabel(&aboutDialog);
    iconLabel->setPixmap(createAppIcon().pixmap(64, 64));

    QVBoxLayout *textLayout = new QVBoxLayout();
    QLabel *titleLabel = new QLabel("<b>WebRiffStudio</b>", &aboutDialog);
    titleLabel->setStyleSheet("font-size: 18px; color: #f59e0b;");

    QLabel *verLabel = new QLabel("Version 1.0.0", &aboutDialog);
    verLabel->setStyleSheet("font-size: 12px; color: #a1a1aa; font-family: monospace;");

    QLabel *copyLabel = new QLabel("Copyright © 2026<br><b>Ghitara.com & AIurea-Music</b>", &aboutDialog);
    copyLabel->setStyleSheet("font-size: 12px; color: #d4d4d8;");

    textLayout->addWidget(titleLabel);
    textLayout->addWidget(verLabel);
    textLayout->addWidget(copyLabel);

    headerLayout->addWidget(iconLabel);
    headerLayout->addSpacing(16);
    headerLayout->addLayout(textLayout);
    headerLayout->addStretch();

    QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok, &aboutDialog);
    btnBox->setStyleSheet(
        "QPushButton { background-color: #27272a; color: #f4f4f5; border: 1px solid #3f3f46; padding: 6px 16px; border-radius: 4px; font-weight: bold; } "
        "QPushButton:hover { background-color: #3f3f46; }"
    );
    connect(btnBox, &QDialogButtonBox::accepted, &aboutDialog, &QDialog::accept);

    layout->addLayout(headerLayout);
    layout->addStretch();
    layout->addWidget(btnBox, 0, Qt::AlignRight);

    aboutDialog.exec();
}

void MainWindow::saveSettings() {
    QSettings settings("WebRiffStudio", "WebRiffStudio");
    settings.setValue("theme/cyan", m_cyanTheme);
    settings.setValue("fingering/power", m_fingeringToggleBtn ? m_fingeringToggleBtn->isChecked() : false);
    settings.setValue("chords/hopSizeIndex", m_hopSizeCombo ? m_hopSizeCombo->currentIndex() : 1);
    settings.setValue("chords/fftSizeIndex", m_fftSizeCombo ? m_fftSizeCombo->currentIndex() : 4);
}

void MainWindow::loadSettings() {
    QSettings settings("WebRiffStudio", "WebRiffStudio");

    // Theme
    bool cyan = settings.value("theme/cyan", false).toBool();
    m_cyanTheme = cyan;
    if (m_themeToggleBtn) {
        m_themeToggleBtn->setChecked(cyan);
        m_themeToggleBtn->setText(cyan ? "CYAN" : "ORANGE");
    }
    applyTheme(cyan);

    // Power / Chord mode
    bool power = settings.value("fingering/power", false).toBool();
    if (m_fingeringToggleBtn) {
        m_fingeringToggleBtn->setChecked(power);
        m_fingeringToggleBtn->setText(power ? "Power" : "Chord");
        if (m_fretboardWidget) m_fretboardWidget->setFingeringMode(power);
    }

    // Hop size & FFT size
    int hopIdx = settings.value("chords/hopSizeIndex", 1).toInt();
    int fftIdx = settings.value("chords/fftSizeIndex", 4).toInt();
    if (m_hopSizeCombo && hopIdx >= 0 && hopIdx < m_hopSizeCombo->count())
        m_hopSizeCombo->setCurrentIndex(hopIdx);
    if (m_fftSizeCombo && fftIdx >= 0 && fftIdx < m_fftSizeCombo->count())
        m_fftSizeCombo->setCurrentIndex(fftIdx);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    saveSettings();
    QMainWindow::closeEvent(event);
}
