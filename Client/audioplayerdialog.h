#ifndef AUDIOPLAYERDIALOG_H
#define AUDIOPLAYERDIALOG_H

#include <QDialog>
#include <QAudioSink>
#include <QAudioFormat>
#include <QBuffer>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QProgressBar>
#include <QTimer>

class AudioPlayerDialog : public QDialog
{
    Q_OBJECT

public:
    AudioPlayerDialog(const QByteArray &audioData, int quality = 1, QWidget *parent = nullptr);
    ~AudioPlayerDialog();

private slots:
    void on_btnPlay_clicked();
    void on_btnPause_clicked();
    void on_btnStop_clicked();
    void onAudioStateChanged(QAudio::State state);
    void updateProgressBar();

private:
    // UI Elements
    QPushButton *btnPlay;
    QPushButton *btnPause;
    QPushButton *btnStop;
    QLabel *lblStatus;
    QLabel *lblDuration;
    QProgressBar *progressBar;
    QSlider *volumeSlider;

    // Audio components
    QAudioSink *audioOutput;
    QBuffer *audioBuffer;
    QByteArray audioData;
    QTimer *progressTimer;
    bool isPlaying;

    // Audio format
    QAudioFormat audioFormat;

    void setupUI();
    void setupAudioFormat(int quality);
    void startPlayback();
    void stopPlayback();
};

#endif // AUDIOPLAYERDIALOG_H
