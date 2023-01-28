#include "websocket.h"
#include "chathandler.h"
#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

namespace
{

static int Port = 12345;

}

WebSocket::WebSocket(ChatHandler& chatHandler_, QObject *parent)
    : QObject{parent}
    , server(QCoreApplication::applicationName(), QWebSocketServer::NonSecureMode)
    , chatHandler(chatHandler_)
{
    connect(&server, &QWebSocketServer::closed, this, []()
    {
        qInfo() << "Web socket server closed";
    });

    connect(&server, &QWebSocketServer::newConnection, this, [this]()
    {
        QWebSocket* client = server.nextPendingConnection();
        if (!client)
        {
            qCritical() << Q_FUNC_INFO << ": !client";
            return;
        }

        connect(client, &QWebSocket::textMessageReceived, this, [](const QString &message)
        {
            qInfo() << ": RECEIVED:" << message;
        });

        connect(client, &QWebSocket::disconnected, this, [this]
        {
            QWebSocket* client = qobject_cast<QWebSocket*>(sender());

            qInfo() << "Client disconnected";

            const int removedCount = clients.removeAll(client);
            if (removedCount != 1)
            {
                qWarning() << Q_FUNC_INFO << ": removed clients count != 1";
            }
        });

        qInfo() << "New client connected";

        clients.append(client);

        sendInfoToClient(client);

        const QList<Message> lastMessages = chatHandler.getMessagesModel().getLastMessages(30);
        sendMessagesToClient(lastMessages, client);
    });

    connect(&server, &QWebSocketServer::serverError, this, [this](QWebSocketProtocol::CloseCode closeCode)
    {
        qCritical() << "Web socket server error" << closeCode << ":" << server.errorString();
    });

    if (server.listen(QHostAddress::LocalHost, Port))
    {
        //qInfo() << "Web socket server started" << server.serverUrl().toString();
    }
    else
    {
        qCritical() << "Failed to open web socket server";
    }
}

void WebSocket::sendMessages(const QList<Message>& messages)
{
    for (QWebSocket* client : qAsConst(clients))
    {
        sendMessagesToClient(messages, client);
    }
}

void WebSocket::sendInfo()
{
    for (QWebSocket* client : qAsConst(clients))
    {
        sendInfoToClient(client);
    }
}

void WebSocket::send(const QJsonObject& object, const QList<QWebSocket*>& clients)
{
    if (clients.isEmpty())
    {
        return;
    }

    const QString message = QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::JsonFormat::Compact));

    for (QWebSocket* client : qAsConst(clients))
    {
        client->sendTextMessage(message);
    }

    //qDebug("SENDED: " + message.toUtf8());
}

void WebSocket::sendMessagesToClient(const QList<Message> &messages, QWebSocket *client)
{
    QJsonObject root;
    root.insert("type", "chat_messages");

    QJsonArray jsonMessages;

    for (const Message& message : messages)
    {
        if (message.isHasFlag(Message::Flag::DeleterItem))
        {
            continue;
        }

        const QString authorId = message.getAuthorId();
        const Author* author = chatHandler.getMessagesModel().getAuthor(authorId);
        if (!author)
        {
            qCritical() << Q_FUNC_INFO << ": author id" << authorId << "not found, message id =" << message.getId();
            continue;
        }

        jsonMessages.append(message.toJson(*author));
    }

    QJsonObject data;
    data.insert("messages", jsonMessages);

    root.insert("data", data);

    send(root, {client});
}

void WebSocket::sendInfoToClient(QWebSocket *client)
{
    QJsonObject root;
    root.insert("type", "info");

    QJsonArray jsonServices;

    for (const ChatService* service : chatHandler.getServices())
    {
        jsonServices.append(service->toJson());
    }

    QJsonObject data;
    data.insert("services", jsonServices);
    data.insert("viewersCountTotal", chatHandler.getViewersTotalCount());

    root.insert("data", data);

    send(root, {client});
}

