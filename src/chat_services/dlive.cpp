#include "dlive.h"
#include "secrets.h"
#include "crypto/obfuscator.h"
#include <QNetworkRequest>
#include <QNetworkReply>

namespace
{

static const int UpdateStreamInfoPeriod = 10 * 1000;
static const int ReconncectPeriod = 6 * 1000;
static const int EmoteHeight = 28;
static const int StickerHeight = 80;
static const QColor GiftColor = QColor(107, 214, 214);

static bool checkReply(QNetworkReply *reply, const char *tag, QByteArray &resultData)
{
    resultData.clear();

    if (!reply)
    {
        qWarning() << tag << ": !reply";
        return false;
    }

    int statusCode = 200;
    const QVariant rawStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    resultData = reply->readAll();
    reply->deleteLater();

    if (rawStatusCode.isValid())
    {
        statusCode = rawStatusCode.toInt();

        if (statusCode != 200)
        {
            qWarning() << tag << ": status code:" << statusCode;
        }
    }

    if (resultData.isEmpty() && statusCode != 200)
    {
        qWarning() << tag << ": data is empty";
        return false;
    }

    return true;
}

static QDateTime convertTime(const QString& text)
{
    QString raw = text;

    raw = raw.left(raw.length() - 6); // nanoseconds to milliseconds

    bool ok = false;
    const qint64 sinceEpoch = raw.toLongLong(&ok);
    if (ok)
    {
        return QDateTime::fromMSecsSinceEpoch(sinceEpoch);
    }

    return QDateTime();
}

}

DLive::DLive(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network_, cweqt::Manager&, QObject *parent)
    : ChatService(settings, settingsGroupPathParent, AxelChat::ServiceType::DLive, false, parent)
    , network(network_)
    , socket("https://dlive.tv")
{
    ui.findBySetting(stream)->setItemProperty("name", tr("Channel"));
    ui.findBySetting(stream)->setItemProperty("placeholderText", tr("Link or channel name..."));

    // wss://graphigostream.prd.dlive.tv/

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state)
    {
        Q_UNUSED(state)
        //qDebug() << "WebSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &DLive::onWebSocketReceived);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        //qDebug() << "WebSocket connected";

        send("connection_init");
        sendStart();
    });

    QObject::connect(&socket, &QWebSocket::disconnected, this, [this]()
    {
        //qDebug() << "WebSocket disconnected";

        if (state.connected)
        {
            state.connected = false;
            emit connectedChanged(false);
            emit stateChanged();
        }

        state.viewersCount = -1;
    });

    QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError error_)
    {
        qDebug() << "WebSocket error:" << error_ << ":" << socket.errorString();
    });

    QObject::connect(&timerUpdaetStreamInfo, &QTimer::timeout, this, [this]()
    {
        if (!enabled.get() || info.userName.isEmpty())
        {
            return;
        }

        requestLiveStream(state.streamId);
    });
    timerUpdaetStreamInfo.start(UpdateStreamInfoPeriod);

    QObject::connect(&timerReconnect, &QTimer::timeout, this, [this]()
    {
        if (!enabled.get())
        {
            return;
        }

        if (!state.connected)
        {
            reconnect();
        }
    });
    timerReconnect.start(ReconncectPeriod);

    reconnect();
}

ChatService::ConnectionStateType DLive::getConnectionStateType() const
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

QString DLive::getStateDescription() const
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

void DLive::reconnectImpl()
{
    state = State();
    info = Info();

    socket.close();

    state.controlPanelUrl = "https://dlive.tv/s/dashboard#0";

    state.streamId = extractChannelName(stream.get());

    if (state.streamId.isEmpty())
    {
        return;
    }

    state.streamUrl = "https://dlive.tv/" + state.streamId;

    if (!enabled.get())
    {
        return;
    }

    timerReconnect.start();

    requestChatRoom(state.streamId);
    requestLiveStream(state.streamId);
}

void DLive::send(const QString &type, const QJsonObject &payload, const int64_t id)
{
    QJsonObject object({
        { "type", type },
        { "payload", payload },
    });

    if (id != -1)
    {
        object.insert("id", QString("%1").arg(id));
    }

    //qDebug() << "send:" << object;

    socket.sendTextMessage(QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::JsonFormat::Compact)));
}

