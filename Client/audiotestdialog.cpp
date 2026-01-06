#include "audiotestdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QMediaDevices>
#include <QDateTime>
#include <QTimer>

AudioTestDialog::AudioTestDialog(QWidget *parent)
    : QDialog(parent), audioInput(nullptr), audioOutput(nullptr),
      recordBuffer(nullptr), playbackBuffer(nullptr)
{
    setWindowTitle("Audio Test Dialog");
    setGeometry(100, 100, 600, 700);
    setupUI();
    setupAudioFormat();
    listAudioDevices();
}

AudioTestDialog::~AudioTestDialog()
{
    if (audioInput)
    {
        audioInput->stop();
        delete audioInput;
    }
    if (audioOutput)
    {
        audioOutput->stop();
        delete audioOutput;
    }
    if (recordBuffer)
        delete recordBuffer;
    if (playbackBuffer)
        delete playbackBuffer;
}

void AudioTestDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // ===== Input Device Selection =====
    QGroupBox *inputGroup = new QGroupBox("Input Device (Microphone)", this);
    QVBoxLayout *inputLayout = new QVBoxLayout(inputGroup);

    cmbInputDevice = new QComboBox(this);
    inputLayout->addWidget(cmbInputDevice);

    lblMicStatus = new QLabel("Status: Ready", this);
    inputLayout->addWidget(lblMicStatus);

    QHBoxLayout *micButtonLayout = new QHBoxLayout();
    btnTestMic = new QPushButton("Start Recording (5s)", this);
    btnStopMic = new QPushButton("Stop Recording", this);
    btnStopMic->setEnabled(false);

    micButtonLayout->addWidget(btnTestMic);
    micButtonLayout->addWidget(btnStopMic);
    inputLayout->addLayout(micButtonLayout);

    mainLayout->addWidget(inputGroup);

    // ===== Output Device Selection =====
    QGroupBox *outputGroup = new QGroupBox("Output Device (Loudspeaker)", this);
    QVBoxLayout *outputLayout = new QVBoxLayout(outputGroup);

    cmbOutputDevice = new QComboBox(this);
    outputLayout->addWidget(cmbOutputDevice);

    lblLoudspeakerStatus = new QLabel("Status: Ready", this);
    outputLayout->addWidget(lblLoudspeakerStatus);

    btnTestLoudspeaker = new QPushButton("Playback Recorded Audio", this);
    outputLayout->addWidget(btnTestLoudspeaker);

    mainLayout->addWidget(outputGroup);

    // ===== Device List =====
    btnListDevices = new QPushButton("Refresh Device List", this);
    mainLayout->addWidget(btnListDevices);

    // ===== Log =====
    QGroupBox *logGroup = new QGroupBox("Test Log", this);
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);

    logList = new QListWidget(this);
    logLayout->addWidget(logList);

    mainLayout->addWidget(logGroup);

    setLayout(mainLayout);

    // ===== Connections =====
    connect(btnTestMic, &QPushButton::clicked, this, &AudioTestDialog::on_btnTestMic_clicked);
    connect(btnStopMic, &QPushButton::clicked, this, &AudioTestDialog::on_btnStopMic_clicked);
    connect(btnTestLoudspeaker, &QPushButton::clicked, this, &AudioTestDialog::on_btnTestLoudspeaker_clicked);
    connect(btnListDevices, &QPushButton::clicked, this, &AudioTestDialog::on_btnListDevices_clicked);
}

void AudioTestDialog::setupAudioFormat()
{
    audioFormat.setSampleRate(16000);
    audioFormat.setChannelCount(1);
    audioFormat.setSampleFormat(QAudioFormat::Int16);

    logTest("[FORMAT] Sample Rate: 16000 Hz");
    logTest("[FORMAT] Channels: 1 (Mono)");
    logTest("[FORMAT] Sample Format: 16-bit Int");
}

void AudioTestDialog::listAudioDevices()
{
    logTest("===== SCANNING DEVICES =====");

    // List input devices
    auto inputs = QMediaDevices::audioInputs();
    logTest("[INPUT] Found " + QString::number(inputs.size()) + " microphone(s):");
    cmbInputDevice->clear();
    for (const auto &device : inputs)
    {
        logTest("  - " + device.description());
        cmbInputDevice->addItem(device.description(), QVariant::fromValue(device));
    }

    if (inputs.isEmpty())
    {
        logTest("  WARNING: No microphones found!");
    }

    // List output devices
    auto outputs = QMediaDevices::audioOutputs();
    logTest("[OUTPUT] Found " + QString::number(outputs.size()) + " loudspeaker(s):");
    cmbOutputDevice->clear();
    for (const auto &device : outputs)
    {
        logTest("  - " + device.description());
        cmbOutputDevice->addItem(device.description(), QVariant::fromValue(device));
    }

    if (outputs.isEmpty())
    {
        logTest("  WARNING: No loudspeakers found!");
    }

    logTest("===== END SCAN =====");
}

void AudioTestDialog::on_btnListDevices_clicked()
{
    logTest("\n");
    listAudioDevices();
}

