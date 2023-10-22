#pragma once

#include "models/message.h"
#include "chat_services/chatservice.h"
#include <QWebSocketServer>
#include <QWebSocket>
#include <QTimer>

class ChatManager;

class WebSocket : public QObject
{
    Q_OBJECT
public:
    explicit WebSocket(ChatManager& chatManager, QObject *parent = nullptr);
    void sendMessages(const QList<std::shared_ptr<Message>>& messages);
    void sendState();
    void sendAuthorValues(const QString& authorId, const QMap<Author::Role, QVariant>& values);

signals:

private:
    void send(const QJsonObject& object, const QList<QWebSocket*>& clients);

    void sendHelloToClient(QWebSocket* client);
    void sendPing();
    void sendMessagesToClient(const QList<std::shared_ptr<Message>>& messages, QWebSocket* client);
    void sendStateToClient(QWebSocket* client);
    void sendAuthorValuesToClient(QWebSocket* client, const QString& authorId, const QMap<Author::Role, QVariant>& values);

    QWebSocketServer server;
    QList<QWebSocket*> clients;
    ChatManager& chatManager;
    QTimer sendPingTimer;
};
