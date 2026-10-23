#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);

    m_networkManager = new QNetworkAccessManager(this);

    setupUi();
    fetchPageCounter();

    connect(m_player, &QMediaPlayer::positionChanged, this, &MainWindow::updatePosition);
}

void MainWindow::setupUi() {
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    m_songTitleLabel = new QLabel("Select an audio track to analyze", this);
    m_songTitleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #f59e0b;");
    mainLayout->addWidget(m_songTitleLabel);

    // Waveform Controls Toolbar
    QHBoxLayout *waveToolLayout = new QHBoxLayout();
    m_drawToggleBtn = new QPushButton("Draw: ON", this);
    m_drawToggleBtn->setCheckable(true);
    m_drawToggleBtn->setChecked(true);
    m_drawToggleBtn->setStyleSheet("background-color: #d97706; color: #09090b; font-weight: bold; padding: 6px 12px;");

    m_colorToggleBtn = new QPushButton("Color: ON", this);
    m_colorToggleBtn->setCheckable(true);
    m_colorToggleBtn->setChecked(true);
    m_colorToggleBtn->setStyleSheet("background-color: #d97706; color: #09090b; font-weight: bold; padding: 6px 12px;");

    waveToolLayout->addWidget(m_drawToggleBtn);
    waveToolLayout->addWidget(m_colorToggleBtn);
    waveToolLayout->addStretch();
    mainLayout->addLayout(waveToolLayout);

    // Waveform & Fretboard Widgets
    m_waveformWidget = new WaveformWidget(this);
    m_fretboardWidget = new FretboardWidget(this);

    mainLayout->addWidget(m_waveformWidget);
    mainLayout->addWidget(m_fretboardWidget);

    // Transport Bar
    QHBoxLayout *transportLayout = new QHBoxLayout();
    QPushButton *openBtn = new QPushButton("Upload Song", this);
    m_playBtn = new QPushButton("Play", this);
    QPushButton *stopBtn = new QPushButton("Stop", this);

    transportLayout->addWidget(openBtn);
    transportLayout->addWidget(m_playBtn);
    transportLayout->addWidget(stopBtn);
    transportLayout->addStretch();

    mainLayout->addLayout(transportLayout);

    // Status Footer with Page Counter
    QHBoxLayout *footerLayout = new QHBoxLayout();
    QLabel *statusLabel = new QLabel("Status: Ready", this);
    m_pageCounterLabel = new QLabel("Views: ...", this);
    m_pageCounterLabel->setStyleSheet("background-color: #09090b; border: 1px solid #27272a; padding: 2px 6px; font-family: monospace;");

    footerLayout->addWidget(statusLabel);
    footerLayout->addStretch();
    footerLayout->addWidget(m_pageCounterLabel);

    mainLayout->addLayout(footerLayout);
    setCentralWidget(centralWidget);

    // Connections
    connect(openBtn, &QPushButton::clicked, this, &MainWindow::openAudioFile);
    connect(m_playBtn, &QPushButton::clicked, this, &MainWindow::togglePlayPause);
    connect(stopBtn, &QPushButton::clicked, this, &MainWindow::stopAudio);

    connect(m_drawToggleBtn, &QPushButton::toggled, [this](bool checked) {
        m_drawToggleBtn->setText(checked ? "Draw: ON" : "Draw: OFF");
        m_drawToggleBtn->setStyleSheet(checked ? "background-color: #d97706; color: #09090b; font-weight: bold; padding: 6px 12px;"
                                               : "background-color: #18181b; color: #f4f4f5; font-weight: bold; padding: 6px 12px;");
        m_waveformWidget->setDrawMode(checked);
    });

    connect(m_colorToggleBtn, &QPushButton::toggled, [this](bool checked) {
        m_colorToggleBtn->setText(checked ? "Color: ON" : "Color: OFF");
        m_colorToggleBtn->setStyleSheet(checked ? "background-color: #d97706; color: #09090b; font-weight: bold; padding: 6px 12px;"
                                                : "background-color: #18181b; color: #f4f4f5; font-weight: bold; padding: 6px 12px;");
        m_waveformWidget->setColorActive(checked);
    });

    connect(m_waveformWidget, &WaveformWidget::seekRequested, [this](double timeSeconds) {
        m_player->setPosition(static_cast<qint64>(timeSeconds * 1000));
    });
}

void MainWindow::openAudioFile() {
    QString fileName = QFileDialog::getOpenFileName(this, "Open Audio File", "", "Audio Files (*.mp3 *.wav *.ogg *.flac)");
    if (!fileName.isEmpty()) {
        m_player->stop();
        m_player->setSource(QUrl::fromLocalFile(fileName));
        m_songTitleLabel->setText(QFileInfo(fileName).fileName());

        // Simulated waveform decode data for display
        QVector<float> samples(2000);
        for (int i = 0; i < samples.size(); ++i) {
            samples[i] = static_cast<float>(rand()) / RAND_MAX * (i % 2 == 0 ? 1 : -1);
        }

        QVector<ChordMarker> chords = { {0.0, "Em"}, {4.0, "G"}, {8.0, "D"}, {12.0, "C"} };
        m_waveformWidget->setAudioBuffer(samples, 16.0, 44100);
        m_waveformWidget->setChords(chords);
    }
}

void MainWindow::togglePlayPause() {
    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
        m_playBtn->setText("Play");
    } else {
        m_player->play();
        m_playBtn->setText("Pause");
    }
}

void MainWindow::stopAudio() {
    m_player->stop();
    m_playBtn->setText("Play");
    m_waveformWidget->setPlaybackPosition(0.0);
}

void MainWindow::updatePosition(qint64 position) {
    m_waveformWidget->setPlaybackPosition(position / 1000.0);
}

void MainWindow::fetchPageCounter() {
    QUrl url("https://ghitara.com/riff/riffstation_backend.php?action=counter");
    QNetworkRequest request(url);
    QNetworkReply *reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            if (doc.isObject()) {
                int count = doc.object().value("count").toInt();
                m_pageCounterLabel->setText(QString("Views: %1").arg(count));
            }
        } else {
            m_pageCounterLabel->setText("Views: --");
        }
        reply->deleteLater();
    });
}