#include "wasd.h"
#include "secrets.h"
#include "crypto/obfuscator.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonArray>

namespace
{

static const QString ApiToken = OBFUSCATE(WASD_API_TOKEN);
static const int ReconncectPeriod = 3 * 1000;
static const int PingPeriod = 3 * 1000;

};

Wasd::Wasd(QSettings &settings, const QString &settingsGroupPath, QNetworkAccessManager &network_, QObject *parent)
    : ChatService(settings, settingsGroupPath, AxelChat::ServiceType::Wasd, parent)
    , network(network_)
    , socket("https://wasd.tv/")
{
    getUIElementBridgeBySetting(stream)->setItemProperty("name", tr("Channel"));
    getUIElementBridgeBySetting(stream)->setItemProperty("placeholderText", tr("Link or channel name..."));

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state)
    {
        Q_UNUSED(state)
        //qDebug() << Q_FUNC_INFO << ": WebSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &Wasd::onWebSocketReceived);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        qDebug() << Q_FUNC_INFO << ": WebSocket connected";

        if (info.jwtToken.isEmpty())
        {
            qDebug() << Q_FUNC_INFO << "jwt token is empty, disconnect";
            socket.close();
            return;
        }

        if (info.streamId.isEmpty() || info.channelId.isEmpty())
        {
            qDebug() << Q_FUNC_INFO << "stream id or channel id is empty, disconnect";
            socket.close();
            return;
        }

        sendJoin(info.streamId, info.channelId, info.jwtToken);
        sendPing();

        emit stateChanged();
    });

    QObject::connect(&socket, &QWebSocket::disconnected, this, [this]()
    {
        qDebug() << Q_FUNC_INFO << ": WebSocket disconnected";

        if (state.connected)
        {
            state.connected = false;
            emit stateChanged();
            emit connectedChanged(false, QString());
        }
    });

    QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError error_)
    {
        qDebug() << Q_FUNC_INFO << ": WebSocket error:" << error_ << ":" << socket.errorString();
    });

    QObject::connect(&timerReconnect, &QTimer::timeout, this, [this]()
    {
        if (socket.state() == QAbstractSocket::SocketState::UnconnectedState && !info.jwtToken.isEmpty())
        {
            socket.setProxy(network.proxy());
            socket.open(QUrl("wss://chat.wasd.tv/socket.io/?EIO=3&transport=websocket"));
        }
    });
    timerReconnect.start(ReconncectPeriod);

    QObject::connect(&timerReconnect, &QTimer::timeout, this, [this]()
    {
        if (socket.state() == QAbstractSocket::SocketState::UnconnectedState && !info.jwtToken.isEmpty())
        {
            socket.setProxy(network.proxy());
            socket.open(QUrl("wss://chat.wasd.tv/socket.io/?EIO=3&transport=websocket"));
        }
    });
    timerReconnect.start(ReconncectPeriod);


    QObject::connect(&timerPing, &QTimer::timeout, this, [this]()
    {
        if (socket.state() == QAbstractSocket::SocketState::ConnectedState)
        {
            sendPing();
        }
    });
    timerPing.start(PingPeriod);

    reconnect();
}

ChatService::ConnectionStateType Wasd::getConnectionStateType() const
{
    if (state.connected)
    {
        return ChatService::ConnectionStateType::Connected;
    }
    else if (enabled.get() && !state.streamId.isEmpty())
    {
        return ChatService::ConnectionStateType::Connecting;
    }

    return ChatService::ConnectionStateType::NotConnected;
}

QString Wasd::getStateDescription() const
{
    switch (getConnectionStateType())
    {
    case ConnectionStateType::NotConnected:
        if (stream.get().isEmpty())
        {
            return tr("Channel not specified");
        }

        if (state.streamId.isEmpty())
        {
            return tr("The channel is not correct");
        }

        return tr("Not connected");

    case ConnectionStateType::Connecting:
        return tr("Connecting...");

    case ConnectionStateType::Connected:
        return tr("Successfully connected!");

    }

    return "<unknown_state>";
}

void Wasd::reconnectImpl()
{
    socket.close();

    state = State();
    info = Info();

    state.controlPanelUrl = QUrl(QString("https://wasd.tv/stream-settings"));

    state.streamId = extractChannelName(stream.get());

    if (state.streamId.isEmpty())
    {
        emit stateChanged();
        return;
    }

    state.chatUrl = QUrl(QString("https://wasd.tv/chat?channel_name=%1").arg(state.streamId));
    state.streamUrl = QUrl(QString("https://wasd.tv/%1").arg(state.streamId));

    if (enabled.get())
    {
        requestTokenJWT();
        requestChannelInfo(state.streamId);
    }
}

