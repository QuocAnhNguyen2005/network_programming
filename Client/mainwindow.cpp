#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "audiodialog.h"
#include "audioplayerdialog.h"
#include "audiotestdialog.h"

#include <QMessageBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QDebug>
#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
      socket(new QTcpSocket(this)),
      streamSocket(new QTcpSocket(this)),
      audioDialog(nullptr)
{
    ui->setupUi(this);

    // Set default connection values
    ui->txtHost->setText("127.0.0.1");
    ui->txtPort->setText(QString::number(DEFAULT_PORT));

    // Connect socket signals
    connect(socket, &QTcpSocket::connected, this, &MainWindow::onSocketConnected);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onSocketReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::onSocketDisconnected);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &MainWindow::onSocketError);

    // Connect stream socket signals
    connect(streamSocket, &QTcpSocket::connected, this, &MainWindow::onStreamConnected);
    connect(streamSocket, &QTcpSocket::disconnected, this, &MainWindow::onStreamDisconnected);

    // Connect topic changed signal
    connect(ui->txtTopicPub, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &MainWindow::onTopicChanged);

    // Connect message list click signal for replay/download
    connect(ui->listMessage, &QListWidget::itemDoubleClicked,
            this, &MainWindow::on_messageListItemClicked);

    // Initial UI state
    ui->btnDisconnect->setEnabled(false);
    ui->groupBoxChat->setEnabled(false);
    ui->groupTopic->setEnabled(false);
    if (ui->btnAudio)
    {
        ui->btnAudio->setEnabled(false);
    }
}

MainWindow::~MainWindow()
{
    if (socket->isOpen())
    {
        socket->close();
    }
    if (streamSocket->isOpen())
    {
        streamSocket->close();
    }
    if (audioDialog)
    {
        delete audioDialog;
    }
    delete ui;
}

void MainWindow::sendPacket(MessageType type, const QString &topic, const QByteArray &payload)
{
    if (socket->state() != QAbstractSocket::ConnectedState)
        return;

    PacketHeader header;
    std::memset(&header, 0, sizeof(PacketHeader));

    header.msgType = type;
    header.payloadLength = (uint32_t)payload.size();
    header.timestamp = QDateTime::currentMSecsSinceEpoch();
    header.messageId = static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFF);

    QByteArray userBytes = currentUsername.toUtf8();
    std::strncpy(header.sender, userBytes.constData(), MAX_USERNAME_LEN - 1);

    QByteArray topicBytes = topic.toUtf8();
    std::strncpy(header.topic, topicBytes.constData(), MAX_TOPIC_LEN - 1);

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
        QMessageBox::warning(this, "Error", "Please enter a username!");
        return;
    }

    logMessage("Connecting to " + host + ":" + QString::number(port) + "...");
    socket->connectToHost(host, port);
}

void MainWindow::onSocketConnected()
{
    logMessage("TCP Connection established!");

    // Send LOGIN packet
    sendPacket(MSG_LOGIN, "", "");

    // Connect to stream server on port 8081
    if (streamSocket->state() != QAbstractSocket::ConnectedState)
    {
        QString host = ui->txtHost->text();
        logMessage("Connecting to stream server on port 8081...");
        streamSocket->connectToHost(host, 8081);
    }

    // Update UI
    ui->btnConnect->setEnabled(false);
    ui->txtHost->setEnabled(false);
    ui->txtPort->setEnabled(false);
    ui->txtUsername->setEnabled(false);

    ui->btnDisconnect->setEnabled(true);
    ui->groupBoxChat->setEnabled(true);
    ui->groupTopic->setEnabled(true);
    if (ui->btnAudio)
    {
        ui->btnAudio->setEnabled(true);
    }
}

void MainWindow::on_btnDisconnect_clicked()
{
    sendPacket(MSG_LOGOUT, "", "");
    socket->disconnectFromHost();
}

void MainWindow::onSocketDisconnected()
{
    logMessage("Disconnected from server.");

    ui->btnConnect->setEnabled(true);
    ui->txtHost->setEnabled(true);
    ui->txtPort->setEnabled(true);
    ui->txtUsername->setEnabled(true);

    ui->btnDisconnect->setEnabled(false);
    ui->groupBoxChat->setEnabled(false);
    ui->groupTopic->setEnabled(false);
    if (ui->btnAudio)
    {
        ui->btnAudio->setEnabled(false);
    }
}

void MainWindow::onSocketError(QAbstractSocket::SocketError socketError)
{
    logMessage("Socket Error: " + socket->errorString());
}

