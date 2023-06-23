#pragma once

#include "chatservice.h"
#include "webinterceptorhandler.h"
#include "models/message.h"
#include <QTimer>
#include <QWebSocket>
#include <QJsonValue>

class Kick : public ChatService
{
    Q_OBJECT
public:
    explicit Kick(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;

protected:
    void reconnectImpl() override;

private slots:
    void onWebSocketReceived(const QString &rawData);

    void send(const QJsonObject& object);
    void sendSubscribe(const QString& chatroomId);
    void sendPing();

    void interceptChannelInfo(const QString& channelName);
    void onChannelInfoReply(const QByteArray& data);

    void parseChatMessageEvent(const QJsonObject& data);

private:
    struct Info
    {
        QString chatroomId;
    };

    static QString extractChannelName(const QString& stream);

    QNetworkAccessManager& network;
    QWebSocket socket;

    Info info;

    QTimer timerReconnect;
    QTimer timerPing;
    QTimer timerCheckPing;

    WebInterceptorHandler interceptor;
};

