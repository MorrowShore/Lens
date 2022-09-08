#include "goodgame.h"
#include "models/chatmessagesmodle.hpp"
#include "models/chatmessage.h"
#include "models/chatauthor.h"
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace
{

static const int RequestChatInterval = 2000;

static const int RequestChannelStatus = 10000;

}

GoodGame::GoodGame(QSettings& settings_, const QString& settingsGroupPath, QNetworkAccessManager& network_, QObject *parent)
    : ChatService(settings_, settingsGroupPath, ChatService::ServiceType::GoodGame, parent)
    , settings(settings_)
    , network(network_)
{
    getParameter(stream)->setPlaceholder(tr("Link or channel name..."));

    QObject::connect(&_socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState)
    {
        //qDebug() << Q_FUNC_INFO << ": WebSocket state changed:" << state;
    });

    QObject::connect(&_socket, &QWebSocket::textMessageReceived, this, &GoodGame::onWebSocketReceived);

    QObject::connect(&_socket, &QWebSocket::connected, this, [this]()
    {
        if (state.connected)
        {
            state.connected = false;
            emit connectedChanged(false, _lastConnectedChannelName);
        }

        requestChannelStatus();
        emit stateChanged();
    });

    QObject::connect(&_socket, &QWebSocket::disconnected, this, [this]()
    {
        if (state.connected)
        {
            state.connected = false;
            emit stateChanged();
            emit connectedChanged(false, _lastConnectedChannelName);
        }
    });

    QObject::connect(&_socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [](QAbstractSocket::SocketError error_)
    {
        qDebug() << Q_FUNC_INFO << ": WebSocket error:" << error_;
    });

    reconnect();

    timerUpdateMessages.setInterval(RequestChatInterval);
    connect(&timerUpdateMessages, &QTimer::timeout, this, [this]()
    {
        if (!state.connected)
        {
            return;
        }

        requestChannelHistory();
    });
    timerUpdateMessages.start();

    timerUpdateChannelStatus.setInterval(RequestChannelStatus);
    connect(&timerUpdateChannelStatus, &QTimer::timeout, this, [this]()
    {
        if (!state.connected)
        {
            return;
        }

        requestChannelStatus();
    });
    timerUpdateChannelStatus.start();
}

ChatService::ConnectionStateType GoodGame::getConnectionStateType() const
{
    if (state.connected)
    {
        return ChatService::ConnectionStateType::Connected;
    }
    else if (!state.streamId.isEmpty())
    {
        return ChatService::ConnectionStateType::Connecting;
    }

    return ChatService::ConnectionStateType::NotConnected;
}

QString GoodGame::getStateDescription() const
{
    switch (getConnectionStateType())
    {
    case ConnectionStateType::NotConnected:
        if (state.streamId.isEmpty())
        {
            return tr("Channel not specified");
        }

        return tr("Not connected");

    case ConnectionStateType::Connecting:
        return tr("Connecting...");

    case ConnectionStateType::Connected:
        return tr("Successfully connected!");

    }

    return "<unknown_state>";
}

void GoodGame::requestAuth()
{
    QJsonDocument document;

    QJsonObject root;
    root.insert("type", "auth");

    QJsonObject data;
    data.insert("user_id", "0");

    root.insert("data", data);
    document.setObject(root);
    sendToWebSocket(document);
}

void GoodGame::requestChannelHistory()
{
    QJsonDocument document;

    QJsonObject root;
    root.insert("type", "get_channel_history");

    QJsonObject data;
    data.insert("channel_id", QString("%1").arg(channelId));

    root.insert("data", data);
    document.setObject(root);
    sendToWebSocket(document);
}

void GoodGame::requestChannelStatus()
{
    channelId = -1;

    const QString channelName = state.streamId;

    QNetworkRequest request(QUrl("https://goodgame.ru/api/getchannelstatus?fmt=json&id=" + channelName));
    request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, AxelChat::UserAgentNetworkHeaderName);

    QNetworkReply* reply = network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, channelName]()
    {
        channelId = -1;

        uint64_t id = 0;
        bool found = false;
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QStringList idsKeys = root.keys();
        for (const QString& channelObjKey : idsKeys)
        {
            id = channelObjKey.toULongLong(&found);
            if (found)
            {
                const QJsonObject channel = root.value(channelObjKey).toObject();
                if (channel.value("key").toString().trimmed().toLower() == channelName.trimmed().toLower())
                {
                    const QString viewers = channel.value("viewers").toString();
                    bool ok = false;
                    int viewersCount = viewers.toInt(&ok);
                    if (!ok)
                    {
                        viewersCount = -1;
                    }

                    state.viewersCount = viewersCount;
                    break;
                }
                else
                {
                    found = false;
                }
            }
        }

        if (found)
        {
            channelId = id;

            state.chatUrl = QString("https://goodgame.ru/chat/%1").arg(channelId);

            emit stateChanged();

            requestChannelHistory();
        }
        else
        {
            qWarning() << Q_FUNC_INFO << ": channel id not found";
        }
    });
}

