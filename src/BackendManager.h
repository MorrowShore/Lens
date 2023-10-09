#pragma once

#include <QNetworkAccessManager>

class BackendManager : public QObject
{
    Q_OBJECT
public:
    explicit BackendManager(QNetworkAccessManager& network, QObject *parent = nullptr);
    void sendStarted();

private:
    void sendEvent(const QDateTime& time, const QString& type, const QJsonValue& data);

    QNetworkAccessManager& network;

};
