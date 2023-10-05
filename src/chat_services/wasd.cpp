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
static const int ReconnectPeriod = 3 * 1000;
static const int PingPeriod = 3 * 1000;
static const int StickerImageHeight = 128;
static const int SmileImageHeight = 32;
static const int RequestChannelInterval = 10000;

};

Wasd::Wasd(QSettings &settings, const QString &settingsGroupPathParent, QNetworkAccessManager &network_, cweqt::Manager&, QObject *parent)
    : ChatService(settings, settingsGroupPathParent, AxelChat::ServiceType::Wasd, false, parent)
    , network(network_)
    , socket("https://wasd.tv/")
{
    ui.findBySetting(stream)->setItemProperty("name", tr("Channel"));
    ui.findBySetting(stream)->setItemProperty("placeholderText", tr("Link or channel name..."));

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state)
    {
        Q_UNUSED(state)
        //qDebug() << "WebSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &Wasd::onWebSocketReceived);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        //qDebug() << "WebSocket connected";

        if (info.jwtToken.isEmpty())
        {
            qCritical() << "jwt token is empty, disconnect";
            socket.close();
            return;
        }

        if (info.streamId.isEmpty() || info.channelId.isEmpty())
        {
            qCritical() << "stream id or channel id is empty, disconnect";
            socket.close();
            return;
        }

        sendJoin(info.streamId, info.channelId, info.jwtToken);
        sendPing();
    });

    QObject::connect(&socket, &QWebSocket::disconnected, this, [this]()
    {
        //qDebug() << "WebSocket disconnected";
        setConnected(false);
    });

    QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError error_)
    {
        qCritical() << "WebSocket error:" << error_ << ":" << socket.errorString();
    });

    QObject::connect(&timerReconnect, &QTimer::timeout, this, [this]()
    {
        if (socket.state() != QAbstractSocket::SocketState::ConnectedState && !info.jwtToken.isEmpty() && !info.streamId.isEmpty() && !info.channelId.isEmpty())
        {
            socket.setProxy(network.proxy());
            socket.open(QUrl("wss://chat.wasd.tv/socket.io/?EIO=3&transport=websocket"));
        }
    });
    timerReconnect.start(ReconnectPeriod);

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
}

ChatService::ConnectionState Wasd::getConnectionState() const
{
    if (isConnected())
    {
        return ChatService::ConnectionState::Connected;
    }
    else if (enabled.get() && !state.streamId.isEmpty())
    {
        return ChatService::ConnectionState::Connecting;
    }
    
    return ChatService::ConnectionState::NotConnected;
}

QString Wasd::getStateDescription() const
{
    switch (getConnectionState())
    {
    case ConnectionState::NotConnected:
        if (stream.get().isEmpty())
        {
            return tr("Channel not specified");
        }

        if (state.streamId.isEmpty())
        {
            return tr("The channel is not correct");
        }

        return tr("Not connected");
        
    case ConnectionState::Connecting:
        return tr("Connecting...");
        
    case ConnectionState::Connected:
        return tr("Successfully connected!");

    }

    return "<unknown_state>";
}

