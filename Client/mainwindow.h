#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QAbstractSocket>
#include <QListWidget>
#include "../protocol.h"
#include <QMap>
#include <QString>
#include <QStringList>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class AudioDialog;

// Structure to store message data
struct MessageData
{
    enum Type
    {
        TEXT,
        FILE,
        AUDIO
    };
    Type type;
    QString sender;
    QString topic;
    QString content; // Text content or file name
    QByteArray data; // File or audio data
    qint64 timestamp;
    int audioQuality; // 0=8kHz, 1=16kHz, 2=48kHz (for AUDIO type)
};

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
    void on_btnAudio_clicked();
    void on_btnTestAudio_clicked();
    void on_messageListItemClicked(QListWidgetItem *item);

    void onSocketConnected();
    void onSocketReadyRead();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onTopicChanged(const QString &topic);

    void onStreamConnected();
    void onStreamDisconnected();

private:
    Ui::MainWindow *ui;
    QTcpSocket *socket;
    QTcpSocket *streamSocket;
    AudioDialog *audioDialog;

    QString currentUsername;
    QMap<QString, QStringList> topicMessages;
    QMap<QString, QList<MessageData>> messageDataStore; // Store actual message data

    // Audio streaming buffers - key is "sender:topic"
    QMap<QString, QByteArray> audioFrameBuffers;
    QMap<QString, bool> audioStreamActive;
    QMap<QString, int> audioQualityMap; // Track audio quality per stream

    void sendPacket(MessageType type, const QString &topic, const QByteArray &payload);
    void logMessage(const QString &msg);
    void addMessageToHistory(const MessageData &msgData);
    void replayAudio(const QByteArray &audioData, int quality = 1);
    void downloadFile(const QString &filename, const QByteArray &fileData);
};
#endif