void MainWindow::onSocketReadyRead()
{
    while (socket->bytesAvailable() > 0)
    {
        if (socket->bytesAvailable() < (qint64)sizeof(PacketHeader))
            return;

        PacketHeader header;
        socket->peek((char *)&header, sizeof(PacketHeader));

        if (socket->bytesAvailable() < (qint64)sizeof(PacketHeader) + header.payloadLength)
            return;

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
            logMessage("[ACK] Request processed for topic: " + topic);
            break;

        case MSG_PUBLISH_TEXT:
        {
            MessageData msgData;
            msgData.type = MessageData::TEXT;
            msgData.sender = sender;
            msgData.topic = topic;
            msgData.content = QString::fromUtf8(payload);
            msgData.timestamp = QDateTime::currentMSecsSinceEpoch();

            addMessageToHistory(msgData);
            break;
        }

        case MSG_PUBLISH_FILE:
        {
            MessageData msgData;
            msgData.type = MessageData::FILE;
            msgData.sender = sender;
            msgData.topic = topic;
            msgData.content = QString("file_%1.bin").arg(header.messageId);
            msgData.data = payload;
            msgData.timestamp = QDateTime::currentMSecsSinceEpoch();

            addMessageToHistory(msgData);
            logMessage("[FILE] Received file from " + sender + " (" +
                       QString::number(payload.size()) + " bytes) - Double-click to download");
            break;
        }

        case MSG_STREAM_START:
        {
            QString streamKey = sender + ":" + topic;
            audioFrameBuffers[streamKey].clear();
            audioStreamActive[streamKey] = true;
            audioQualityMap[streamKey] = header.flags; // Store quality from flags field
            logMessage("[AUDIO] Stream started from " + sender + " on topic " + topic +
                       " (Quality: " + QString::number(header.flags) + ")");
            break;
        }

        case MSG_STREAM_FRAME:
        {
            // Accumulate audio frames
            QString streamKey = sender + ":" + topic;
            if (audioStreamActive.value(streamKey, false))
            {
                audioFrameBuffers[streamKey].append(payload);
                logMessage("[AUDIO] Received frame from " + sender + " (" +
                           QString::number(payload.size()) + " bytes, total: " +
                           QString::number(audioFrameBuffers[streamKey].size()) + " bytes)");
            }
            break;
        }

        case MSG_STREAM_STOP:
        {
            // Audio stream finished - save complete audio
            QString streamKey = sender + ":" + topic;
            if (audioStreamActive.value(streamKey, false))
            {
                audioStreamActive[streamKey] = false;
                QByteArray completeAudio = audioFrameBuffers[streamKey];

                if (!completeAudio.isEmpty())
                {
                    int quality = audioQualityMap.value(streamKey, 1); // Default to medium
                    qDebug() << "[MainWindow] Complete audio from" << sender << "- Size:" << completeAudio.size()
                             << "bytes, Quality:" << quality;
                    logMessage("[AUDIO] Stream finished from " + sender +
                               " (Total: " + QString::number(completeAudio.size()) + " bytes, Quality: " +
                               QString::number(quality) + ")");

                    // Verify audio data
                    if (completeAudio.size() < 100)
                    {
                        qDebug() << "[MainWindow] WARNING: Audio data very small (" << completeAudio.size() << "bytes)";
                    }

                    // Store as complete audio message
                    MessageData msgData;
                    msgData.type = MessageData::AUDIO;
                    msgData.sender = sender;
                    msgData.topic = topic;
                    msgData.content = "Audio from " + sender;
                    msgData.data = completeAudio;
                    msgData.timestamp = QDateTime::currentMSecsSinceEpoch();
                    msgData.audioQuality = quality;

                    addMessageToHistory(msgData);
                }

                // Clear buffer and quality for next stream
                audioFrameBuffers.remove(streamKey);
                audioQualityMap.remove(streamKey);
            }
            break;
        }

        case MSG_ERROR:
            logMessage("[ERROR] " + QString::fromUtf8(payload));
            break;

        default:
            logMessage("Received message type: " + QString::number(header.msgType));
        }
    }
}

void MainWindow::on_btnSubscribe_clicked()
{
    QString topic = ui->txtTopicSub->text();
    if (topic.isEmpty())
    {
        QMessageBox::warning(this, "Error", "Please enter a topic!");
        return;
    }

    sendPacket(MSG_SUBSCRIBE, topic, "");
    logMessage("Subscribed to: " + topic);
    ui->txtTopicPub->addItem(topic);
}

void MainWindow::on_btnUnsubscribe_clicked()
{
    QString topic = ui->txtTopicSub->text();
    if (topic.isEmpty())
    {
        QMessageBox::warning(this, "Error", "Please enter a topic!");
        return;
    }

    sendPacket(MSG_UNSUBSCRIBE, topic, "");
    logMessage("Unsubscribed from: " + topic);
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
        QMessageBox::warning(this, "Error", "Please select topic and enter message!");
        return;
    }

    sendPacket(MSG_PUBLISH_TEXT, topic, msg.toUtf8());

    ui->txtMessage->clear();
}

void MainWindow::onTopicChanged(const QString &topic)
{
    ui->listMessage->clear();
    ui->listMessage->addItem("=== Messages for topic: " + topic + " ===");

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
        QMessageBox::warning(this, "Warning", "Please select topic before sending file.");
        return;
    }

    QString filePath = QFileDialog::getOpenFileName(this, "Select file to send");
    if (filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        QMessageBox::critical(this, "Error", "Cannot read file!");
        return;
    }

    QByteArray fileData = file.readAll();
    file.close();

    if (fileData.size() > MAX_BUFFER_SIZE)
    {
        QMessageBox::warning(this, "Error", "File exceeds buffer size!");
        return;
    }

    sendPacket(MSG_PUBLISH_FILE, topic, fileData);
    logMessage("Sending file: " + QFileInfo(filePath).fileName());
}

