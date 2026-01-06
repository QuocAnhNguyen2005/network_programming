#include "audioplayerdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QMediaDevices>
#include <QDebug>

AudioPlayerDialog::AudioPlayerDialog(const QByteArray &data, int quality, QWidget *parent)
    : QDialog(parent), audioData(data), audioOutput(nullptr), audioBuffer(nullptr),
      progressTimer(nullptr), isPlaying(false)
{
    setWindowTitle("Audio Player");
    resize(400, 250);

    qDebug() << "[AudioPlayer] Constructor called with" << data.size() << "bytes, quality:" << quality;

    setupUI();
    setupAudioFormat(quality);

    // Create audio buffer with the data
    audioBuffer = new QBuffer(&audioData, this);
    audioBuffer->open(QIODevice::ReadOnly);
    audioBuffer->seek(0);

    qDebug() << "[AudioPlayer] Buffer created, position:" << audioBuffer->pos();

    // Create audio output
    QAudioDevice deviceInfo = QMediaDevices::defaultAudioOutput();
    if (!deviceInfo.isFormatSupported(audioFormat))
    {
        qDebug() << "[AudioPlayer] Format NOT supported by device";
        QMessageBox::warning(this, "Format Error",
                             "Audio format not supported by default device. Trying anyway...");
    }
    else
    {
        qDebug() << "[AudioPlayer] Format SUPPORTED by device";
    }

    audioOutput = new QAudioSink(deviceInfo, audioFormat, this);
    connect(audioOutput, &QAudioSink::stateChanged, this, &AudioPlayerDialog::onAudioStateChanged);

    // Setup progress timer
    progressTimer = new QTimer(this);
    connect(progressTimer, &QTimer::timeout, this, &AudioPlayerDialog::updateProgressBar);

    lblStatus->setText("Ready to play");
    lblDuration->setText(QString("Audio size: %1 bytes (Quality: %2)").arg(audioData.size()).arg(quality));
    progressBar->setMaximum(audioData.size());
    progressBar->setValue(0);

    qDebug() << "[AudioPlayer] Initialization complete";
}

AudioPlayerDialog::~AudioPlayerDialog()
{
    stopPlayback();
    if (audioBuffer)
    {
        audioBuffer->close();
        delete audioBuffer;
    }
    if (audioOutput)
    {
        delete audioOutput;
    }
}

void AudioPlayerDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Status labels
    lblStatus = new QLabel("Initializing...", this);
    lblDuration = new QLabel("Duration: 0s", this);
    mainLayout->addWidget(lblStatus);
    mainLayout->addWidget(lblDuration);

    // Progress bar
    progressBar = new QProgressBar(this);
    mainLayout->addWidget(progressBar);

    // Control buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    btnPlay = new QPushButton("Play", this);
    btnPause = new QPushButton("Pause", this);
    btnStop = new QPushButton("Stop", this);

    btnPause->setEnabled(false);
    btnStop->setEnabled(false);

    connect(btnPlay, &QPushButton::clicked, this, &AudioPlayerDialog::on_btnPlay_clicked);
    connect(btnPause, &QPushButton::clicked, this, &AudioPlayerDialog::on_btnPause_clicked);
    connect(btnStop, &QPushButton::clicked, this, &AudioPlayerDialog::on_btnStop_clicked);

    buttonLayout->addWidget(btnPlay);
    buttonLayout->addWidget(btnPause);
    buttonLayout->addWidget(btnStop);
    mainLayout->addLayout(buttonLayout);

    // Volume control
    QGroupBox *volumeGroup = new QGroupBox("Volume", this);
    QHBoxLayout *volumeLayout = new QHBoxLayout(volumeGroup);
    volumeSlider = new QSlider(Qt::Horizontal, this);
    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(70);
    connect(volumeSlider, &QSlider::valueChanged, this, [this](int value)
            {
        if (audioOutput)
            audioOutput->setVolume(value / 100.0); });
    volumeLayout->addWidget(volumeSlider);
    mainLayout->addWidget(volumeGroup);

    setLayout(mainLayout);
}

void AudioPlayerDialog::setupAudioFormat(int quality)
{
    // Setup audio format to match the recording format
    // Quality: 0 = 8kHz, 1 = 16kHz, 2 = 48kHz
    audioFormat.setChannelCount(1);
    audioFormat.setSampleFormat(QAudioFormat::Int16);

    switch (quality)
    {
    case 0: // Low: 8kHz
        audioFormat.setSampleRate(8000);
        break;
    case 2: // High: 48kHz
        audioFormat.setSampleRate(48000);
        break;
    case 1: // Medium: 16kHz
    default:
        audioFormat.setSampleRate(16000);
        break;
    }
}

