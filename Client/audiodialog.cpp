#include "audiodialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QMediaDevices>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>

static QAudioFormat createAudioFormat(int quality)
{
    QAudioFormat format;
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    // Quality levels: 0 = low, 1 = medium, 2 = high
    switch (quality)
    {
    case 0: // Low: 8kHz
        format.setSampleRate(8000);
        break;
    case 1: // Medium: 16kHz
        format.setSampleRate(16000);
        break;
    case 2: // High: 48kHz
        format.setSampleRate(48000);
        break;
    default:
        format.setSampleRate(16000);
    }

    return format;
}

AudioDialog::AudioDialog(QTcpSocket *chatSocket, const QString &username, QWidget *parent)
    : QDialog(parent), chatSocket(chatSocket), username(username), streamSocket(nullptr)
{
    setWindowTitle("Audio Streaming - " + username);
    setGeometry(100, 100, 500, 600);
    setupUI();
}

AudioDialog::~AudioDialog()
{
    stopAudioCapture();
    if (streamSocket)
    {
        streamSocket->close();
    }
}

void AudioDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // ===== Status Section =====
    QGroupBox *statusGroup = new QGroupBox("Status", this);
    QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);

    lblStatus = new QLabel("Status: Idle", this);
    lblDevice = new QLabel("Device: Default", this);
    statusLayout->addWidget(lblStatus);
    statusLayout->addWidget(lblDevice);
    mainLayout->addWidget(statusGroup);

    // ===== Audio Quality Selection =====
    QGroupBox *qualityGroup = new QGroupBox("Audio Settings", this);
    QHBoxLayout *qualityLayout = new QHBoxLayout(qualityGroup);

    QLabel *qualityLabel = new QLabel("Quality:", this);
    cmbAudioQuality = new QComboBox(this);
    cmbAudioQuality->addItem("Low (8 kHz)", 0);
    cmbAudioQuality->addItem("Medium (16 kHz)", 1);
    cmbAudioQuality->addItem("High (48 kHz)", 2);
    cmbAudioQuality->setCurrentIndex(1);

    qualityLayout->addWidget(qualityLabel);
    qualityLayout->addWidget(cmbAudioQuality);
    qualityLayout->addStretch();
    mainLayout->addWidget(qualityGroup);

    // ===== Audio Level =====
    QLabel *levelLabel = new QLabel("Audio Level:", this);
    audioLevel = new QProgressBar(this);
    audioLevel->setRange(0, 100);
    audioLevel->setValue(0);
    mainLayout->addWidget(levelLabel);
    mainLayout->addWidget(audioLevel);

    // ===== Control Buttons =====
    QHBoxLayout *buttonLayout = new QHBoxLayout();

    btnStartAudio = new QPushButton("Start Audio", this);
    btnStopAudio = new QPushButton("Stop Audio", this);
    btnSelectAudioDevice = new QPushButton("Select Device", this);

    btnStopAudio->setEnabled(false);

    buttonLayout->addWidget(btnStartAudio);
    buttonLayout->addWidget(btnStopAudio);
    buttonLayout->addWidget(btnSelectAudioDevice);

    mainLayout->addLayout(buttonLayout);

    // ===== Audio Log =====
    QLabel *logLabel = new QLabel("Audio Log:", this);
    listAudioLog = new QListWidget(this);
    mainLayout->addWidget(logLabel);
    mainLayout->addWidget(listAudioLog);

    mainLayout->addStretch();
    setLayout(mainLayout);

    // ===== Connect Signals =====
    connect(btnStartAudio, &QPushButton::clicked, this, &AudioDialog::on_btnStartAudio_clicked);
    connect(btnStopAudio, &QPushButton::clicked, this, &AudioDialog::on_btnStopAudio_clicked);
    connect(btnSelectAudioDevice, &QPushButton::clicked, this, &AudioDialog::on_btnSelectAudioDevice_clicked);
}

