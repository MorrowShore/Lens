#include "websocket.h"
#include "chathandler.h"
#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

namespace
{

static const int Port = 8355;
static const int LastMessagesCount = 30;

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
            qCritical() << "!client";
            return;
        }

        connect(client, &QWebSocket::textMessageReceived, this, [](const QString &message)
        {
            qInfo() << "RECEIVED:" << message;
        });

        connect(client, &QWebSocket::disconnected, this, [this]
        {
            QWebSocket* client = qobject_cast<QWebSocket*>(sender());

            qInfo() << "Client disconnected";

            const int removedCount = clients.removeAll(client);
            if (removedCount != 1)
            {
                qWarning() << "removed clients count != 1";
            }
        });

        qInfo() << "New client connected";

        clients.append(client);

        sendHelloToClient(client);
        sendStateToClient(client);

        const QList<std::shared_ptr<Message>> lastMessages = chatHandler.getMessagesModel().getLastMessages(LastMessagesCount);
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

void WebSocket::sendMessages(const QList<std::shared_ptr<Message>>& messages)
{
    for (QWebSocket* client : qAsConst(clients))
    {
        sendMessagesToClient(messages, client);
    }
}

void WebSocket::sendState()
{
    for (QWebSocket* client : qAsConst(clients))
    {
        sendStateToClient(client);
    }
}

void WebSocket::sendAuthorValues(const QString &authorId, const QMap<Author::Role, QVariant> &values)
{
    for (QWebSocket* client : qAsConst(clients))
    {
        sendAuthorValuesToClient(client, authorId, values);
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

void WebSocket::sendHelloToClient(QWebSocket *client)
{
    QJsonObject root;
    root.insert("type", "hello");

    QJsonArray jsonServices;

    for (const std::shared_ptr<ChatService>& service : chatHandler.getServices())
    {
        if (!service)
        {
            qWarning() << "servies is null";
            continue;
        }

        jsonServices.append(service->getStaticInfoJson());
    }

    QJsonObject data;

    data.insert("app_name", QCoreApplication::applicationName());
    data.insert("app_version", QCoreApplication::applicationVersion());
    data.insert("services", jsonServices);

    root.insert("data", data);

    send(root, {client});
}

void WebSocket::sendMessagesToClient(const QList<std::shared_ptr<Message>> &messages, QWebSocket *client)
{
    QJsonObject root;
    root.insert("type", "messages");

    QJsonArray jsonMessages;

    for (const std::shared_ptr<Message>& message : messages)
    {
        if (!message)
        {
            qWarning() << "message is null";
            continue;
        }

        if (message->isHasFlag(Message::Flag::DeleterItem))
        {
            continue;
        }

        const QString authorId = message->getAuthorId();
        const std::shared_ptr<Author> author = chatHandler.getMessagesModel().getAuthor(authorId);
        if (!author)
        {
            qCritical() << "author id" << authorId << "not found, message id =" << message->getId();
            continue;
        }

        jsonMessages.append(message->toJson(*author));
    }

    QJsonObject data;
    data.insert("messages", jsonMessages);

    root.insert("data", data);

    send(root, {client});
}

void WebSocket::sendStateToClient(QWebSocket *client)
{
    QJsonObject root;
    root.insert("type", "state");

    QJsonArray jsonServices;

    for (const std::shared_ptr<ChatService>& service : chatHandler.getServices())
    {
        if (!service)
        {
            qWarning() << "servies is null";
            continue;
        }

        jsonServices.append(service->getStateJson());
    }

    QJsonObject data;
    data.insert("services", jsonServices);
    data.insert("viewers", chatHandler.getViewersTotalCount());

    root.insert("data", data);

    send(root, {client});
}

void WebSocket::sendAuthorValuesToClient(QWebSocket *client, const QString &authorId, const QMap<Author::Role, QVariant> &values)
{
    QJsonObject root;
    root.insert("type", "author_values");

    QJsonObject data;
    data.insert("author_id", authorId);

    QJsonObject jsonValues;
    const QList<Author::Role> keys = values.keys();
    for (const Author::Role key : qAsConst(keys))
    {
        const QString jsonKey = Author::getJsonRoleName(key);
        const QJsonValue jsonValue = QJsonValue::fromVariant(values[key]);
        jsonValues.insert(jsonKey, jsonValue);

    }
    data.insert("values", jsonValues);

    root.insert("data", data);

    send(root, {client});
}

