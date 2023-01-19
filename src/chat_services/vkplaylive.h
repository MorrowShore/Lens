#pragma once

#include "chatservice.h"
#include <QTimer>
#include <QWebSocket>

class VkPlayLive : public ChatService
{
public:
    explicit VkPlayLive(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;
    void reconnect() override;

private slots:
    void onWebSocketReceived(const QString &rawData);

private:
    static QString extractChannelName(const QString& stream);
    void send(const QJsonDocument &data);
    void sendParams(const QJsonObject& params, int method = -1);
    void parseMessage(const QJsonObject& data);
    void parseStreamInfo(const QByteArray& data);

    struct Info
    {
        QString token;
        QString wsChannel;
        int lastMessageId = 0;
        QString version;
    };

    QSettings& settings;
    QNetworkAccessManager& network;

    QTimer timerRequestToken;
    QTimer timerRequestChatPage;

    QWebSocket socket;
    Info info;
    QString lastConnectedChannelName;
};