void DLive::sendStart()
{
    static const QString Query =
        "subscription StreamMessageSubscription($streamer: String!) {\n"
        "  streamMessageReceived(streamer: $streamer) {\n"
        "    type\n"
        "    ... on ChatGift {\n"
        "      id\n"
        "      gift\n"
        "      amount\n"
        "      message\n"
        "      recentCount\n"
        "      expireDuration\n"
        "      ...VStreamChatSenderInfoFrag\n"
        "      __typename\n"
        "    }\n"
        "    ... on ChatHost {\n"
        "      id\n"
        "      viewer\n"
        "      ...VStreamChatSenderInfoFrag\n"
        "      __typename\n"
        "    }\n"
        "    ... on ChatSubscription {\n"
        "      id\n"
        "      month\n"
        "      ...VStreamChatSenderInfoFrag\n"
        "      __typename\n"
        "    }\n"
        "    ... on ChatExtendSub {\n"
        "      id\n"
        "      month\n"
        "      length\n"
        "      ...VStreamChatSenderInfoFrag\n"
        "      __typename\n"
        "    }\n"
        "    ... on ChatChangeMode {\n"
        "      mode\n"
        "      __typename\n"
        "    }\n"
        "    ... on ChatText {\n"
        "      id\n"
        "      emojis\n"
        "      content\n"
        "      createdAt\n"
        "      subLength\n"
        "      ...VStreamChatSenderInfoFrag\n"
        "      __typename\n"
        "    }\n"
        "    ... on ChatSubStreak {\n"
        "      id\n"
        "      ...VStreamChatSenderInfoFrag\n"
        "      length\n"
        "      __typename\n"
        "    }\n"
        "    ... on ChatClip {\n"
        "      id\n"
        "      url\n"
        "      ...VStreamChatSenderInfoFrag\n"
        "      __typename\n"
        "    }\n"
        "    ... on ChatFollow {\n"
        "      id\n"
        "      ...VStreamChatSenderInfoFrag\n"
        "      __typename\n"
        "    }\n"
        "    ... on ChatDelete {\n"
        "      ids\n"
        "      __typename\n"
        "    }\n"
        "    ... on ChatBan {\n"
        "      id\n"
        "      ...VStreamChatSenderInfoFrag\n"
        "      bannedBy {\n"
        "        id\n"
        "        displayname\n"
        "        __typename\n"
        "      }\n"
        "      bannedByRoomRole\n"
        "      __typename\n"
        "    }\n"
        "    ... on ChatModerator {\n"
        "      id\n"
        "      ...VStreamChatSenderInfoFrag\n"
        "      add\n"
        "      __typename\n"
        "    }\n"
        "    ... on ChatEmoteAdd {\n"
        "      id\n"
        "      ...VStreamChatSenderInfoFrag\n"
        "      emote\n"
        "      __typename\n"
        "    }\n"
        "    ... on ChatTimeout {\n"
        "      id\n"
        "      ...VStreamChatSenderInfoFrag\n"
        "      minute\n"
        "      bannedBy {\n"
        "        id\n"
        "        displayname\n"
        "        __typename\n"
        "      }\n"
        "      bannedByRoomRole\n"
        "      __typename\n"
        "    }\n"
        "    ... on ChatTCValueAdd {\n"
        "      id\n"
        "      ...VStreamChatSenderInfoFrag\n"
        "      amount\n"
        "      totalAmount\n"
        "      __typename\n"
        "    }\n"
        "    ... on ChatGiftSub {\n"
        "      id\n"
        "      ...VStreamChatSenderInfoFrag\n"
        "      count\n"
        "      receiver\n"
        "      __typename\n"
        "    }\n"
        "    ... on ChatGiftSubReceive {\n"
        "      id\n"
        "      ...VStreamChatSenderInfoFrag\n"
        "      gifter\n"
        "      __typename\n"
        "    }\n"
        "    __typename\n"
        "  }\n"
        "}\n"
        "\n"
        "fragment VStreamChatSenderInfoFrag on SenderInfo {\n"
        "  subscribing\n"
        "  role\n"
        "  roomRole\n"
        "  sender {\n"
        "    id\n"
        "    username\n"
        "    displayname\n"
        "    avatar\n"
        "    partnerStatus\n"
        "    badges\n"
        "    effect\n"
        "    __typename\n"
        "  }\n"
        "  __typename\n"
        "}\n";

    const QJsonObject payload = generateQuery("StreamMessageSubscription", OBFUSCATE(DLIVE_HASH_CHATROOM), {{ "streamer", info.userName }}, Query);

    send("start", payload, 1);
}