void AudioPlayerDialog::on_btnPlay_clicked()
{
    startPlayback();
}

void AudioPlayerDialog::on_btnPause_clicked()
{
    if (audioOutput && audioOutput->state() == QAudio::ActiveState)
    {
        audioOutput->suspend();
        lblStatus->setText("Paused");
        btnPlay->setEnabled(true);
        btnPause->setEnabled(false);
    }
}

void AudioPlayerDialog::on_btnStop_clicked()
{
    stopPlayback();
}

void AudioPlayerDialog::startPlayback()
{
    if (!audioOutput || !audioBuffer)
    {
        qDebug() << "[AudioPlayer] ERROR: audioOutput or audioBuffer is null";
        return;
    }

    if (isPlaying)
    {
        qDebug() << "[AudioPlayer] Already playing, ignoring start request";
        return;
    }

    qDebug() << "[AudioPlayer] Starting playback...";
    qDebug() << "[AudioPlayer] Buffer size:" << audioBuffer->size();

    // Reset buffer to beginning
    audioBuffer->close();
    audioBuffer->open(QIODevice::ReadOnly);
    audioBuffer->seek(0);
    qDebug() << "[AudioPlayer] Buffer position after seek:" << audioBuffer->pos();

    // Set volume
    double volume = volumeSlider->value() / 100.0;
    audioOutput->setVolume(volume);
    qDebug() << "[AudioPlayer] Volume set to:" << volume;

    // Start playback
    qDebug() << "[AudioPlayer] Calling audioOutput->start()...";
    audioOutput->start(audioBuffer);
    isPlaying = true;
    // Start progress timer
    progressTimer->start(100); // Update every 100ms

    qDebug() << "[AudioPlayer] Playback initialization complete";
}

void AudioPlayerDialog::stopPlayback()
{
    qDebug() << "[AudioPlayer] stopPlayback() called";
    isPlaying = false;

    if (progressTimer && progressTimer->isActive())
    {
        progressTimer->stop();
    }

    if (audioOutput)
    {
        qDebug() << "[AudioPlayer] Stopping audio output, state:" << audioOutput->state();
        audioOutput->stop();
        audioOutput->reset();
    }

    if (audioBuffer)
    {
        audioBuffer->close();
        audioBuffer->seek(0);
        audioBuffer->open(QIODevice::ReadOnly);
    }

    lblStatus->setText("Stopped");
    progressBar->setValue(0);
    btnPlay->setEnabled(true);
    btnPause->setEnabled(false);
    btnStop->setEnabled(false);

    qDebug() << "[AudioPlayer] stopPlayback() complete";
}

void AudioPlayerDialog::onAudioStateChanged(QAudio::State state)
{
    switch (state)
    {
    case QAudio::IdleState:
        qDebug() << "[AudioPlayer] State: IDLE (playback finished or underrun)";
        // Only stop if we were actually playing (prevent restart)
        if (isPlaying)
        {
            stopPlayback();
            lblStatus->setText("Playback finished");
        }
        break;
    case QAudio::StoppedState:
        qDebug() << "[AudioPlayer] State: STOPPED";
        isPlaying = false;
        if (audioOutput && audioOutput->error() != QAudio::NoError)
        {
            qDebug() << "[AudioPlayer] ERROR:" << audioOutput->error();
            lblStatus->setText(QString("Error: %1").arg(audioOutput->error()));
            QMessageBox::warning(this, "Playback Error",
                                 QString("Audio playback error occurred: %1").arg(audioOutput->error()));
        }
        break;
    case QAudio::ActiveState:
        qDebug() << "[AudioPlayer] State: ACTIVE (playing)";
        lblStatus->setText("Playing...");
        break;
    case QAudio::SuspendedState:
        qDebug() << "[AudioPlayer] State: SUSPENDED";
        lblStatus->setText("Paused");
        break;
    }
}

void AudioPlayerDialog::updateProgressBar()
{
    if (audioBuffer && audioOutput)
    {
        qint64 currentPos = audioBuffer->pos();
        progressBar->setValue(currentPos);

        // Calculate and display duration
        int sampleRate = audioFormat.sampleRate();
        int bytesPerSample = audioFormat.bytesPerSample();
        int channels = audioFormat.channelCount();
        int bytesPerSecond = sampleRate * bytesPerSample * channels;

        if (bytesPerSecond > 0)
        {
            double currentSeconds = (double)currentPos / bytesPerSecond;
            double totalSeconds = (double)audioData.size() / bytesPerSecond;
            lblDuration->setText(QString("Time: %1s / %2s")
                                     .arg(currentSeconds, 0, 'f', 1)
                                     .arg(totalSeconds, 0, 'f', 1));
        }
    }
}
