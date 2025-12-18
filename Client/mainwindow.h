#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QAbstractSocket>
#include "../protocol.h"
#include <QAudioSource>
#include <QAudioSink>
#include <QMediaDevices>
#include <QAudioFormat>
#include <QIODevice>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnConnect_clicked();
    void on_btnDisconnect_clicked();
    void on_btnSubscribe_clicked();
    void on_btnUnsubscribe_clicked();
    void on_btnSend_clicked();
    void on_btnBrowseFile_clicked();

    void onSocketConnected();
    void onSocketReadyRead();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onTopicChanged(const QString &topic);

private slots:
    void onStreamConnected();
    void onStreamReadyRead();
    void onStreamDisconnected();
    void startAudioStream(const QString &topic);
    void stopAudioStream();
    void sendAudioFrame(const QByteArray &data);

private:
    Ui::MainWindow *ui;
    QTcpSocket *socket;
    QTcpSocket *streamSocket;

    // ===== AUDIO STREAM =====
    QAudioSource *audioSource = nullptr;
    QAudioSink *audioSink = nullptr;

    QIODevice *audioInDevice = nullptr;
    QIODevice *audioOutDevice = nullptr;

    bool isStreaming = false;
    uint32_t streamSessionId = 0;
    bool audioOutputReady = false;

    QString currentUsername;
    QMap<QString, QStringList> topicMessages;

    void sendPacket(MessageType type, const QString &topic, const QByteArray &payload);
    void sendStreamPacket(MessageType type, const QString &topic, const QByteArray &payload,
                          uint32_t sessionId,
                          uint8_t flags);

    void logMessage(const QString &msg);
};
#endif