void DLive::onWebSocketReceived(const QString &raw)
{
    const QJsonObject root = QJsonDocument::fromJson(raw.toUtf8()).object();

    //qDebug() << "received:" << root;

    const QString type = root.value("type").toString();

    if (type == "data")
    {
        const QJsonObject data = root.value("payload").toObject().value("data").toObject();
        if (data.isEmpty())
        {
            qWarning() << "paload data is empty, root =" << root;
            return;
        }

        if (data.contains("streamMessageReceived"))
        {
            const QJsonArray jsonMessages = data.value("streamMessageReceived").toArray();
            if (jsonMessages.isEmpty())
            {
                qWarning() << "messages is empty, root =" << root;
                return;
            }

            parseMessages(jsonMessages);
        }
        else
        {
            qWarning() << "unknown data format, root =" << root;
            return;
        }
    }
    else if (type == "connection_ack")
    {
        if (!state.connected)
        {
            state.connected = true;
            emit connectedChanged(true);
            emit stateChanged();
        }
    }
    else if (type == "ka")
    {
        //
    }
    else
    {
        qWarning() << "unknown message type" << type << ", root =" << root;
        return;
    }
}

void DLive::requestChatRoom(const QString &displayName_)
{
    if (!enabled.get())
    {
        return;
    }

    QString displayName = displayName_.trimmed();
    if (displayName.isEmpty())
    {
        qWarning() << "display name is empty";
        return;
    }

    const QByteArray body = QJsonDocument(generateQuery("LivestreamChatroomInfo", OBFUSCATE(DLIVE_HASH_CHATROOM),
    {
        { "displayname", displayName },
        { "count", 40 },
        { "isLoggedIn", false },
        { "limit", 20 },
    })).toJson(QJsonDocument::JsonFormat::Compact);

    QNetworkRequest request(QUrl("https://graphigo.prd.dlive.tv/"));
    request.setRawHeader("Content-Type", "application/json");

    QNetworkReply* reply = network.post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, displayName]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(data).object();

        const QJsonObject jsonUser = root
            .value("data").toObject()
            .value("userByDisplayName").toObject();

        info.userName = jsonUser.value("username").toString();

        if (info.userName.isEmpty())
        {
            qWarning() << "user name is empty, display name =" << displayName;
            return;
        }

        state.chatUrl = "https://dlive.tv/c/" + state.streamId + "/" + info.userName;

        if (!state.connected || socket.state() != QAbstractSocket::SocketState::ConnectedState)
        {
            socket.open(QUrl("wss://graphigostream.prd.dlive.tv/"));
        }

        parseEmoji(jsonUser.value("emoji").toObject());

        emit stateChanged();
    });
}

void DLive::requestLiveStream(const QString &displayName_)
{
    if (!enabled.get())
    {
        return;
    }

    QString displayName = displayName_.trimmed();
    if (displayName.isEmpty())
    {
        qWarning() << "display name is empty";
        return;
    }

    const QByteArray body = QJsonDocument(generateQuery("LivestreamPage", OBFUSCATE(DLIVE_HASH_LIVESTREAM),
        {
            { "displayname", displayName },
            { "add", false },
            { "isLoggedIn", false },
            { "isMe", false },
            { "showUnpicked", false },
            { "order", "PickTime" },
        })).toJson(QJsonDocument::JsonFormat::Compact);

    QNetworkRequest request(QUrl("https://graphigo.prd.dlive.tv/"));
    request.setRawHeader("Content-Type", "application/json");

    QNetworkReply* reply = network.post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, displayName]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(data).object();

        const QJsonObject jsonUser = root
        .value("data").toObject()
        .value("userByDisplayName").toObject();

        state.viewersCount = -1;

        const QJsonObject jsonLivestream = jsonUser.value("livestream").toObject();

        state.viewersCount = jsonLivestream.value("watchingCount").toInt(-1);

        emit stateChanged();
    });
}

