#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , socket(new QTcpSocket(this))
{
    ui->setupUi(this);

    ui->txtHost->setText("127.0.0.1");
    ui->txtPort->setText(QString::number(DEFAULT_PORT)); //

    connect(socket, &QTcpSocket::connected, this, &MainWindow::onSocketConnected);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onSocketReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::onSocketDisconnected);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &MainWindow::onSocketError);

    ui->btnDisconnect->setEnabled(false);
    ui->groupBoxChat->setEnabled(false);
}

MainWindow::~MainWindow()
{
    if(socket->isOpen()) {
        socket->close();
    }
    delete ui;
}

void MainWindow::sendPacket(MessageType type, const QString &topic, const QByteArray &payload) {
    if(socket->state() != QAbstractSocket::ConnectedState) return;

    PacketHeader header;
    memset(&header, 0, sizeof(PacketHeader));

    header.msgType = type;
    header.payloadLength = (uint32_t)payload.size();
    header.timestamp = QDateTime::currentMSecsSinceEpoch();

    QByteArray userBytes = currentUsername.toUtf8();
    strncpy(header.sender, userBytes.constData(), MAX_USERNAME_LEN - 1);

    QByteArray topicBytes = topic.toUtf8();
    strncpy(header.topic, topicBytes.constData(), MAX_TOPIC_LEN - 1);

    socket->write((const char*)&header, sizeof(PacketHeader));

    if(header.payloadLength > 0) {
        socket->write(payload);
    }

    socket->flush();
}


void MainWindow::on_btnConnect_clicked()
{
    QString host = ui->txtHost->text();
    int port = ui->txtPort->text().toInt();
    currentUsername = ui->txtUsername->text();

    if(currentUsername.isEmpty()) {
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
}

void MainWindow::onSocketError(QAbstractSocket::SocketError socketError)
{
    logMessage("Lỗi Socket: " + socket->errorString());
}


void MainWindow::onSocketReadyRead()
{
    while (socket->bytesAvailable() > 0) {

        if (socket->bytesAvailable() < (qint64)sizeof(PacketHeader)) {
            return;
        }

        PacketHeader header;
        socket->peek((char*)&header, sizeof(PacketHeader));

        if (socket->bytesAvailable() < (qint64)sizeof(PacketHeader) + header.payloadLength) {
            return;
        }

        socket->read((char*)&header, sizeof(PacketHeader));
        QByteArray payload;
        if (header.payloadLength > 0) {
            payload = socket->read(header.payloadLength);
        }

        QString sender(header.sender);
        QString topic(header.topic);

        switch (header.msgType) {
        case MSG_ACK:
            logMessage(QString("[SERVER] ACK: Yêu cầu đã được xử lý (Topic: %1)").arg(topic));
            break;
        case MSG_PUBLISH_TEXT:
            ui->listWidgetLog->addItem(QString("[%1] %2: %3")
                                           .arg(topic)
                                           .arg(sender)
                                           .arg(QString::fromUtf8(payload)));
            ui->listWidgetLog->scrollToBottom();
            break;
        case MSG_PUBLISH_FILE:
            logMessage(QString("[%1] %2 đã gửi một file (%3 bytes).")
                           .arg(topic).arg(sender).arg(header.payloadLength));
            break;
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
    if (topic.isEmpty()) return;

    sendPacket(MSG_SUBSCRIBE, topic, "");
    logMessage("Đã gửi yêu cầu Subscribe: " + topic);
}

void MainWindow::on_btnUnsubscribe_clicked()
{
    QString topic = ui->txtTopicSub->text();
    if (topic.isEmpty()) return;

    sendPacket(MSG_UNSUBSCRIBE, topic, "");
    logMessage("Đã gửi yêu cầu Unsubscribe: " + topic);
}

void MainWindow::on_btnSend_clicked()
{
    QString topic = ui->txtTopicPub->text();
    QString msg = ui->txtMessage->text();

    if (topic.isEmpty() || msg.isEmpty()) return;

    sendPacket(MSG_PUBLISH_TEXT, topic, msg.toUtf8());

    ui->txtMessage->clear();
}

void MainWindow::on_btnBrowseFile_clicked()
{
    QString topic = ui->txtTopicPub->text();
    if (topic.isEmpty()) {
        QMessageBox::warning(this, "Cảnh báo", "Vui lòng nhập Topic trước khi gửi file.");
        return;
    }

    QString filePath = QFileDialog::getOpenFileName(this, "Chọn file để gửi");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "Lỗi", "Không thể đọc file!");
        return;
    }

    QByteArray fileData = file.readAll();
    file.close();

    if (fileData.size() > MAX_BUFFER_SIZE) {
        QMessageBox::warning(this, "Lỗi", "File quá lớn so với buffer server!");
        return;
    }

    sendPacket(MSG_PUBLISH_FILE, topic, fileData);
    logMessage("Đang gửi file: " + QFileInfo(filePath).fileName());
}

void MainWindow::logMessage(const QString &msg) {
    QString time = QDateTime::currentDateTime().toString("HH:mm:ss");
    ui->listWidgetLog->addItem("[" + time + "] " + msg);
    ui->listWidgetLog->scrollToBottom();
}
