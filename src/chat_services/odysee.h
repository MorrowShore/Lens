#pragma once

#include "chatservice.h"
#include <QJsonObject>
#include <QWebSocket>
#include <QTimer>

class Odysee : public ChatService
{
    Q_OBJECT
public:
    explicit Odysee(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;

protected:
    void reconnectImpl() override;
    void onReceiveWebSocket(const QString& rawData);

private slots:
    void requestClaimId();
    void requestLive();
    void sendPing();

private:
    struct Info
    {
        QString channel;
        QString video;
        QString claimId;
    };

    void parseComment(const QJsonObject& data);
    void parseRemoved(const QJsonObject& data);

    QNetworkAccessManager& network;
    QWebSocket socket;

    QTimer timerReconnect;
    QTimer pingTimer;
    QTimer checkPingTimer;

    Info info;
};