void DLive::parseMessages(const QJsonArray &jsonMessages)
{
    QList<std::shared_ptr<Message>> messages;
    QList<std::shared_ptr<Author>> authors;

    for (const QJsonValue& v : qAsConst(jsonMessages))
    {
        QPair<std::shared_ptr<Message>, std::shared_ptr<Author>> pair;

        const QJsonObject object = v.toObject();
        const QString typeName = object.value("__typename").toString();

        if (typeName == "ChatText")
        {
            pair = parseChatText(object);
        }
        else if (typeName == "ChatGift")
        {
            pair = parseChatGift(object);
        }
        else if (typeName == "ChatDelete")
        {
            const QString type = object.value("type").toString();
            if (type != "Delete")
            {
                qWarning() << "unknown type" << type;
            }

            const QJsonArray ids = object.value("ids").toArray();
            for (const QJsonValue& v : qAsConst(ids))
            {
                const QString id = v.toString();

                const auto pair = Message::Builder::createDeleter(getServiceType(), generateMessageId(id));

                messages.append(pair.second);
                authors.append(pair.first);
            }
        }
        else
        {
            qWarning() << "Unknown type name" << typeName << ", object =" << object;
            continue;
        }

        if (pair.first && pair.second)
        {
            messages.append(pair.first);
            authors.append(pair.second);
        }
    }

    emit readyRead(messages, authors);
}

std::shared_ptr<Author> DLive::parseSender(const QJsonObject &json) const
{
    const QString userName = json.value("username").toString();
    const QString displayName = json.value("displayname").toString();

    Author::Builder authorBuilder(getServiceType(), generateAuthorId(userName), displayName);
    authorBuilder.setAvatar(json.value("avatar").toString());
    authorBuilder.setPage("https://dlive.tv/" + displayName);
    //TODO: badges

    return authorBuilder.build();
}

QPair<std::shared_ptr<Message>, std::shared_ptr<Author>> DLive::parseChatText(const QJsonObject &json) const
{
    const QString type = json.value("type").toString();

    if (type != "Message")
    {
        qWarning() << "unknown type" << type << ", json =" << json;
    }

    auto author = parseSender(json.value("sender").toObject());

    Message::Builder messageBuilder(author, generateMessageId(json.value("id").toString()));

    messageBuilder.setPublishedTime(convertTime(json.value("createdAt").toString()));

    bool contentAsPlainText = false;
    const QString content = json.value("content").toString();

    bool isSticker = false;
    {
        static const QRegExp rx("^:emote/[a-zA-Z0-9_\\-]+/[a-zA-Z0-9_\\-]+/([a-zA-Z0-9_\\-]+):$");
        if (rx.indexIn(content.trimmed()) != -1)
        {
            const QString stickerId = rx.cap(1);
            if (!stickerId.isEmpty())
            {
                const QString stickerUrl = "https://images.prd.dlivecdn.com/emote/" + stickerId;

                messageBuilder.addImage(QUrl(stickerUrl), StickerHeight, false);

                isSticker = true;
            }
        }
    }

    if (!isSticker)
    {
        const QJsonArray emotesPositions = json.value("emojis").toArray();

        if (emotesPositions.count() > 0)
        {
            if (emotesPositions.count() % 2 == 0)
            {
                for (int i = 0; i < emotesPositions.count(); i += 2)
                {
                    const int left = emotesPositions[i].toInt();
                    const int right = emotesPositions[i + 1].toInt();

                    if (i == 0)
                    {
                        if (left > 0)
                        {
                            messageBuilder.addText(content.left(left));
                        }
                    }
                    else
                    {
                        const int textLeftPos = emotesPositions[i - 1].toInt() + 1;
                        const int textLength = left - textLeftPos;

                        const QString text = content.mid(textLeftPos, textLength);
                        messageBuilder.addText(text);
                    }

                    const QString emoteName = content.mid(left, right - left + 1);
                    const QString emoteUrl = emotes.value(emoteName);

                    if (!emoteUrl.isEmpty())
                    {
                        messageBuilder.addImage(emoteUrl, EmoteHeight, false);
                    }
                    else
                    {
                        messageBuilder.addText(emoteName);

                        qWarning() << "unknown emote" << emoteName;
                    }
                }

                const int leftPosText = emotesPositions.last().toInt() + 1;
                if (leftPosText < content.length() - 1)
                {
                    messageBuilder.addText(content.mid(leftPosText));
                }
            }
            else
            {
                contentAsPlainText = true;

                qWarning() << "emotes positions array size % 2 != 0";
            }
        }
        else
        {
            contentAsPlainText = true;
        }

        if (contentAsPlainText)
        {
            messageBuilder.addText(content);
        }
    }

    return { messageBuilder.build(), author };
}

