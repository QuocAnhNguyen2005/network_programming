#ifndef AUDIODIALOG_H
#define AUDIODIALOG_H

#include <QDialog>
#include <QTcpSocket>
#include <QAudioSource>
#include <QAudioSink>
#include <QMediaDevices>
#include <QAudioFormat>
#include <QIODevice>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QComboBox>
#include <QProgressBar>
#include "../protocol.h"

class AudioDialog : public QDialog
{
    Q_OBJECT

public:
    AudioDialog(QTcpSocket *chatSocket, const QString &username, QWidget *parent = nullptr);
    ~AudioDialog();

    void setStreamSocket(QTcpSocket *socket) { streamSocket = socket; }
    void setCurrentTopic(const QString &topic) { currentTopic = topic; }

private slots:
    void on_btnStartAudio_clicked();
    void on_btnStopAudio_clicked();
    void on_btnSelectAudioDevice_clicked();
    void onAudioInputReady();
    void onStreamReadyRead();
    void onStreamConnected();
    void onStreamDisconnected();

private:
    // UI Elements
    QPushButton *btnStartAudio;
    QPushButton *btnStopAudio;
    QPushButton *btnSelectAudioDevice;
    QLabel *lblStatus;
    QLabel *lblDevice;
    QListWidget *listAudioLog;
    QProgressBar *audioLevel;
    QComboBox *cmbAudioQuality;

    // Socket and networking
    QTcpSocket *chatSocket;
    QTcpSocket *streamSocket;

    // Audio components
    QAudioSource *audioSource = nullptr;
    QAudioSink *audioSink = nullptr;
    QIODevice *audioInDevice = nullptr;
    QIODevice *audioOutDevice = nullptr;

    // State
    bool isStreaming = false;
    uint32_t streamSessionId = 0;
    QString username;
    QString currentTopic;
    int frameCounter = 0;

    // Helper methods
    void setupUI();
    void sendStreamPacket(MessageType type, const QString &topic,
                          const QByteArray &payload, uint8_t flags = 0);
    void logAudio(const QString &msg);
    void startAudioCapture();
    void stopAudioCapture();
};

#endif // AUDIODIALOG_H
