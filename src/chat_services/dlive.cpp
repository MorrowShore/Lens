#include "dlive.h"
#include "utils/QtStringUtils.h"
#include <QNetworkRequest>
#include <QNetworkReply>

namespace
{

static const int UpdateStreamInfoPeriod = 10 * 1000;
static const int CheckPingTimeout = 30 * 1000;
static const int EmoteHeight = 28;
static const int StickerHeight = 80;
static const QColor HighlightColor = QColor(107, 214, 214);

static bool checkReply(QNetworkReply *reply, const char *tag, QByteArray &resultData)
{
    resultData.clear();

    if (!reply)
    {
        qCritical() << tag << ": reply is null";
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
        qCritical() << tag << ": data is empty";
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

static QString getGiftName(const QString& type)
{
    if (type == "LEMON")
    {
        return QTranslator::tr("lemon");
    }
    else if (type == "ICE_CREAM")
    {
        return QTranslator::tr("ice cream");
    }
    else if (type == "DIAMOND")
    {
        return QTranslator::tr("diamond");
    }
    else if (type == "NINJAGHINI")
    {
        return QTranslator::tr("Ninjaghini");
    }
    else if (type == "NINJET")
    {
        return QTranslator::tr("Ninjet");
    }

    qCritical() << "unknown gift type" << type;

    return type;
}

}

DLive::DLive(ChatManager& manager, QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network_, cweqt::Manager&, QObject *parent)
    : ChatService(manager, settings, settingsGroupPathParent, ChatServiceType::DLive, false, parent)
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
        setConnected(false);
    });

    QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError error_)
    {
        qCritical() << "WebSocket error:" << error_ << ":" << socket.errorString();
    });

    QObject::connect(&timerUpdaetStreamInfo, &QTimer::timeout, this, [this]()
    {
        if (!isEnabled() || info.userName.isEmpty())
        {
            return;
        }

        requestLiveStream(state.streamId);
    });
    timerUpdaetStreamInfo.start(UpdateStreamInfoPeriod);

    QObject::connect(&checkPingTimer, &QTimer::timeout, this, [this]()
    {
        if (socket.state() != QAbstractSocket::SocketState::ConnectedState)
        {
            checkPingTimer.stop();
            return;
        }

        qWarning() << "check ping timeout, disconnect";
        socket.close();
    });
}

ChatService::ConnectionState DLive::getConnectionState() const
{
    if (isConnected())
    {
        return ChatService::ConnectionState::Connected;
    }
    else if (isEnabled() && !state.streamId.isEmpty())
    {
        return ChatService::ConnectionState::Connecting;
    }
    
    return ChatService::ConnectionState::NotConnected;
}

QString DLive::getMainError() const
{
    if (state.streamId.isEmpty())
    {
        return tr("Channel not specified");
    }
    return tr("Not connected");
}

void DLive::reconnectImpl()
{
    info = Info();

    socket.close();

    state.controlPanelUrl = "https://dlive.tv/s/dashboard#0";

    state.streamId = extractChannelName(stream.get());

    if (state.streamId.isEmpty())
    {
        return;
    }

    state.streamUrl = "https://dlive.tv/" + state.streamId;

    if (!isEnabled())
    {
        return;
    }

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

    static const QString Hash = "7e70c2e0ec55dd3594c55b6907bef1dbc32a67eb9b608caf0552c2026994f8a2";
    const QJsonObject payload = generateQuery("StreamMessageSubscription", Hash, {{ "streamer", info.userName }}, Query);

    send("start", payload, 1);
}