void AudioTestDialog::on_btnTestMic_clicked()
{
    if (audioInput)
        return;

    logTest("[MIC] Starting microphone test...");
    lblMicStatus->setText("Status: Recording...");

    QAudioDevice inputDevice = cmbInputDevice->currentData().value<QAudioDevice>();

    // Check if format is supported
    if (!inputDevice.isFormatSupported(audioFormat))
    {
        logTest("[MIC] WARNING: Format not officially supported, trying anyway...");
    }

    audioInput = new QAudioSource(inputDevice, audioFormat, this);
    connect(audioInput, &QAudioSource::stateChanged, this, &AudioTestDialog::onInputStateChanged);

    recordedData.clear();
    recordBuffer = new QBuffer(&recordedData, this);
    recordBuffer->open(QIODevice::WriteOnly);

    audioInput->start(recordBuffer);
    logTest("[MIC] Recording started from: " + inputDevice.description());

    // Auto stop after 5 seconds
    QTimer::singleShot(5000, this, &AudioTestDialog::on_btnStopMic_clicked);

    btnTestMic->setEnabled(false);
    btnStopMic->setEnabled(true);
}

void AudioTestDialog::on_btnStopMic_clicked()
{
    if (!audioInput)
        return;

    audioInput->stop();
    audioInput = nullptr;

    if (recordBuffer)
    {
        recordBuffer->close();
        recordBuffer->deleteLater();
        recordBuffer = nullptr;
    }

    logTest("[MIC] Recording stopped. Data size: " + QString::number(recordedData.size()) + " bytes");

    if (recordedData.isEmpty())
    {
        lblMicStatus->setText("Status: No data recorded!");
        logTest("[MIC] ERROR: No audio data captured! Check if microphone is working.");
    }
    else
    {
        lblMicStatus->setText("Status: Recorded " + QString::number(recordedData.size()) + " bytes");
        logTest("[MIC] SUCCESS: Audio data ready for playback");
    }

    btnTestMic->setEnabled(true);
    btnStopMic->setEnabled(false);
}

void AudioTestDialog::on_btnTestLoudspeaker_clicked()
{
    if (recordedData.isEmpty())
    {
        QMessageBox::warning(this, "Error", "No audio data! Record microphone first.");
        return;
    }

    if (audioOutput)
        return;

    logTest("[SPEAKER] Starting loudspeaker test...");
    lblLoudspeakerStatus->setText("Status: Playing...");

    QAudioDevice outputDevice = cmbOutputDevice->currentData().value<QAudioDevice>();

    // Check if format is supported
    if (!outputDevice.isFormatSupported(audioFormat))
    {
        logTest("[SPEAKER] WARNING: Format not officially supported, trying anyway...");
    }

    audioOutput = new QAudioSink(outputDevice, audioFormat, this);
    connect(audioOutput, &QAudioSink::stateChanged, this, &AudioTestDialog::onOutputStateChanged);

    playbackBuffer = new QBuffer(&recordedData, this);
    playbackBuffer->open(QIODevice::ReadOnly);
    playbackBuffer->seek(0);

    audioOutput->start(playbackBuffer);
    logTest("[SPEAKER] Playing audio from: " + outputDevice.description());
    logTest("[SPEAKER] Audio size: " + QString::number(recordedData.size()) + " bytes");
    logTest("[SPEAKER] Duration: " + QString::number(recordedData.size() / 32, 'f', 1) + " seconds");
}

void AudioTestDialog::onInputStateChanged(QAudio::State state)
{
    QString stateStr;
    switch (state)
    {
    case QAudio::IdleState:
        stateStr = "Idle";
        break;
    case QAudio::ActiveState:
        stateStr = "Active (Recording)";
        break;
    case QAudio::SuspendedState:
        stateStr = "Suspended";
        break;
    case QAudio::StoppedState:
        stateStr = "Stopped";
        if (audioInput)
        {
            if (audioInput->error() != QAudio::NoError)
            {
                logTest("[MIC] ERROR: " + QString::number(audioInput->error()));
                lblMicStatus->setText("Status: Error " + QString::number(audioInput->error()));
            }
        }
        break;
    }
    logTest("[MIC] State: " + stateStr);
}

void AudioTestDialog::onOutputStateChanged(QAudio::State state)
{
    QString stateStr;
    switch (state)
    {
    case QAudio::IdleState:
        stateStr = "Idle (Playback finished)";
        lblLoudspeakerStatus->setText("Status: Playback finished");
        audioOutput = nullptr;
        break;
    case QAudio::ActiveState:
        stateStr = "Active (Playing)";
        break;
    case QAudio::SuspendedState:
        stateStr = "Suspended";
        break;
    case QAudio::StoppedState:
        stateStr = "Stopped";
        if (audioOutput)
        {
            if (audioOutput->error() != QAudio::NoError)
            {
                logTest("[SPEAKER] ERROR: " + QString::number(audioOutput->error()));
                lblLoudspeakerStatus->setText("Status: Error " + QString::number(audioOutput->error()));
            }
        }
        break;
    }
    logTest("[SPEAKER] State: " + stateStr);
}

void AudioTestDialog::onMicDataReady()
{
    if (recordBuffer)
    {
        recordedData = recordBuffer->data();
        logTest("[MIC] Data ready: " + QString::number(recordedData.size()) + " bytes");
    }
}

void AudioTestDialog::logTest(const QString &msg)
{
    QString time = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    logList->addItem("[" + time + "] " + msg);
    logList->scrollToBottom();
}