void AudioDialog::on_btnStartAudio_clicked()
{
    if (currentTopic.isEmpty() || currentTopic == "None")
    {
        logAudio("Please select a topic first!");
        return;
    }

    if (isStreaming)
    {
        logAudio("Already streaming");
        return;
    }

    startAudioCapture();
}

void AudioDialog::on_btnStopAudio_clicked()
{
    stopAudioCapture();
}

void AudioDialog::on_btnSelectAudioDevice_clicked()
{
    QDialog deviceDialog(this);
    deviceDialog.setWindowTitle("Select Audio Device");
    deviceDialog.setGeometry(150, 150, 400, 300);

    QVBoxLayout *layout = new QVBoxLayout(&deviceDialog);

    QLabel *inputLabel = new QLabel("Input Devices:", &deviceDialog);
    QComboBox *inputDevices = new QComboBox(&deviceDialog);

    QLabel *outputLabel = new QLabel("Output Devices:", &deviceDialog);
    QComboBox *outputDevices = new QComboBox(&deviceDialog);

    // Populate device lists
    for (const auto &device : QMediaDevices::audioInputs())
    {
        inputDevices->addItem(device.description());
    }

    for (const auto &device : QMediaDevices::audioOutputs())
    {
        outputDevices->addItem(device.description());
    }

    layout->addWidget(inputLabel);
    layout->addWidget(inputDevices);
    layout->addWidget(outputLabel);
    layout->addWidget(outputDevices);

    QPushButton *okButton = new QPushButton("OK", &deviceDialog);
    connect(okButton, &QPushButton::clicked, &deviceDialog, &QDialog::accept);
    layout->addWidget(okButton);

    if (deviceDialog.exec() == QDialog::Accepted)
    {
        lblDevice->setText("Device: " + inputDevices->currentText());
        logAudio("Device selected: " + inputDevices->currentText());
    }
}

void AudioDialog::startAudioCapture()
{
    logAudio("[AUDIO] Starting audio capture...");

    // Create audio format based on selected quality
    int quality = cmbAudioQuality->currentData().toInt();
    QAudioFormat format = createAudioFormat(quality);

    // Get default input device
    QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();

    // Create audio source
    audioSource = new QAudioSource(inputDevice, format, this);

    // Start capturing audio
    audioInDevice = audioSource->start();

    if (audioInDevice)
    {
        connect(audioInDevice, &QIODevice::readyRead, this, &AudioDialog::onAudioInputReady);
        isStreaming = true;
        btnStartAudio->setEnabled(false);
        btnStopAudio->setEnabled(true);
        lblStatus->setText("Status: Recording...");
        logAudio("[AUDIO] Recording started - Quality: " + cmbAudioQuality->currentText());

        // Send STREAM_START packet
        streamSessionId = static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch());
        sendStreamPacket(MSG_STREAM_START, currentTopic, QByteArray());
    }
    else
    {
        logAudio("[ERROR] Failed to start audio source");
        isStreaming = false;
    }
}

void AudioDialog::stopAudioCapture()
{
    if (!isStreaming)
        return;

    logAudio("[AUDIO] Stopping audio capture...");

    if (audioSource)
    {
        audioSource->stop();
        audioInDevice = nullptr;
        delete audioSource;
        audioSource = nullptr;
    }

    if (audioSink)
    {
        audioSink->stop();
        audioOutDevice = nullptr;
        delete audioSink;
        audioSink = nullptr;
        logAudio("[PLAYBACK] Audio output stopped");
    }

    // Send STREAM_STOP packet
    sendStreamPacket(MSG_STREAM_STOP, currentTopic, QByteArray());

    isStreaming = false;
    btnStartAudio->setEnabled(true);
    btnStopAudio->setEnabled(false);
    lblStatus->setText("Status: Idle");
    audioLevel->setValue(0);
    logAudio("[AUDIO] Recording stopped");
}