void Wasd::reconnectImpl()
{
    socket.close();

    info = Info();

    state.controlPanelUrl = QUrl(QString("https://wasd.tv/stream-settings"));

    state.streamId = extractChannelName(stream.get());

    if (state.streamId.isEmpty())
    {
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
    //qDebug() << "received:" << rawData;

    if (!enabled.get())
    {
        return;
    }

    if (rawData.isEmpty())
    {
        qCritical() << "received data is empty";
        return;
    }

    bool ok = false;
    const SocketIO2Type type = (SocketIO2Type)QString(rawData[0]).toInt(&ok);
    if (!ok)
    {
        qCritical() << "failed to get type";
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
        qCritical() << "json parse error =" << jsonError.errorString() << ", offset =" << jsonError.offset << ", raw data =" << rawData;
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
            qCritical() << "failed to get jwt token";
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
            const QJsonObject channel = result.value("channel").toObject();
            const bool isLive = channel.value("channel_is_live").toBool();
            if (!isLive)
            {
                qWarning() << "channel" << channelName << "is not live";

                setConnected(false);

                socket.close();
                info.channelId = QString();
                info.streamId = QString();

                return;
            }

            const QJsonObject mediaContainer = result.value("media_container").toObject();

            info.channelId = QString("%1").arg(mediaContainer.value("channel_id").toVariant().toLongLong());

            const QJsonArray streams = mediaContainer.value("media_container_streams").toArray();
            if (streams.isEmpty())
            {
                qCritical() << "streams is empty";
            }
            else
            {
                if (streams.count() != 1)
                {
                    qCritical() << "streams count is not 1";
                }

                const QJsonObject stream = streams.first().toObject();
                
                setViewers(stream.value("stream_current_viewers").toInt(-1));
                emit stateChanged();

                info.streamId = QString("%1").arg(stream.value("stream_id").toVariant().toLongLong());
            }

            if (info.channelId.isEmpty() || info.streamId.isEmpty())
            {
                qCritical() << "channel id or stream id is empty";
            }
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

    //qDebug() << "send:" << message;

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
                qCritical() << "array size not 2";
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

    qWarning() << "unknown message type" << (int)type;
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
        qWarning() << "unknown event type" << type;
    }
}

void Wasd::parseEventMessage(const QJsonObject &data)
{
    // https://github.com/shevernitskiy/wasdtv/blob/main/src/types/api.ts

    // TODO: 'MESSAGE' | 'EVENT' | 'STICKER' | 'GIFTS' | 'YADONAT' | 'SUBSCRIBE' | 'HIGHLIGHTED_MESSAGE'

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
    else if (role == "CHANNEL_USER")
    {
        //
    }
    else
    {
        qWarning() << "unknown role" << role << "of author" << name;
    }

    const QJsonObject jsonUserAvatar = data.value("user_avatar").toObject();
    QUrl avatarUrl = jsonUserAvatar.value("large").toString();
    if (avatarUrl.isEmpty())
    {
        const QStringList keys = jsonUserAvatar.keys();
        if (keys.isEmpty())
        {
            qWarning() << "user avatar sizes is empty";
        }
        else
        {
            avatarUrl = jsonUserAvatar.value(keys.first()).toString();
        }
    }

    std::shared_ptr<Author> author = std::make_shared<Author>(
        getServiceType(),
        name,
        authorId,
        avatarUrl,
        pageUrl,
        leftBadges,
        QStringList(),
        std::set<Author::Flag>(),
        authorNicknameColor,
        authorNicknameBackgroundColor);

    const QDateTime publishedAt = QDateTime::fromString(data.value("date_time").toString(), Qt::DateFormat::ISODateWithMs);
    const QString messageId = data.value("id").toString();

    QList<std::shared_ptr<Message::Content>> contents;

    if (const QJsonValue v = data.value("message"); v.isString())
    {
        const QString text = v.toString();
        const QList<std::shared_ptr<Message::Content>> newContents = parseText(text);
        for (const std::shared_ptr<Message::Content>& content : qAsConst(newContents))
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
                qWarning() << "sticker image sizes is empty";
            }
            else
            {
                url = jsonStickerImage.value(keys.first()).toString();
            }
        }

        contents.append(std::make_shared<Message::Image>(url, StickerImageHeight));
    }

    if (contents.isEmpty())
    {
        qWarning() << "contents is empty, maybe message structure is unknown";
        return;
    }

    std::shared_ptr<Message> message = std::make_shared<Message>(
        contents,
        author,
        publishedAt,
        QDateTime::currentDateTime(),
        getServiceTypeId(getServiceType()) + QString("/%1").arg(messageId));

    QList<std::shared_ptr<Message>> messages = { message };
    QList<std::shared_ptr<Author>> authors = { author };

    emit readyRead(messages, authors);
}

void Wasd::parseEventJoined(const QJsonObject &)
{
    if (!isConnected() && !state.streamId.isEmpty())
    {
        setConnected(true);
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
        qCritical() << "id not equal token, id =" << id << ", token =" << token << ", url =" << url;
    }

    smiles.insert(token, url);
}

QList<std::shared_ptr<Message::Content>> Wasd::parseText(const QString &rawText) const
{
    QList<std::shared_ptr<Message::Content>> contents;

    const QStringList words = rawText.split(' ', Qt::SplitBehaviorFlags::KeepEmptyParts);

    QString text;
    for (const QString& word : words)
    {
        if (smiles.contains(word))
        {
            if (!text.isEmpty())
            {
                contents.append(std::make_shared<Message::Text>(text));
                text = QString();
            }

            contents.append(std::make_shared<Message::Image>(smiles.value(word), SmileImageHeight));
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
        contents.append(std::make_shared<Message::Text>(text));
        text = QString();
    }

    return contents;
}

QString Wasd::extractChannelName(const QString &stream)
{
    QString channelName = stream.toLower().trimmed();

    const QString simpleUrl = AxelChat::simplifyUrl(stream);

    if (simpleUrl.startsWith("wasd.tv/", Qt::CaseSensitivity::CaseInsensitive))
    {
        const QUrlQuery query = QUrlQuery(QUrl(stream).query());
        const QString queryChannelName = query.queryItemValue("channel_name");
        if (!queryChannelName.isEmpty())
        {
            return queryChannelName;
        }

        channelName = simpleUrl.mid(8);

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