QString GoodGame::getStreamId(const QString &stream)
{
    QString streamId = stream.trimmed().toLower();
    if (streamId.contains("goodgame.ru"))
    {
        streamId = AxelChat::simplifyUrl(streamId);

        if (!AxelChat::removeFromStart(streamId, "goodgame.ru/channel/", Qt::CaseSensitivity::CaseInsensitive))
        {
            return QString();
        }

        if (streamId.contains('#'))
        {
            streamId = streamId.left(streamId.indexOf('#'));
        }

        streamId.remove('/');
    }

    return streamId;
}

void GoodGame::reconnect()
{
    _socket.close();

    state = State();

    state.streamId = getStreamId(stream.get());

    if (state.streamId.isEmpty())
    {
        emit stateChanged();
        return;
    }

    state.streamUrl = "https://goodgame.ru/channel/" + state.streamId;

    _socket.setProxy(network.proxy());
    _socket.open(QUrl("wss://chat.goodgame.ru/chat/websocket"));

    emit stateChanged();
}

void GoodGame::onParameterChanged(Parameter &parameter)
{
    Setting<QString>& setting = *parameter.getSetting();

    if (&setting == &stream)
    {
        stream.set(stream.get().trimmed());
        reconnect();
    }
}

void GoodGame::onWebSocketReceived(const QString &rawData)
{
    //qDebug(rawData.toUtf8());

    const QJsonDocument document = QJsonDocument::fromJson(rawData.toUtf8());
    const QJsonObject root = document.object();
    const QJsonObject data = root.value("data").toObject();
    const QString type = root.value("type").toString();
    const QString channelId = data.value("channel_id").toString();

    if (type == "channel_history")
    {
        if (!state.connected)
        {
            state.connected = true;
            _lastConnectedChannelName = state.streamId;
            emit connectedChanged(true, state.streamId);
            emit stateChanged();
        }

        QList<ChatMessage> messages;
        QList<ChatAuthor> authors;

        const QJsonArray jsonMessages = data.value("messages").toArray();
        for (const QJsonValue& value : qAsConst(jsonMessages))
        {
            const QJsonObject jsonMessage = value.toObject();

            const QString authorId = QString("%1").arg(jsonMessage.value("user_id").toVariant().toLongLong());
            const QString authorName = jsonMessage.value("user_name").toString();
            const QString messageId = QString("%1").arg(jsonMessage.value("message_id").toVariant().toLongLong());
            const QDateTime publishedAt = QDateTime::fromSecsSinceEpoch(jsonMessage.value("timestamp").toVariant().toLongLong());
            const QString text = jsonMessage.value("text").toString();

            const ChatAuthor author(getServiceType(),
                                    authorName,
                                    authorId,
                                    QUrl(),
                                    QUrl("https://goodgame.ru/user/" + authorId));

            const ChatMessage message({ new ChatMessage::Text(text) }, author, publishedAt, QDateTime::currentDateTime(), messageId);

            messages.append(message);
            authors.append(author);
        }

        if (!messages.isEmpty())
        {
            emit readyRead(messages, authors);
        }
    }
    else if (type == "welcome")
    {
        const double protocolVersion = data.value("protocolVersion").toDouble();
        if (protocolVersion != 1.1)
        {
            qWarning() << Q_FUNC_INFO << ": unsupported protocol version" << protocolVersion;
        }
    }
    else if (type == "error")
    {
        qWarning() << Q_FUNC_INFO << ": client received error, channel id =" << channelId << ", error num =" << data.value("error_num").toInt() << ", error text =" << data.value("errorMsg").toString();
    }
    else if (type == "success_auth")
    {
        requestChannelStatus();
    }
    else
    {
        qWarning() << Q_FUNC_INFO << ": unknown message type" << type << ", data = \n" << rawData;
    }


    if (!state.connected)
    {
        requestAuth();
    }
}

void GoodGame::sendToWebSocket(const QJsonDocument &data)
{
    _socket.sendTextMessage(QString::fromUtf8(data.toJson()));
}