void AudioDialog::onAudioInputReady()
{
    if (!audioInDevice || !isStreaming)
        return;

    frameCounter++;

    // Read audio data from device
    QByteArray audioData = audioInDevice->readAll();

    if (!audioData.isEmpty())
    {
        // Send audio frame every 25 frames (approx once per 0.5s at typical frame rate)
        if (frameCounter % 5 == 0)
        {
            sendStreamPacket(MSG_STREAM_FRAME, currentTopic, audioData);

            // Update audio level indicator
            if (audioData.size() > 0)
            {
                // Simple level calculation (average absolute value)
                const int16_t *samples = reinterpret_cast<const int16_t *>(audioData.constData());
                int sampleCount = audioData.size() / sizeof(int16_t);
                int64_t sum = 0;

                for (int i = 0; i < sampleCount; i++)
                {
                    sum += std::abs(samples[i]);
                }

                int level = static_cast<int>(100 * (sum / sampleCount) / 32768);
                audioLevel->setValue(qMin(level, 100));
            }

            if (frameCounter % 50 == 0)
            {
                logAudio("[AUDIO] Frame " + QString::number(frameCounter) + " sent (" +
                         QString::number(audioData.size()) + " bytes)");
            }
        }
    }
}

void AudioDialog::onStreamReadyRead()
{
    if (!streamSocket)
        return;

    while (streamSocket->bytesAvailable() >= (qint64)sizeof(PacketHeader))
    {
        PacketHeader header;
        streamSocket->peek((char *)&header, sizeof(PacketHeader));

        if (streamSocket->bytesAvailable() < (qint64)sizeof(PacketHeader) + header.payloadLength)
            break;

        streamSocket->read((char *)&header, sizeof(PacketHeader));

        if (header.payloadLength > 0)
        {
            QByteArray payload = streamSocket->read(header.payloadLength);

            if (header.msgType == MSG_STREAM_FRAME)
            {
                // Play received audio
                if (!audioSink && payload.size() > 0)
                {
                    // Initialize audio output on first frame
                    int quality = cmbAudioQuality->currentData().toInt();
                    QAudioFormat format = createAudioFormat(quality);
                    QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();
                    audioSink = new QAudioSink(outputDevice, format, this);
                    audioOutDevice = audioSink->start();
                    logAudio("[PLAYBACK] Audio output started");
                }

                if (audioOutDevice && payload.size() > 0)
                {
                    audioOutDevice->write(payload);
                }

                static int frameCount = 0;
                if (++frameCount % 50 == 0)
                {
                    logAudio("[AUDIO] Playing frame (" + QString::number(payload.size()) + " bytes)");
                }
            }
        }
    }
}

void AudioDialog::onStreamConnected()
{
    logAudio("[STREAM] Connected to streaming server");
    lblStatus->setText("Status: Stream Connected");
}

void AudioDialog::onStreamDisconnected()
{
    logAudio("[STREAM] Disconnected from streaming server");
    if (isStreaming)
    {
        stopAudioCapture();
    }
    lblStatus->setText("Status: Stream Disconnected");
}

void AudioDialog::sendStreamPacket(MessageType type, const QString &topic,
                                   const QByteArray &payload, uint8_t flags)
{
    if (!streamSocket || streamSocket->state() != QAbstractSocket::ConnectedState)
    {
        logAudio("[ERROR] Not connected to stream server");
        return;
    }

    PacketHeader header;
    std::memset(&header, 0, sizeof(PacketHeader));

    header.msgType = type;
    header.messageId = streamSessionId;
    header.payloadLength = payload.size();
    header.timestamp = QDateTime::currentMSecsSinceEpoch();
    header.flags = flags;

    std::strncpy(header.sender, username.toUtf8().constData(), MAX_USERNAME_LEN - 1);
    std::strncpy(header.topic, topic.toUtf8().constData(), MAX_TOPIC_LEN - 1);

    streamSocket->write((char *)&header, sizeof(PacketHeader));
    if (!payload.isEmpty())
    {
        streamSocket->write(payload);
    }
    streamSocket->flush();
}

void AudioDialog::logAudio(const QString &msg)
{
    QString time = QDateTime::currentDateTime().toString("HH:mm:ss");
    listAudioLog->addItem("[" + time + "] " + msg);
    listAudioLog->scrollToBottom();
}