void Wasd::onWebSocketReceived(const QString &rawData)
{
    qDebug("\nreceived: " + rawData.toUtf8() + "\n");

    if (!enabled.get())
    {
        return;
    }

    if (rawData.isEmpty())
    {
        qWarning() << Q_FUNC_INFO << "received data is empty";
        return;
    }

    bool ok = false;
    const SocketIO2Type type = (SocketIO2Type)QString(rawData[0]).toInt(&ok);
    if (!ok)
    {
        qWarning() << Q_FUNC_INFO << "failed to get type";
        return;
    }

    QByteArray payload;

    for (int i = 1; i < rawData.length(); i++)
    {
        if (!rawData[i].isNumber())
        {
            payload = rawData.mid(i).toUtf8();
            break;
        }
    }

    QJsonParseError jsonError;
    const QJsonDocument doc = payload.isEmpty() ? QJsonDocument() : QJsonDocument::fromJson(payload, &jsonError);
    if (jsonError.error != QJsonParseError::ParseError::NoError)
    {
        qWarning() << Q_FUNC_INFO << "json parse error =" << jsonError.errorString() << ", offset =" << jsonError.offset << payload;
    }

    parseMessage(type, doc);
}

void Wasd::requestTokenJWT()
{
    QNetworkRequest request(QUrl("https://wasd.tv/api/auth/chat-token"));
    request.setRawHeader("Authorization", QString("Token %1").arg(ApiToken).toUtf8());

    QNetworkReply* reply = network.post(request, QByteArray());
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const QByteArray data = reply->readAll();
        reply->deleteLater();

        const QJsonObject root = QJsonDocument::fromJson(data).object();

        info.jwtToken = root.value("result").toString();

        if (info.jwtToken.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "failed to get jwt token";
        }
    });
}

void Wasd::requestChannelInfo(const QString &channelName)
{
    QNetworkRequest request(QUrl("https://wasd.tv/api/v2/broadcasts/public?channel_name=" + channelName));

    QNetworkReply* reply = network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, channelName]()
    {
        const QByteArray data = reply->readAll();
        reply->deleteLater();

        const QJsonObject root = QJsonDocument::fromJson(data).object();
        const QJsonObject result = root.value("result").toObject();

        if (channelName.toLower().trimmed() == state.streamId.toLower().trimmed())
        {
            const QJsonObject mediaContainer = result.value("media_container").toObject();

            info.channelId = QString("%1").arg(mediaContainer.value("channel_id").toVariant().toLongLong());

            const QJsonArray streams = mediaContainer.value("media_container_streams").toArray();
            if (streams.isEmpty())
            {
                qWarning() << Q_FUNC_INFO << "streams is empty";
            }
            else
            {
                if (streams.count() != 1)
                {
                    qWarning() << Q_FUNC_INFO << "streams count is not 1";
                }

                const QJsonObject stream = streams.first().toObject();

                state.viewersCount = stream.value("stream_current_viewers").toInt();

                info.streamId = QString("%1").arg(stream.value("stream_id").toVariant().toLongLong());
            }

            if (info.channelId.isEmpty() || info.streamId.isEmpty())
            {
                qWarning() << Q_FUNC_INFO << "channel id or stream id is empty";
            }

            emit stateChanged();
        }
    });
}

void Wasd::send(const SocketIO2Type type, const QByteArray& id, const QJsonDocument& payload)
{
    const int typeNum = (int)type;

    QString message = QString("%1").arg(typeNum) + id + QString::fromUtf8(payload.toJson(QJsonDocument::JsonFormat::Compact));

    qDebug("\nsend: " + message.toUtf8() + "\n");

    socket.sendTextMessage(message);
}

void Wasd::sendJoin(const QString &streamId, const QString &channelId, const QString &jwt)
{
    const bool excludeStickers = true; // TODO

    QJsonArray array =
    {
        "join",
        QJsonObject(
        {
            { "streamId", streamId },
            { "channelId", channelId },
            { "jwt", jwt },
            { "excludeStickers", excludeStickers }
        })
    };

    send(SocketIO2Type::CONNECT_ERROR, "2", QJsonDocument(array));
}

void Wasd::sendPing()
{
    send(SocketIO2Type::EVENT);
}

