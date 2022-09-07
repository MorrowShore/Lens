#include "goodgame.h"
#include "models/chatmessagesmodle.hpp"
#include "models/chatmessage.h"
#include "models/chatauthor.h"
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>

GoodGame::GoodGame(QSettings& settings_, const QString& settingsGroupPath, QNetworkAccessManager& network_, QObject *parent)
    : ChatService(ChatService::ServiceType::GoodGame, parent)
    , settings(settings_)
    , SettingsGroupPath(settingsGroupPath)
    , network(network_)
{
    QObject::connect(&_socket, &QWebSocket::stateChanged, this, [=](QAbstractSocket::SocketState state){
        //qDebug() << "GoodGame WebSocket state changed:" << state;
    });

    QObject::connect(&_socket, &QWebSocket::textMessageReceived, this, &GoodGame::onWebSocketReceived);

    QObject::connect(&_socket, &QWebSocket::connected, this, [=]() {
        if (_info.connected)
        {
            _info.connected = false;
            emit stateChanged();
            emit connected(_lastConnectedChannelName);
        }
    });

    QObject::connect(&_socket, &QWebSocket::disconnected, this, [=](){
        qDebug() << "GoodGame disconnected";

        if (_info.connected)
        {
            _info.connected = false;
            emit stateChanged();
            emit disconnected(_lastConnectedChannelName);
        }
    });

    QObject::connect(&_socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [=](QAbstractSocket::SocketError error_){
        qDebug() << "GoodGame WebSocket error:" << error_;
    });

    reconnect();

    QObject::connect(&_timerReconnect, &QTimer::timeout, this, &GoodGame::timeoutReconnect);
    _timerReconnect.start(2000);
}

GoodGame::~GoodGame()
{
    _socket.close();
}

ChatService::ConnectionStateType GoodGame::getConnectionStateType() const
{
    return ChatService::ConnectionStateType::NotConnected;
}

QString GoodGame::getStateDescription() const
{
    return "unknown";
}

int GoodGame::getViewersCount() const
{
    //ToDo:
    return 0;
}

void GoodGame::timeoutReconnect()
{
    if (!_info.connected /*&& !_info.oauthToken.isEmpty()*/)
    {
        reconnect();
    }
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

void GoodGame::requestGetChannelHistory()
{
    QJsonDocument document;

    QJsonObject root;
    root.insert("type", "get_channel_history");

    QJsonObject data;
    data.insert("channel_id", QString("%1").arg(_info.channelId));

    root.insert("data", data);
    document.setObject(root);
    sendToWebSocket(document);
}

void GoodGame::requestChannelId()
{
    const QString channelName = _info.channelName;

    QNetworkRequest request(QUrl("https://goodgame.ru/api/getchannelstatus?fmt=json&id=" + channelName));
    request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, AxelChat::UserAgentNetworkHeaderName);

    QNetworkReply* reply = network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, channelName]()
    {
        uint64_t id = 0;
        bool found = false;
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        for (const QString& key : root.keys())
        {
            id = key.toULongLong(&found);
            if (found)
            {
                const QJsonObject channel = root.value(key).toObject();
                if (channel.value("key").toString().trimmed().toLower() == channelName.trimmed().toLower())
                {
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
            _info.channelId = id;
            requestGetChannelHistory();
        }
    });
}

void GoodGame::reconnect()
{
    _socket.close();

    //https://goodgame.ru/chat/26624

    _socket.setProxy(network.proxy());

    _socket.open(QUrl("wss://chat.goodgame.ru/chat/websocket"));
}

void GoodGame::setBroadcastLink(const QString &link)
{
    // https://goodgame.ru/api/getchannelstatus?id=lepestqchek&fmt=json

    const QString channelName = link.toLower(); // TODO
    _info.channelName = channelName;
}

QString GoodGame::getBroadcastLink() const
{
    return QString(); // TODO
}

void GoodGame::onWebSocketReceived(const QString &rawData)
{
    qDebug(rawData.toUtf8());

    const QJsonDocument document = QJsonDocument::fromJson(rawData.toUtf8());
    const QJsonObject root = document.object();
    const QJsonObject data = root.value("data").toObject();
    const QString type = root.value("type").toString();
    const QString channelId = data.value("channel_id").toString();

    if (type == "channel_history")
    {
        if (!_info.connected)
        {
            _info.connected = true;
            emit connected(_info.channelName);
        }

        QList<ChatMessage> messages;
        QList<ChatAuthor> authors;

        const QJsonArray jsonMessages = data.value("messages").toArray();
        for (const QJsonValue& value : qAsConst(jsonMessages))
        {
            const QJsonObject jsonMessage = value.toObject();

            QList<ChatMessage::Content*> contents;

            const QString userId = jsonMessage.value("user_id").toString();
            const QString userName = jsonMessage.value("user_name").toString();
            const int userGroup = jsonMessage.value("user_group").toInt();
            const QString messageId = jsonMessage.value("message_id").toString();
            const QDateTime publishedAt = QDateTime::fromMSecsSinceEpoch(qint64(jsonMessage.value("timestamp").toDouble()));
            const QString text = jsonMessage.value("text").toString();

            contents.append(new ChatMessage::Text(text));

            const ChatAuthor author(ChatService::ServiceType::GoodGame,
                                    userName,
                                    userId);

            const ChatMessage message(contents,
                                      userId,
                                      publishedAt);

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
        _info.protocolVersion = data.value("protocolVersion").toDouble();
        if (_info.protocolVersion != 1.1)
        {
            qWarning() << "GoodGame: unsupported protocol version" << _info.protocolVersion;
        }
    }
    else if (type == "error")
    {
        qWarning() << "GoodGame: client received error, channel id =" << channelId << ", error num =" << data.value("error_num").toInt() << ", error text =" << data.value("errorMsg").toString();
    }
    else if (type == "success_auth")
    {
        requestChannelId();

        /*_info.channelName = "<123>";
        _info.connected = true;
        _lastConnectedChannelName = _info.channelId;
        emit connected(_info.channelName);
        emit stateChanged();

        requestGetChannelHistory();

        qDebug() << "GoodGame connected" << _info.channelId;*/
    }
    else
    {
        qDebug() << "GoodGame: unknown message type" << type << ", data = \n" << rawData;
    }


    if (!_info.connected)
    {
        requestAuth();
    }
}

void GoodGame::sendToWebSocket(const QJsonDocument &data)
{
    _socket.sendTextMessage(QString::fromUtf8(data.toJson()));
}
