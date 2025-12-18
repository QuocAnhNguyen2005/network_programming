#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QAudioFormat>
#include <QMediaDevices>
#include <QAudioDevice>

static QAudioFormat createAudioFormat()
{
    QAudioFormat format;
    format.setSampleRate(8000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    return format;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), socket(new QTcpSocket(this))
{
    ui->setupUi(this);

    ui->txtHost->setText("127.0.0.1");
    ui->txtPort->setText(QString::number(DEFAULT_PORT)); //

    connect(socket, &QTcpSocket::connected, this, &MainWindow::onSocketConnected);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onSocketReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::onSocketDisconnected);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &MainWindow::onSocketError);

    connect(ui->txtTopicPub, &QComboBox::currentTextChanged, this,
            &MainWindow::onTopicChanged);
    ui->btnDisconnect->setEnabled(false);
    ui->groupBoxChat->setEnabled(false);
    ui->groupTopic->setEnabled(false);

    streamSocket = new QTcpSocket(this);

    connect(streamSocket, &QTcpSocket::connected,
            this, &MainWindow::onStreamConnected);
    connect(streamSocket, &QTcpSocket::readyRead,
            this, &MainWindow::onStreamReadyRead);
    connect(streamSocket, &QTcpSocket::disconnected,
            this, &MainWindow::onStreamDisconnected);
}

MainWindow::~MainWindow()
{
    if (socket->isOpen())
    {
        socket->close();
    }
    delete ui;
}

void MainWindow::sendPacket(MessageType type, const QString &topic, const QByteArray &payload)
{
    if (socket->state() != QAbstractSocket::ConnectedState)
        return;

    PacketHeader header;
    memset(&header, 0, sizeof(PacketHeader));

    header.msgType = type;
    header.payloadLength = (uint32_t)payload.size();
    header.timestamp = QDateTime::currentMSecsSinceEpoch();

    QByteArray userBytes = currentUsername.toUtf8();
    strncpy(header.sender, userBytes.constData(), MAX_USERNAME_LEN - 1);

    QByteArray topicBytes = topic.toUtf8();
    strncpy(header.topic, topicBytes.constData(), MAX_TOPIC_LEN - 1);

    socket->write((const char *)&header, sizeof(PacketHeader));

    if (header.payloadLength > 0)
    {
        socket->write(payload);
    }

    socket->flush();
}

void MainWindow::on_btnConnect_clicked()
{
    QString host = ui->txtHost->text();
    int port = ui->txtPort->text().toInt();
    currentUsername = ui->txtUsername->text();

    if (currentUsername.isEmpty())
    {
        QMessageBox::warning(this, "Lỗi", "Vui lòng nhập Username!");
        return;
    }

    logMessage("Đang kết nối đến " + host + ":" + QString::number(port) + "...");
    socket->connectToHost(host, port);
}

void MainWindow::onSocketConnected()
{
    logMessage("Đã kết nối TCP thành công!");

    sendPacket(MSG_LOGIN, "", "");

    ui->btnConnect->setEnabled(false);
    ui->txtHost->setEnabled(false);
    ui->txtPort->setEnabled(false);
    ui->txtUsername->setEnabled(false);

    ui->btnDisconnect->setEnabled(true);
    ui->groupBoxChat->setEnabled(true);
    ui->groupTopic->setEnabled(true);
    ui->groupLog->setEnabled(true);
}

void MainWindow::on_btnDisconnect_clicked()
{
    sendPacket(MSG_LOGOUT, "", "");
    socket->disconnectFromHost();
}

void MainWindow::onSocketDisconnected()
{
    logMessage("Đã ngắt kết nối khỏi server.");
    ui->btnConnect->setEnabled(true);
    ui->txtHost->setEnabled(true);
    ui->txtPort->setEnabled(true);
    ui->txtUsername->setEnabled(true);

    ui->btnDisconnect->setEnabled(false);
    ui->groupBoxChat->setEnabled(false);
    ui->groupTopic->setEnabled(false);
    ui->groupLog->setEnabled(false);
}

void MainWindow::onSocketError(QAbstractSocket::SocketError socketError)
{
    logMessage("Lỗi Socket: " + socket->errorString());
}