void MainWindow::on_btnAudio_clicked()
{
    QString topic = ui->txtTopicPub->currentText();
    if (topic == "None" || topic.isEmpty())
    {
        QMessageBox::warning(this, "Warning", "Please select topic first.");
        return;
    }

    if (!audioDialog)
    {
        audioDialog = new AudioDialog(socket, currentUsername, this);
        audioDialog->setStreamSocket(streamSocket);

        // Connect stream socket to server stream port if needed
        if (streamSocket->state() != QAbstractSocket::ConnectedState)
        {
            QString host = ui->txtHost->text();
            streamSocket->connectToHost(host, 8081); // Stream port
        }
    }

    audioDialog->setCurrentTopic(topic);
    audioDialog->exec();
}

void MainWindow::on_btnTestAudio_clicked()
{
    AudioTestDialog testDialog(this);
    testDialog.exec();
}

void MainWindow::onStreamConnected()
{
    logMessage("Stream connection established!");
}

void MainWindow::onStreamDisconnected()
{
    logMessage("Stream connection closed.");
}

void MainWindow::logMessage(const QString &msg)
{
    QString time = QDateTime::currentDateTime().toString("HH:mm:ss");
    ui->listWidgetLog->addItem("[" + time + "] " + msg);
    ui->listWidgetLog->scrollToBottom();
}

void MainWindow::addMessageToHistory(const MessageData &msgData)
{
    // Store in data map
    messageDataStore[msgData.topic].append(msgData);

    // Create display text
    QString displayText;
    QIcon icon;

    switch (msgData.type)
    {
    case MessageData::TEXT:
        displayText = QString("[%1] %2: %3")
                          .arg(msgData.topic)
                          .arg(msgData.sender)
                          .arg(msgData.content);
        break;

    case MessageData::FILE:
        displayText = QString("📁 [%1] %2 sent file (%3 bytes) - Double-click to download")
                          .arg(msgData.topic)
                          .arg(msgData.sender)
                          .arg(msgData.data.size());
        break;

    case MessageData::AUDIO:
        displayText = QString("🔊 [%1] Audio from %2 (%3 bytes) - Double-click to play")
                          .arg(msgData.topic)
                          .arg(msgData.sender)
                          .arg(msgData.data.size());
        break;
    }

    topicMessages[msgData.topic].append(displayText);

    if (ui->txtTopicPub->currentText() == msgData.topic)
    {
        ui->listMessage->addItem(displayText);
        ui->listMessage->scrollToBottom();
    }
}

void MainWindow::on_messageListItemClicked(QListWidgetItem *item)
{
    QString text = item->text();
    QString currentTopic = ui->txtTopicPub->currentText();

    // Find the corresponding message data
    if (!messageDataStore.contains(currentTopic))
        return;

    const QList<MessageData> &messages = messageDataStore[currentTopic];

    // Match by finding the message in the list
    for (int i = 0; i < messages.size(); i++)
    {
        const MessageData &msg = messages[i];

        if (msg.type == MessageData::FILE && text.contains(msg.sender) && text.contains("file"))
        {
            downloadFile(msg.content, msg.data);
            return;
        }
        else if (msg.type == MessageData::AUDIO && text.contains(msg.sender) && text.contains("Audio"))
        {
            replayAudio(msg.data, msg.audioQuality);
            return;
        }
    }
}

void MainWindow::downloadFile(const QString &filename, const QByteArray &fileData)
{
    QString savePath = QFileDialog::getSaveFileName(
        this,
        "Save File",
        QDir::homePath() + "/" + filename,
        "All Files (*.*)");

    if (!savePath.isEmpty())
    {
        QFile file(savePath);
        if (file.open(QIODevice::WriteOnly))
        {
            file.write(fileData);
            file.close();
            QMessageBox::information(this, "Success",
                                     "File saved to: " + savePath);
            logMessage("[DOWNLOAD] File saved: " + savePath);
        }
        else
        {
            QMessageBox::critical(this, "Error", "Failed to save file!");
        }
    }
}

void MainWindow::replayAudio(const QByteArray &audioData, int quality)
{
    if (audioData.isEmpty())
    {
        QMessageBox::warning(this, "Warning", "No audio data available");
        return;
    }

    logMessage("[AUDIO] Opening audio player for " + QString::number(audioData.size()) +
               " bytes (Quality: " + QString::number(quality) + ")");

    // Create and show audio player dialog with quality
    AudioPlayerDialog *playerDialog = new AudioPlayerDialog(audioData, quality, this);
    playerDialog->setAttribute(Qt::WA_DeleteOnClose);
    playerDialog->show();
}