void Wasd::parseMessage(const SocketIO2Type type, const QJsonDocument &doc)
{
    switch(type)
    {
    case SocketIO2Type::CONNECT:
        return;

    case SocketIO2Type::DISCONNECT:
        break;

    case SocketIO2Type::EVENT:
        break;

    case SocketIO2Type::ACK:
        // TODO
        return;

    case SocketIO2Type::CONNECT_ERROR:
    {
        const QJsonArray array = doc.array();
        if (array.isEmpty())
        {
            return;
        }

        const QString eventType = array.first().toString();

        if (array.count() >= 2)
        {
            if (array.count() != 2)
            {
                qWarning() << Q_FUNC_INFO << "array size not 2";
            }

            parseEvent(eventType, array[1].toObject());
        }
        else
        {
            parseEvent(eventType, QJsonObject());
        }
    }
        return;

    case SocketIO2Type::BINARY_EVENT:
        break;

    case SocketIO2Type::BINARY_ACK:
        break;
    }

    qWarning() << Q_FUNC_INFO << "unknown message type" << (int)type;
}

void Wasd::parseEvent(const QString &type, const QJsonObject &data)
{
    if (type == "message")
    {
        // {"date_time":"2023-04-04T20:39:09.091Z","channel_id":416355,"stream_id":1380773,"streamer_id":435858,"user_id":396621,"user_avatar":{"large":"https://st.wasd.tv/upload/avatars/a718dab1-01d5-4320-b8b2-788c226f7060/original.jpeg","small":"https://st.wasd.tv/upload/avatars/a718dab1-01d5-4320-b8b2-788c226f7060/original.jpeg","medium":"https://st.wasd.tv/upload/avatars/a718dab1-01d5-4320-b8b2-788c226f7060/original.jpeg"},"is_follower":true,"user_login":"Misvander","user_channel_role":"CHANNEL_USER","other_roles":["CHANNEL_FOLLOWER","CHANNEL_SUBSCRIBER"],"message":"разойдись свинопасы","id":"945f01cc-38b8-4bd1-8681-7b59d10df7da","hash":"6fca37c6-3f35-41d1-abf5-e6c3b459951e","meta":{"days_as_sub":426}}


    }
    else if (type == "sticker")
    {
        // {"id":"735ed617-05e4-4340-b812-3520c834e625","date_time":"2023-04-04T20:39:09.673Z","hash":"c61b9974-0041-479d-bece-357bb087a082","channel_id":416355,"stream_id":1380773,"streamer_id":435858,"user_id":396598,"user_avatar":{"large":"https://st.wasd.tv/upload/avatars/9def8a16-269d-4fd0-a58d-12a82a2fe07a/original.jpeg","small":"https://st.wasd.tv/upload/avatars/9def8a16-269d-4fd0-a58d-12a82a2fe07a/original.jpeg","medium":"https://st.wasd.tv/upload/avatars/9def8a16-269d-4fd0-a58d-12a82a2fe07a/original.jpeg"},"is_follower":true,"user_login":"RedArcher","user_channel_role":"CHANNEL_USER","other_roles":["CHANNEL_FOLLOWER"],"sticker":{"sticker_id":10085,"created_at":"","updated_at":null,"deleted_at":null,"sticker_pack_id":1285,"sticker_image":{"large":"https://st.wasd.tv/upload/stickers/3c844640-ac48-483e-aeb2-9b065ffbdbd5/original.png","small":"https://st.wasd.tv/upload/stickers/3c844640-ac48-483e-aeb2-9b065ffbdbd5/64x64.png","medium":"https://st.wasd.tv/upload/stickers/3c844640-ac48-483e-aeb2-9b065ffbdbd5/128x128.png"},"sticker_name":"Archer_02","sticker_alias":"RedArcher-archer_02","sticker_status":null}}
    }
    else if (type == "system_message")
    {
        qInfo() << "WASD system message:" << data.value("message").toString();
    }
    else if (type == "joined")
    {
        if (!state.connected && !state.streamId.isEmpty())
        {
            state.connected = true;

            emit connectedChanged(true, state.streamId);
            emit stateChanged();
        }
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "unknown event type" << type;
    }
}

QString Wasd::extractChannelName(const QString &stream)
{
    QString channelName = stream.toLower().trimmed();

    if (channelName.startsWith("https://wasd.tv/", Qt::CaseSensitivity::CaseInsensitive))
    {
        channelName = channelName.mid(16);

        if (channelName.contains('?'))
        {
            channelName = channelName.left(channelName.indexOf('?'));
        }
    }

    QRegExp rx("^[a-zA-Z0-9_\\-]+$", Qt::CaseInsensitive);
    if (rx.indexIn(channelName) != -1)
    {
        return channelName;
    }

    return QString();
}
