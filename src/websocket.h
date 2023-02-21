#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include "models/message.h"
#include "chat_services/chatservice.h"
#include <QWebSocketServer>
#include <QWebSocket>

class ChatHandler;

class WebSocket : public QObject
{
    Q_OBJECT
public:
    explicit WebSocket(ChatHandler& chatHandler, QObject *parent = nullptr);
    void sendMessages(const QList<Message>& messages);
    void sendState();
    void sendAuthorValues(const QString& authorId, const QMap<Author::Role, QVariant>& values);

signals:

private:
    void send(const QJsonObject& object, const QList<QWebSocket*>& clients);

    void sendHelloToClient(QWebSocket* client);
    void sendMessagesToClient(const QList<Message>& messages, QWebSocket* client);
    void sendStateToClient(QWebSocket* client);
    void sendAuthorValuesToClient(QWebSocket* client, const QString& authorId, const QMap<Author::Role, QVariant>& values);

    QWebSocketServer server;
    QList<QWebSocket*> clients;
    ChatHandler& chatHandler;
};

#endif // WEBSOCKET_H