void DLive::onWebSocketReceived(const QString &raw)
{
    const QJsonObject root = QJsonDocument::fromJson(raw.toUtf8()).object();

    //qDebug() << "received:" << root;

    checkPingTimer.setInterval(CheckPingTimeout);
    checkPingTimer.start();

    const QString type = root.value("type").toString();

    if (type == "data")
    {
        const QJsonObject data = root.value("payload").toObject().value("data").toObject();
        if (data.isEmpty())
        {
            qCritical() << "payload data is empty, root =" << root;
            return;
        }

        if (data.contains("streamMessageReceived"))
        {
            const QJsonArray jsonMessages = data.value("streamMessageReceived").toArray();
            if (jsonMessages.isEmpty())
            {
                qCritical() << "messages is empty, root =" << root;
                return;
            }

            parseMessages(jsonMessages);
        }
        else
        {
            qCritical() << "unknown data format, root =" << root;
            return;
        }
    }
    else if (type == "connection_ack")
    {
        setConnected(true);

        checkPingTimer.setInterval(CheckPingTimeout);
        checkPingTimer.start();
    }
    else if (type == "ka")
    {
        //
    }
    else
    {
        qCritical() << "unknown message type" << type << ", root =" << root;
        return;
    }
}

void DLive::requestChatRoom(const QString &displayName_)
{
    if (!isEnabled())
    {
        return;
    }

    QString displayName = displayName_.trimmed();
    if (displayName.isEmpty())
    {
        qCritical() << "display name is empty";
        return;
    }

    static const QString Hash = "5f7d24cc4ec8e7fb23fdc3a16ee0db8694fb75937b45551498c82dbec7d1e2e7";
    const QByteArray body = QJsonDocument(generateQuery("LivestreamChatroomInfo", Hash,
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
        const QJsonObject jsonData = root.value("data").toObject();

        parseBadges(jsonData.value("listBadgeResource").toArray());

        const QJsonObject jsonUser = jsonData.value("userByDisplayName").toObject();

        info.userName = jsonUser.value("username").toString();

        {
            const QJsonObject json = jsonUser.value("subSetting").toObject();

            Author::Tag tag;
            tag.text = json.value("badgeText").toString();
            tag.color = json.value("badgeColor").toString();
            tag.textColor = json.value("textColor").toString();

            info.subscriberTag = tag;
        }

        if (info.userName.isEmpty())
        {
            qCritical() << "user name is empty, display name =" << displayName;
            return;
        }

        state.chatUrl = "https://dlive.tv/c/" + state.streamId + "/" + info.userName;

        if (!isConnected() || socket.state() != QAbstractSocket::SocketState::ConnectedState)
        {
            socket.setProxy(network.proxy());
            socket.open(QUrl("wss://graphigostream.prd.dlive.tv/"));
        }

        parseEmoji(jsonUser.value("emoji").toObject());

        emit stateChanged();

        parseMessages(jsonUser.value("chats").toArray());
    });
}