void MainWindow::onSocketReadyRead()
{
    while (socket->bytesAvailable() > 0)
    {

        if (socket->bytesAvailable() < (qint64)sizeof(PacketHeader))
        {
            return;
        }

        PacketHeader header;
        socket->peek((char *)&header, sizeof(PacketHeader));

        if (socket->bytesAvailable() < (qint64)sizeof(PacketHeader) + header.payloadLength)
        {
            return;
        }

        socket->read((char *)&header, sizeof(PacketHeader));
        QByteArray payload;
        if (header.payloadLength > 0)
        {
            payload = socket->read(header.payloadLength);
        }

        QString sender(header.sender);
        QString topic(header.topic);

        switch (header.msgType)
        {
        case MSG_ACK:
            logMessage(QString("[SERVER] ACK: Yêu cầu đã được xử lý (Topic: %1)").arg(topic));
            break;
        case MSG_PUBLISH_TEXT:
        {
            QString msg = QString("[%1] %2: %3")
                              .arg(topic)
                              .arg(sender)
                              .arg(QString::fromUtf8(payload));

            topicMessages[topic].append(msg);

            if (ui->txtTopicPub->currentText() == topic)
            {
                ui->listMessage->addItem(msg);
                ui->listMessage->scrollToBottom();
            }
            break;
        }
        case MSG_PUBLISH_FILE:
        {
            QString msg = QString("[%1] %2 đã gửi một file (%3 bytes)")
                              .arg(topic)
                              .arg(sender)
                              .arg(header.payloadLength);

            topicMessages[topic].append(msg);

            if (ui->txtTopicPub->currentText() == topic)
            {
                ui->listMessage->addItem(msg);
                ui->listMessage->scrollToBottom();
            }
            break;
        }
        case MSG_ERROR:
            logMessage("[SERVER] Lỗi: Yêu cầu không hợp lệ!");
            break;
        default:
            logMessage(QString("Nhận loại tin nhắn lạ: %1").arg(header.msgType));
        }
    }
}

void MainWindow::on_btnSubscribe_clicked()
{
    QString topic = ui->txtTopicSub->text();
    if (topic.isEmpty())
    {
        QMessageBox::warning(this, "Lỗi", "Vui lòng nhập Topic!");
        return;
    }

    sendPacket(MSG_SUBSCRIBE, topic, "");
    logMessage("Đã gửi yêu cầu Subscribe: " + topic);
    ui->txtTopicPub->addItem(topic);
}

void MainWindow::on_btnUnsubscribe_clicked()
{
    QString topic = ui->txtTopicSub->text();
    if (topic.isEmpty())
    {
        QMessageBox::warning(this, "Lỗi", "Vui lòng nhập Topic!");
        return;
    }

    sendPacket(MSG_UNSUBSCRIBE, topic, "");
    logMessage("Đã gửi yêu cầu Unsubscribe: " + topic);
    int index = ui->txtTopicPub->findText(topic);
    if (index != -1)
    {
        ui->txtTopicPub->removeItem(index);
    }
}

void MainWindow::on_btnSend_clicked()
{
    QString topic = ui->txtTopicPub->currentText();
    QString msg = ui->txtMessage->text();

    if (topic == "None" || topic.isEmpty() || msg.isEmpty())
    {
        QMessageBox::warning(this, "Lỗi", "Vui lòng nhập Topic và Message!");
        return;
    }

    sendPacket(MSG_PUBLISH_TEXT, topic, msg.toUtf8());

    ui->txtMessage->clear();
}

void MainWindow::onTopicChanged(const QString &topic)
{
    ui->listMessage->clear();
    ui->listMessage->addItem("Chat messages for topic: " + topic);

    if (!topicMessages.contains(topic))
        return;

    ui->listMessage->addItems(topicMessages[topic]);
    ui->listMessage->scrollToBottom();
}

void MainWindow::on_btnBrowseFile_clicked()
{
    QString topic = ui->txtTopicPub->currentText();
    if (topic == "None" || topic.isEmpty())
    {
        QMessageBox::warning(this, "Cảnh báo", "Vui lòng nhập Topic trước khi gửi file.");
        return;
    }

    QString filePath = QFileDialog::getOpenFileName(this, "Chọn file để gửi");
    if (filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        QMessageBox::critical(this, "Lỗi", "Không thể đọc file!");
        return;
    }

    QByteArray fileData = file.readAll();
    file.close();

    if (fileData.size() > MAX_BUFFER_SIZE)
    {
        QMessageBox::warning(this, "Lỗi", "File quá lớn so với buffer server!");
        return;
    }

    sendPacket(MSG_PUBLISH_FILE, topic, fileData);
    logMessage("Đang gửi file: " + QFileInfo(filePath).fileName());
}

void MainWindow::logMessage(const QString &msg)
{
    QString time = QDateTime::currentDateTime().toString("HH:mm:ss");
    ui->listWidgetLog->addItem("[" + time + "] " + msg);
    ui->listWidgetLog->scrollToBottom();
}

