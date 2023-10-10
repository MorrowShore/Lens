#pragma once

#include <QNetworkAccessManager>
#include <QElapsedTimer>

class BackendManager : public QObject
{
    Q_OBJECT
public:
    explicit BackendManager(QNetworkAccessManager& network, QObject *parent = nullptr);
    ~BackendManager();

private:
    void sendSessionUsage();

    QNetworkAccessManager& network;

    QElapsedTimer usageDuration;
    const QDateTime startTime;
};