void DLive::requestLiveStream(const QString &displayName_)
{
    if (!isEnabled())
    {
        return;
    }

    QString displayName = displayName_.trimmed();
    if (displayName.isEmpty())
    {
        qCritical() << "display name is empty";
        return;
    }

    static const QString Hash = "950c61faccae0df49c8e19a3a0e741ccb39fd322c850bca52a7562bfa63f49c1";
    const QByteArray body = QJsonDocument(generateQuery("LivestreamPage", Hash,
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
        
        const QJsonObject jsonLivestream = jsonUser.value("livestream").toObject();
        
        setViewers(jsonLivestream.value("watchingCount").toInt(-1));
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
        else if (typeName == "ChatFollow")
        {
            pair = parseGenericMessage(object, tr("Just followed!"), true);
        }
        else if (typeName == "ChatSubscription")
        {
            pair = parseGenericMessage(object, tr("Just subscribed monthly!"), true);
        }
        else if (typeName == "ChatSubStreak")
        {
            const int count = object.value("length").toInt();

            pair = parseGenericMessage(object, tr("Is celebrating %1-month sub streak!").arg(count), true);
        }
        else if (typeName == "ChatTCValueAdd")
        {
            pair = parseGenericMessage(object, tr("Just added something to Chest!"), true);
        }
        else if (typeName == "ChatClip")
        {
            pair = parseGenericMessage(object, tr("Just made a clip"), true);
        }
        else if (typeName == "ChatHost")
        {
            const int count = object.value("viewer").toInt();

            pair = parseGenericMessage(object, tr("With %1 viewers now is HOSTING!").arg(count), true);
        }
        else if (typeName == "ChatGiftSub")
        {
            const QString receiver = object.value("receiver").toString();
            pair = parseGenericMessage(object, tr("Gifted a one-month subscription to %1!").arg(receiver), true);
        }
        else if (typeName == "ChatGiftSubReceive")
        {
            const QString gifter = object.value("gifter").toString();
            pair = parseGenericMessage(object, tr("Just received a one-month subscription from %1!").arg(gifter), true);
        }
        else if (typeName == "ChatDelete")
        {
            const QString type = object.value("type").toString();
            if (type != "Delete")
            {
                qCritical() << "unknown type" << type;
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
        else if (
            typeName == "ChatLive" ||
            typeName == "ChatOffline" ||
            typeName == "ChatModerator" ||
            typeName == "ChatEmoteAdd")
        {
            //
        }
        else
        {
            qCritical() << "Unknown type name" << typeName << ", object =" << object;
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

std::shared_ptr<Author> DLive::parseAuthorFromMessage(const QJsonObject &jsonMessage) const
{
    const QJsonObject sender = jsonMessage.value("sender").toObject();

    const QString userName = sender.value("username").toString();
    const QString displayName = sender.value("displayname").toString();

    Author::Builder authorBuilder(getServiceType(), generateAuthorId(userName), displayName);
    authorBuilder.setAvatar(sender.value("avatar").toString());
    authorBuilder.setPage("https://dlive.tv/" + displayName);

    // TODO: badges for Staff, GLive Guardian

    {
        // role

        const QString role = jsonMessage.value("role").toString();
        if (role == "Bot")
        {
            authorBuilder.addLeftBadge("https://dlive.tv/img/bot-icon.0df374e6.svg");
        }
        else if (role.isEmpty() || role == "None")
        {
            //
        }
        else
        {
            qCritical() << "unknown role" << role << ", message =" << jsonMessage;

            authorBuilder.addLeftBadge(UnknownBadge);
        }
    }

    {
        // room role

        const QString roomRole = jsonMessage.value("roomRole").toString();
        if (roomRole == "Moderator")
        {
            authorBuilder.addLeftBadge("https://dlive.tv/img/moderator-icon.ad4d0ed2.svg");
        }
        else if (roomRole == "Owner")
        {
            authorBuilder.addLeftBadge("https://dlive.tv/img/streamer-icon.6a664131.svg");
        }
        else if (roomRole.isEmpty() || roomRole == "Member")
        {
            //
        }
        else
        {
            qCritical() << "unknown room role" << roomRole << ", message =" << jsonMessage;

            authorBuilder.addLeftBadge(UnknownBadge);
        }
    }

    {
        // named badges

        const QJsonArray jsonBadges = sender.value("badges").toArray();

        for (const QJsonValue& v : qAsConst(jsonBadges))
        {
            const QString badgeName = v.toString();

            if (badges.contains(badgeName))
            {
                authorBuilder.addLeftBadge(badges.value(badgeName));
            }
            else
            {
                qWarning() << "unknown bagde" << badgeName;

                authorBuilder.addLeftBadge(UnknownBadge);
            }
        }
    }

    {
        // partner status

        const QString status = sender.value("partnerStatus").toString();
        if (status == "VERIFIED_PARTNER")
        {
            authorBuilder.addLeftBadge("https://dlive.tv/img/verified-badge.f5557500.svg");
        }
        else if (status == "GLOBAL_PARTNER")
        {
            authorBuilder.addLeftBadge("https://dlive.tv/img/global-badge.d2fca949.svg");
        }
        else if (status.isEmpty() || status == "NONE" || status == "AFFILIATE")
        {
            //
        }
        else
        {
            qCritical() << "unknown partner status" << status << ", message =" << jsonMessage;

            authorBuilder.addLeftBadge(UnknownBadge);
        }
    }

    {
        // subscribing tags

        const bool subscribing =  jsonMessage.value("subscribing").toBool();
        if (subscribing)
        {
            if (info.subscriberTag)
            {
                authorBuilder.addLeftTag(*info.subscriberTag);
            }
            else
            {
                qWarning() << "subscriber tag is null";
            }
        }
    }

    return authorBuilder.build();
}

QPair<std::shared_ptr<Message>, std::shared_ptr<Author>> DLive::parseChatText(const QJsonObject &json) const
{
    const QString type = json.value("type").toString();

    if (type != "Message")
    {
        qCritical() << "unknown type" << type << ", json =" << json;
    }

    auto author = parseAuthorFromMessage(json);

    Message::Builder messageBuilder(author, generateMessageId(json.value("id").toString()));

    messageBuilder.setPublishedTime(convertTime(json.value("createdAt").toString()));

    bool contentAsPlainText = false;
    const QString content = json.value("content").toString();

    bool isSticker = false;
    {
        static const QRegExp rx("^:emote/[a-zA-Z0-9_\\-\\.]+/[a-zA-Z0-9_\\-\\.]+/([a-zA-Z0-9_\\-\\.]+):$");
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
                        messageBuilder.addImage("https://images.prd.dlivecdn.com/emoji/" + emoteName, EmoteHeight, false);
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

                qCritical() << "emotes positions array size % 2 != 0";
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
    const QString giftType = json.value("gift").toString();
    const QString message = json.value("message").toString().trimmed();

    auto author = parseAuthorFromMessage(json);

    Message::Builder messageBuilder(author, generateMessageId(json.value("id").toString()));

    messageBuilder.setPublishedTime(convertTime(json.value("createdAt").toString()));

    messageBuilder.setForcedColor(Message::ColorRole::BodyBackgroundColorRole, HighlightColor);

    Message::TextStyle style;
    style.bold = true;

    messageBuilder.addText(tr("Just donated %1 %2").arg(amout).arg(getGiftName(giftType)), style);

    if (!message.isEmpty())
    {
        messageBuilder.addText("\n\n");
        messageBuilder.addText(message);
    }

    return { messageBuilder.build(), author };
}

QPair<std::shared_ptr<Message>, std::shared_ptr<Author>> DLive::parseGenericMessage(const QJsonObject &json, const QString &text, bool highlight) const
{
    auto author = parseAuthorFromMessage(json);

    Message::Builder messageBuilder(author, generateMessageId(json.value("id").toString()));

    messageBuilder.setPublishedTime(convertTime(json.value("createdAt").toString()));

    Message::TextStyle style;

    if (highlight)
    {
        style.bold = true;
        messageBuilder.setForcedColor(Message::ColorRole::BodyBackgroundColorRole, HighlightColor);
    }

    messageBuilder.addText(text, style);

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

void DLive::parseBadges(const QJsonArray &jsonBadges)
{
    badges.clear();

    for (const QJsonValue& v : qAsConst(jsonBadges))
    {
        const QJsonObject o = v.toObject();
        const QString name = o.value("name").toString();
        const QString url = o.value("url").toString();

        badges.insert(name, url);
    }
}

QString DLive::extractChannelName(const QString &stream)
{
    QRegExp rx;

    const QString simpleUserSpecifiedUserChannel = QtStringUtils::simplifyUrl(stream);

    QString result = stream;

    rx = QRegExp("^dlive.tv/(.*)$", Qt::CaseInsensitive);
    if (rx.indexIn(simpleUserSpecifiedUserChannel) != -1)
    {
        result = rx.cap(1);

        if (result.startsWith("c/", Qt::CaseSensitivity::CaseInsensitive))
        {
            result = result.mid(2);
        }
    }

    if (result.contains('/'))
    {
        result = result.left(result.indexOf('/'));
    }

    rx = QRegExp("^[a-zA-Z0-9_\\-\\.]+$", Qt::CaseInsensitive);
    if (rx.indexIn(result) != -1)
    {
        return result;
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
