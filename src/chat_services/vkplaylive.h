#pragma once

#include "chatservice.h"
#include <QTimer>
#include <QWebSocket>

class VkPlayLive : public ChatService
{
    Q_OBJECT
public:
    explicit VkPlayLive(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);
    
    ConnectionState getConnectionState() const override;
    QString getStateDescription() const override;

protected:
    void reconnectImpl() override;

private slots:
    void onWebSocketReceived(const QString &rawData);

private:
    static QString extractChannelName(const QString& stream);
    void send(const QJsonDocument &data);
    void sendParams(const QJsonObject& params, int method = -1);
    void sendHeartbeat();
    void parseMessage(const QJsonObject& data);

    struct Info
    {
        QString token;
        QString wsChannel;
        int lastMessageId = 0;
        QString version;
    };

    QNetworkAccessManager& network;

    QTimer timerRequestToken;
    QTimer heartbeatTimer;
    QTimer heartbeatAcknowledgementTimer;

    QWebSocket socket;
    Info info;
};
