#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QAbstractSocket>
#include "../protocol.h"

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

private:
    Ui::MainWindow *ui;
    QTcpSocket *socket;

    QString currentUsername;
    QMap<QString, QStringList> topicMessages;

    void sendPacket(MessageType type, const QString &topic, const QByteArray &payload);

    void logMessage(const QString &msg);
};
#endif
