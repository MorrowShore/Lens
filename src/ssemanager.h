#pragma once

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

class SseManager : public QObject
{
    Q_OBJECT
public:
    explicit SseManager(QNetworkAccessManager& network, QObject *parent = nullptr);
    ~SseManager();

    void start(const QUrl& url);
    void stop();

signals:
    void readyRead(const QByteArray& data);
    void started();
    void stopped();

private:
    QNetworkAccessManager& network;
    QNetworkReply* reply = nullptr;
    bool isStarted = false;
};
