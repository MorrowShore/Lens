#pragma once

#include <QNetworkAccessManager>
#include <QElapsedTimer>

class BackendManager : public QObject
{
    Q_OBJECT
public:
    explicit BackendManager(QNetworkAccessManager& network, QObject *parent = nullptr);

public slots:
    void sendSessionUsage();

private:
    QNetworkAccessManager& network;

    QElapsedTimer usageDuration;
    const QDateTime startTime;
};
