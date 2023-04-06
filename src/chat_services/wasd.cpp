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
static const int StickerImageHeight = 128;
static const int SmileImageHeight = 32;
static const int RequestChannelInterval = 10000;

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

    QObject::connect(&timerRequestChannel, &QTimer::timeout, this, [this]()
    {
        if (!enabled.get() || state.streamId.isEmpty())
        {
            return;
        }

        requestChannel(state.streamId);
    });
    timerRequestChannel.start(RequestChannelInterval);

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

    requestSmiles();

    state.chatUrl = QUrl(QString("https://wasd.tv/chat?channel_name=%1").arg(state.streamId));
    state.streamUrl = QUrl(QString("https://wasd.tv/%1").arg(state.streamId));

    if (enabled.get())
    {
        requestTokenJWT();
        requestChannel(state.streamId);
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

    parseSocketMessage(type, doc);
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

void Wasd::requestChannel(const QString &channelName)
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

void Wasd::requestSmiles()
{
    QNetworkRequest request(QUrl("https://static.wasd.tv/settings/smiles.json"));

    QNetworkReply* reply = network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const QByteArray data = reply->readAll();
        reply->deleteLater();

        const QJsonObject root = QJsonDocument::fromJson(data).object();
        const QJsonArray result = root.value("result").toArray();
        for (const QJsonValue& v : qAsConst(result))
        {
            const QJsonArray jsonSmiles = v.toObject().value("smiles").toArray();
            for (const QJsonValue& v : qAsConst(jsonSmiles))
            {
                parseSmile(v.toObject());
            }
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
    const bool excludeStickers = true;

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

void Wasd::parseSocketMessage(const SocketIO2Type type, const QJsonDocument &doc)
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
    if (type == "message" || type == "sticker")
    {
        parseEventMessage(data);
    }
    else if (type == "system_message")
    {
        qInfo() << "WASD system message:" << data.value("message").toString();
    }
    else if (type == "joined")
    {
        parseEventJoined(data);
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "unknown event type" << type;
    }
}

void Wasd::parseEventMessage(const QJsonObject &data)
{
    // https://github.com/shevernitskiy/wasdtv/blob/main/src/types/api.ts

    // TODO: 'MESSAGE' | 'EVENT' | 'STICKER' | 'GIFTS' | 'YADONAT' | 'SUBSCRIBE' | 'HIGHLIGHTED_MESSAGE'

    /* TODO: ChatAction
    | 'WRITE_TO_CHAT'
    | 'WRITE_TO_FOLLOWERS_CHAT'
    | 'WRITE_TO_SUBSCRIPTION_CHAT'
    | 'WRITE_NO_DELAY'
    | 'CHANNEL_USER'
    | 'CHAT_START_VOTING'
    | 'CHAT_MAKE_VOTING_CHOICE'
    | 'ASSIGN_MODERATOR'
    | 'BAN_USER'
    | 'DELETE_MESSAGE'
    | 'MUTE_USER'
    | 'REMOVE_MESSAGES'
    | 'VIEW_BANNED_USERS'
     */

    /* TODO: Role
    | 'CHANNEL_FOLLOWER'
    | 'CHANNEL_USER'
    | 'CHANEL_SUBSCRIBER'
    | 'CHANNEL_BANNED'
    | 'CHANNEL_MUTE'
    | 'CHANNEL_HIDE'
    | 'PROMO_CODE_WINNER'
    | 'PROMO_CODE_CANDIDATE'
    | 'WASD_ADMIN'
    | 'WASD_TEAM'
    | 'WASD_PARTNER'
    | 'ANON'
    */

    const QString name = data.value("user_login").toString();
    const QString authorId = QString("%1").arg(data.value("user_id").toVariant().toLongLong());
    const QUrl pageUrl = "https://wasd.tv/" + name.trimmed();

    QColor authorNicknameColor;
    QColor authorNicknameBackgroundColor;
    QStringList leftBadges;

    const QString role = data.value("user_channel_role").toString();
    if (role == "CHANNEL_OWNER")
    {
        leftBadges.append("qrc:/resources/images/king.svg");
        authorNicknameColor = QColor(255, 255, 255);
        authorNicknameBackgroundColor = QColor(245, 166, 35);
    }
    else if (role == "CHANNEL_MODERATOR")
    {
        leftBadges.append("qrc:/resources/images/youtube-moderator-icon.svg");
        authorNicknameColor = QColor(255, 255, 255);
        authorNicknameBackgroundColor = QColor(68, 72, 88);
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "unknown role" << role << "of author" << name;
    }

    const QJsonObject jsonUserAvatar = data.value("user_avatar").toObject();
    QUrl avatarUrl = jsonUserAvatar.value("large").toString();
    if (avatarUrl.isEmpty())
    {
        const QStringList keys = jsonUserAvatar.keys();
        if (keys.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "user avatar sizes is empty";
        }
        else
        {
            avatarUrl = jsonUserAvatar.value(keys.first()).toString();
        }
    }

    const Author author(getServiceType(),
                        name,
                        authorId,
                        avatarUrl,
                        pageUrl,
                        leftBadges,
                        {},
                        {},
                        authorNicknameColor,
                        authorNicknameBackgroundColor);

    const QDateTime publishedAt = QDateTime::fromString(data.value("date_time").toString(), Qt::DateFormat::ISODateWithMs);
    const QString messageId = data.value("id").toString();

    QList<Message::Content*> contents;

    if (const QJsonValue v = data.value("message"); v.isString())
    {
        const QString text = v.toString();
        const QList<Message::Content*> newContents = parseText(text);
        for (Message::Content* content : newContents)
        {
            contents.append(content);
        }
    }

    if (const QJsonValue v = data.value("sticker"); v.isObject())
    {
        const QJsonObject jsonStickerImage = v.toObject().value("sticker_image").toObject();
        QUrl url = jsonStickerImage.value("medium").toString();
        if (url.isEmpty())
        {
            const QStringList keys = jsonStickerImage.keys();
            if (keys.isEmpty())
            {
                qWarning() << Q_FUNC_INFO << "sticker image sizes is empty";
            }
            else
            {
                url = jsonStickerImage.value(keys.first()).toString();
            }
        }

        contents.append(new Message::Image(url, StickerImageHeight));
    }

    if (contents.isEmpty())
    {
        qWarning() << Q_FUNC_INFO << "contents is empty, maybe message structure is unknown";
        return;
    }

    const Message message(contents, author, publishedAt, QDateTime::currentDateTime(), getServiceTypeId(serviceType) + QString("/%1").arg(messageId));

    QList<Message> messages = { message };
    QList<Author> authors = { author };

    emit readyRead(messages, authors);
}

void Wasd::parseEventJoined(const QJsonObject &)
{
    if (!state.connected && !state.streamId.isEmpty())
    {
        state.connected = true;

        emit connectedChanged(true, state.streamId);
        emit stateChanged();
    }
}

void Wasd::parseSmile(const QJsonObject &jsonSmile)
{
    const QString id = jsonSmile.value("id").toString();
    const QString token = jsonSmile.value("token").toString();
    const QUrl url = jsonSmile.value("image_url").toString();

    if (id != token)
    {
        // TODO
        qWarning() << Q_FUNC_INFO << "id not equal token, id =" << id << ", token =" << token << ", url =" << url;
    }

    smiles.insert(token, url);
}

QList<Message::Content *> Wasd::parseText(const QString &rawText) const
{
    QList<Message::Content *> contents;

    const QStringList words = rawText.split(' ', Qt::SplitBehaviorFlags::KeepEmptyParts);

    QString text;
    for (const QString& word : words)
    {
        if (smiles.contains(word))
        {
            if (!text.isEmpty())
            {
                contents.append(new Message::Text(text));
                text = QString();
            }

            contents.append(new Message::Image(smiles.value(word), SmileImageHeight));
        }
        else
        {
            if (!text.isEmpty())
            {
                text += ' ';
            }

            text += word;
        }
    }

    if (!text.isEmpty())
    {
        contents.append(new Message::Text(text));
        text = QString();
    }

    return contents;
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
