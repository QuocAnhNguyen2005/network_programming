#ifndef AUDIOTESTDIALOG_H
#define AUDIOTESTDIALOG_H

#include <QDialog>
#include <QAudioSource>
#include <QAudioSink>
#include <QAudioFormat>
#include <QBuffer>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QListWidget>
#include <QMediaDevices>

class AudioTestDialog : public QDialog
{
    Q_OBJECT

public:
    AudioTestDialog(QWidget *parent = nullptr);
    ~AudioTestDialog();

private slots:
    void on_btnTestMic_clicked();
    void on_btnStopMic_clicked();
    void on_btnTestLoudspeaker_clicked();
    void on_btnListDevices_clicked();
    void onInputStateChanged(QAudio::State state);
    void onOutputStateChanged(QAudio::State state);
    void onMicDataReady();

private:
    // UI Components
    QPushButton *btnTestMic;
    QPushButton *btnStopMic;
    QPushButton *btnTestLoudspeaker;
    QPushButton *btnListDevices;
    QLabel *lblMicStatus;
    QLabel *lblLoudspeakerStatus;
    QComboBox *cmbInputDevice;
    QComboBox *cmbOutputDevice;
    QListWidget *logList;

    // Audio Components
    QAudioSource *audioInput;
    QAudioSink *audioOutput;
    QBuffer *recordBuffer;
    QBuffer *playbackBuffer;
    QByteArray recordedData;
    QAudioFormat audioFormat;

    void setupUI();
    void setupAudioFormat();
    void listAudioDevices();
    void logTest(const QString &msg);
};

#endif // AUDIOTESTDIALOG_H