void MainWindow::sendStreamPacket(MessageType type,
                                  const QString &topic,
                                  const QByteArray &payload,
                                  uint32_t sessionId,
                                  uint8_t flags)
{
    if (streamSocket->state() != QAbstractSocket::ConnectedState)
        return;

    PacketHeader header;
    memset(&header, 0, sizeof(PacketHeader));

    header.msgType = type;
    header.messageId = sessionId;
    header.payloadLength = payload.size();
    header.timestamp = QDateTime::currentMSecsSinceEpoch();
    header.flags = flags;

    strncpy(header.sender,
            currentUsername.toUtf8().constData(),
            MAX_USERNAME_LEN - 1);

    strncpy(header.topic,
            topic.toUtf8().constData(),
            MAX_TOPIC_LEN - 1);

    streamSocket->write((char *)&header, sizeof(PacketHeader));
    if (!payload.isEmpty())
        streamSocket->write(payload);

    streamSocket->flush();
}

void MainWindow::onStreamReadyRead()
{
    static int frameCounter = 0;
    frameCounter++;

    if (frameCounter % 25 == 0)
    {
        ui->listAudio->addItem("[AUDIO] Receiving audio...");
    }

    while (streamSocket->bytesAvailable() >= (qint64)sizeof(PacketHeader))
    {
        PacketHeader header;
        streamSocket->peek((char *)&header, sizeof(PacketHeader));

        if (streamSocket->bytesAvailable() <
            sizeof(PacketHeader) + header.payloadLength)
            return;

        streamSocket->read((char *)&header, sizeof(PacketHeader));
        if (QString(header.sender) == currentUsername)
        {
            // Bỏ frame của chính mình
            continue;
        }

        QByteArray payload;
        if (header.payloadLength > 0)
            payload = streamSocket->read(header.payloadLength);

        if (header.msgType == MSG_STREAM_FRAME)
        {
            // ===== Setup AUDIO OUTPUT nếu chưa có =====
            if (!audioOutputReady)
            {
                if (!audioSink)
                {
                    QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();
                    QAudioFormat format = createAudioFormat();

                    audioSink = new QAudioSink(outputDevice, format, this);
                    audioOutDevice = audioSink->start();
                }
                audioOutputReady = true;
            }

            audioOutDevice->write(payload);
        }
        else if (header.msgType == MSG_STREAM_STOP)
        {
            if (audioSink)
            {
                audioSink->stop();
                delete audioSink;
                audioSink = nullptr;
            }
        }
    }
}

void MainWindow::startAudioStream(const QString &topic)
{
    if (isStreaming)
        return;

    streamSessionId =
        static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch());

    // Gửi STREAM_START
    sendStreamPacket(MSG_STREAM_START,
                     topic,
                     QByteArray(),
                     streamSessionId,
                     0);

    // ===== Setup AUDIO INPUT =====
    QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();
    QAudioFormat format = createAudioFormat();

    audioSource = new QAudioSource(inputDevice, format, this);
    audioInDevice = audioSource->start();

    connect(audioInDevice, &QIODevice::readyRead, this, [this]()
            {
        QByteArray data = audioInDevice->read(320);
        if (!data.isEmpty())
            sendAudioFrame(data); });

    isStreaming = true;

    ui->listAudio->addItem("[AUDIO] Streaming started");
}

void MainWindow::sendAudioFrame(const QByteArray &data)
{
    if (!streamSocket ||
        streamSocket->state() != QAbstractSocket::ConnectedState)
    {
        return;
    }

    sendStreamPacket(MSG_STREAM_FRAME,
                     ui->txtTopicPub->currentText(),
                     data,
                     streamSessionId,
                     0);
}

void MainWindow::stopAudioStream()
{
    if (!isStreaming)
        return;

    sendStreamPacket(MSG_STREAM_STOP,
                     ui->txtTopicPub->currentText(),
                     QByteArray(),
                     streamSessionId,
                     0);

    if (audioSource)
    {
        audioSource->stop();
        delete audioSource;
        audioSource = nullptr;
    }

    if (audioSink)
    {
        audioSink->stop();
        delete audioSink;
        audioSink = nullptr;
    }

    isStreaming = false;
    ui->listAudio->addItem("[AUDIO] Streaming stopped");
    audioOutputReady = false;
}

void MainWindow::onStreamDisconnected()
{
    if (audioSource)
    {
        audioSource->stop();
        delete audioSource;
        audioSource = nullptr;
    }

    if (audioSink)
    {
        audioSink->stop();
        delete audioSink;
        audioSink = nullptr;
    }

    audioOutputReady = false;
    isStreaming = false;

    ui->listAudio->addItem("[AUDIO] Stream disconnected");
}

void MainWindow::onStreamConnected()
{
    logMessage("Đã kết nối Stream thành công!");
}