QPair<std::shared_ptr<Message>, std::shared_ptr<Author> > DLive::parseChatGift(const QJsonObject &json) const
{
    const QString type = json.value("type").toString();

    if (type != "Gift")
    {
        qWarning() << "unknown type" << type << ", json =" << json;
    }

    const int amout = json.value("amount").toString("-1").toInt();
    const QString giftName = json.value("gift").toString();
    const QString message = json.value("message").toString();

    auto author = parseSender(json.value("sender").toObject());

    Message::Builder messageBuilder(author, generateMessageId(json.value("id").toString()));

    messageBuilder.setPublishedTime(convertTime(json.value("createdAt").toString()));

    messageBuilder.setForcedColor(Message::ColorRole::BodyBackgroundColorRole, GiftColor);

    Message::TextStyle style;
    style.bold = true;

    messageBuilder.addText(tr("Gift: %1\nAmout: %2").arg(giftName).arg(amout), style);

    if (!message.isEmpty())
    {
        messageBuilder.addText("\n\n");
        messageBuilder.addText(message);
    }

    return { messageBuilder.build(), author };
}

void DLive::parseEmoji(const QJsonObject &json)
{
    emotes.clear();

    {
        const QJsonArray list = json.value("global")
            .toObject().value("list")
            .toArray();

        for (const QJsonValue& v : qAsConst(list))
        {
            const QJsonObject emoji = v.toObject();

            const QString name = emoji.value("name").toString();
            const QString url = emoji.value("sourceURL").toString();

            emotes.insert(name, url);
        }
    }

    {
        const QJsonArray list = json.value("vip")
            .toObject().value("list")
            .toArray();

        for (const QJsonValue& v : qAsConst(list))
        {
            const QJsonObject emoji = v.toObject();

            const QString name = emoji.value("name").toString();
            const QString url = emoji.value("sourceURL").toString();

            emotes.insert(name, url);
        }
    }
}

QString DLive::extractChannelName(const QString &stream)
{
    QRegExp rx;

    const QString simpleUserSpecifiedUserChannel = AxelChat::simplifyUrl(stream);
    rx = QRegExp("^dlive.tv/([^/]*)$", Qt::CaseInsensitive);
    if (rx.indexIn(simpleUserSpecifiedUserChannel) != -1)
    {
        return rx.cap(1);
    }

    rx = QRegExp("^[a-zA-Z0-9_]+$", Qt::CaseInsensitive);
    if (rx.indexIn(stream) != -1)
    {
        return stream;
    }

    return QString();
}

QJsonObject DLive::generateQuery(const QString &operationName, const QString& hash, const QMap<QString, QJsonValue> &variables, const QString &query)
{
    QJsonObject result(
    {
        { "operationName", operationName },
        { "extensions", QJsonObject(
            {
                { "persistedQuery", QJsonObject(
                    {
                        { "version", 1 },
                        { "sha256Hash", hash },
                    })
                }
            })
        }
    });

    QJsonObject jsonVariables;

    const QStringList varNames = variables.keys();
    for (const QString& varName : qAsConst(varNames))
    {
        const QJsonValue value = variables.value(varName);

        jsonVariables.insert(varName, value);
    }

    result.insert("variables", jsonVariables);

    if (!query.isEmpty())
    {
        result.insert("query", query);
    }

    return result;
}
