#include "kick.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace
{

static const int ReconncectPeriod = 5 * 1000;
static const int RequestChannelInfoInterval = 10000;
static const QString APP_ID = "eb1d5f283081a78b932c"; // TODO
static const int EmoteImageHeight = 32;

}

Kick::Kick(QSettings &settings, const QString &settingsGroupPathParent, QNetworkAccessManager &network_, cweqt::Manager& web_, QObject *parent)
    : ChatService(settings, settingsGroupPathParent, AxelChat::ServiceType::Kick, parent)
    , network(network_)
    , web(web_)
    , socket("https://kick.com")
{
    getUIElementBridgeBySetting(stream)->setItemProperty("name", tr("Channel"));
    getUIElementBridgeBySetting(stream)->setItemProperty("placeholderText", tr("Link or channel name..."));

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state)
    {
        Q_UNUSED(state)
        //qDebug() << Q_FUNC_INFO << ": WebSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &Kick::onWebSocketReceived);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        //qDebug() << Q_FUNC_INFO << ": WebSocket connected";

        if (info.chatroomId.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "chatroom id is empty";
            socket.close();
            return;
        }

        sendSubscribe(info.chatroomId);

        emit stateChanged();
    });

    QObject::connect(&socket, &QWebSocket::disconnected, this, [this]()
    {
        //qDebug() << Q_FUNC_INFO << ": WebSocket disconnected";

        if (state.connected)
        {
            state.connected = false;
            emit stateChanged();
            emit connectedChanged(false);
        }
    });

    QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError error_)
    {
        qDebug() << Q_FUNC_INFO << ": WebSocket error:" << error_ << ":" << socket.errorString();
    });

    reconnect();

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

    QObject::connect(&web, &cweqt::Manager::initialized, this, [this]()
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

    QObject::connect(&timerRequestChannelInfo, &QTimer::timeout, this, [this]()
    {
        if (!enabled.get() || state.streamId.isEmpty())
        {
            return;
        }

        requestChannelInfo(state.streamId);
    });
    timerRequestChannelInfo.start(RequestChannelInfoInterval);
}

ChatService::ConnectionStateType Kick::getConnectionStateType() const
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

QString Kick::getStateDescription() const
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

void Kick::reconnectImpl()
{
    socket.close();

    state = State();
    info = Info();

    state.controlPanelUrl = QUrl(QString("https://kick.com/dashboard/stream"));

    state.streamId = extractChannelName(stream.get());

    if (state.streamId.isEmpty())
    {
        emit stateChanged();
        return;
    }

    state.chatUrl = QUrl(QString("https://kick.com/%1/chatroom").arg(state.streamId));
    state.streamUrl = QUrl(QString("https://kick.com/%1").arg(state.streamId));

    if (enabled.get())
    {
        requestChannelInfo(state.streamId);
    }
}

void Kick::onWebSocketReceived(const QString &rawData)
{
    if (!enabled.get())
    {
        return;
    }

    //qDebug("\nreceived:\n" + rawData.toUtf8() + "\n");

    const QJsonObject root = QJsonDocument::fromJson(rawData.toUtf8()).object();
    const QString type = root.value("event").toString();
    const QJsonObject data = QJsonDocument::fromJson(root.value("data").toString().toUtf8()).object();

    if (type == "App\\Events\\ChatMessageEvent")
    {
        parseChatMessageEvent(data);
    }
    else if (type == "pusher:pong")
    {
        // TODO
    }
    else if (type == "App\\Events\\MessageDeletedEvent")
    {
        parseMessageDeletedEvent(data);
    }
    else if (type == "App\\Events\\UserBannedEvent")
    {
        // TODO:
        // {"event":"App\\Events\\UserBannedEvent","data":"{\"id\":\"33667119-a401-4175-bf82-c9040940c4ba\",\"user\":{\"id\":8401465,\"username\":\"2PAC_SHAKUR\",\"slug\":\"2pac-shakur\"},\"banned_by\":{\"id\":8537435,\"username\":\"TheJuicer\",\"slug\":\"thejuicer\"},\"expires_at\":\"2023-06-23T13:09:00+00:00\"}","channel":"chatrooms.668.v2"}
    }
    else if (type == "App\\Events\\UserUnbannedEvent")
    {
        //TODO:
        // {"id":"e7467947-44b1-4ff6-a465-26a8c525f6a9","unbanned_by":{"id":7334,"slug":"daiminister","username":"daiminister"},"user":{"id":13811411,"slug":"pistolpetey23","username":"pistolpetey23"}}
    }
    else if (type == "App\\Events\\PinnedMessageCreatedEvent")
    {
        //TODO:
        // {"duration":"20","message":{"chatroom_id":"4530","content":"For all our new people, please make sure to verify before you join a game or raffle. Type !verify","created_at":"2023-07-14T20:08:20+00:00","id":"088a8390-cce6-4ea5-b4f4-e1b2b032c64f","sender":{"id":"10863","identity":{"badges":[{"text":"Moderator","type":"moderator"}],"color":"#75FD46"},"slug":"serekorr","username":"SereKorr"},"type":"message"}}
    }
    else if (type == "App\\Events\\SubscriptionEvent")
    {
        //TODO:
        // {"chatroom_id":32806,"months":3,"username":"rak_eem_t"}
    }
    else if (type == "App\\Events\\StreamHostEvent")
    {
        //TODO:
        // {"chatroom_id":32806,"host_username":"7eventyy","number_viewers":6,"optional_message":""}
    }
    else if (type == "App\\Events\\GiftedSubscriptionsEvent")
    {
        //TODO:
        // {"chatroom_id":32806,"gifted_usernames":["lukechet","Rhoelle","Stuie2k1","chefcurry","kewlar","Layn","rvxky","ap09","bellrizzy","Iaht"],"gifter_username":"Kobebeef22"}
    }
    else if (type == "App\\Events\\ChatroomUpdatedEvent")
    {
        //TODO:
        // {"advanced_bot_protection":{"enabled":false,"remaining_time":0},"emotes_mode":{"enabled":false},"followers_mode":{"enabled":true,"min_duration":10},"id":668,"slow_mode":{"enabled":false,"message_interval":6},"subscribers_mode":{"enabled":false}}
    }
    else if (type == "pusher:connection_established" || type == "pusher_internal:subscription_succeeded")
    {
        if (!state.connected)
        {
            state.connected = true;
            emit connectedChanged(true);
            emit stateChanged();
        }

        sendPing();
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "unknown event type" << type << ", data:";
        qDebug() << data; qDebug();
    }
}

void Kick::send(const QJsonObject &object)
{
    const QString data = QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::JsonFormat::Compact));

    //qDebug("\nsend: " + data.toUtf8() + "\n");

    socket.sendTextMessage(data);
}

void Kick::sendSubscribe(const QString& chatroomId)
{
    send(QJsonObject(
             {
                 { "event", "pusher:subscribe" },
                 { "data", QJsonObject(
                    {
                        { "auth", "" },
                        { "channel", "chatrooms." + chatroomId + ".v2" }
                    })}
             }));
}

void Kick::sendPing()
{
    send(QJsonObject(
             {
                 { "event", "pusher:ping" },
                 { "data", QJsonObject()}
             }));
}

void Kick::requestChannelInfo(const QString &slug)
{
    if (!web.isInitialized())
    {
        return;
    }

    cweqt::Browser::Settings::Filter filter;
    filter.urlPrefixes = { "https://kick.com/api/" };
    filter.mimeTypes = { "text/html", "application/json" };

    web.createDisposable("https://kick.com/api/v2/channels/" + slug, filter, [this, slug](std::shared_ptr<cweqt::Response> response, bool&)
    {
        QJsonParseError error;
        const QJsonObject root = QJsonDocument::fromJson(response->data, &error).object();
        if (error.error != QJsonParseError::ParseError::NoError)
        {
            qWarning() << Q_FUNC_INFO << "json parse error =" << error.errorString() << ", offset =" << error.offset << ", mimeType =" << response->mimeType;// << ", data =" << response->data;
            return;
        }

        const QJsonObject userJson = root.value("user").toObject();

        User user;
        user.avatar = userJson.value("profile_pic").toString();
        if (user.avatar.isEmpty())
        {
            user.avatar = "qrc:/resources/images/kick-default-avatar.png";
        }

        users.insert(generateAuthorId(slug), user);

        emit authorDataUpdated(generateAuthorId(slug), {{Author::Role::AvatarUrl, user.avatar}});

        if (slug == state.streamId)
        {
            state.viewersCount = -1;

            if (socket.state() != QAbstractSocket::SocketState::ConnectedState)
            {
                const QJsonObject chatroom = root.value("chatroom").toObject();

                bool ok = false;
                int64_t chatroomId = chatroom.value("id").toVariant().toLongLong(&ok);
                if (!ok)
                {
                    qWarning() << Q_FUNC_INFO << "failed to convert chatroom id";
                    return;
                }

                info.chatroomId = QString("%1").arg(chatroomId);

                socket.open("wss://ws-us2.pusher.com/app/" + APP_ID + "?protocol=7&client=js&version=7.6.0&flash=false");
            }

            const QJsonArray badgesArray = root.value("subscriber_badges").toArray();
            for (const QJsonValue& v : qAsConst(badgesArray))
            {
                const QJsonObject badgeJson = v.toObject();
                const int months = badgeJson.value("months").toInt();
                const QString url = badgeJson.value("badge_image").toObject().value("src").toString().replace("\\/", "/");
                info.subscriberBadges.insert(months, url);
            }

            state.viewersCount = root.value("livestream").toObject().value("viewer_count").toInt(-1);

            emit stateChanged();
        }
    });
}

void Kick::parseChatMessageEvent(const QJsonObject &data)
{
    const QString type = data.value("type").toString();
    const QString id = data.value("id").toString();
    const QString rawContent = data.value("content").toString();
    const QDateTime publishedTime = QDateTime::fromString(data.value("created_at").toString(), Qt::DateFormat::ISODate);

    const QJsonObject sender = data.value("sender").toObject();
    const QString slug = sender.value("slug").toString();
    const QString authorName = sender.value("username").toString();

    const QJsonObject identity = sender.value("identity").toObject();
    const QColor authorColor = identity.value("color").toString();
    const QJsonArray badgesJson = identity.value("badges").toArray();

    if (type == "message")
    {
        //
    }
    else if (type == "reply")
    {
        // TODO:
        // {"chatroom_id":4530,"content":"[emote:17289:frankdimespeepolove][emote:16676:frankdimesVibe]","created_at":"2023-07-14T19:58:10+00:00","id":"a7354ff8-7ef7-47e7-843a-90963b137071","metadata":{"original_message":{"content":"@kasperh32 hi bruh seme for u","id":"d94a16ec-3f39-47d2-afad-a493e967f983"},"original_sender":{"id":"9899","username":"evilnero1981"}},"sender":{"id":8082,"identity":{"badges":[{"text":"Moderator","type":"moderator"}],"color":"#75FD46"},"slug":"kasperh32","username":"kasperh32"},"type":"reply"}
        //qDebug() << data; qDebug();
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "unkown message type" << type;
        qDebug() << data; qDebug();
    }

    if (rawContent.isEmpty())
    {
        return;
    }

    //https://files.kick.com/emotes/39268/fullsize

    //users.insert(generateAuthorId(channelName), user);

    const QString userId = generateAuthorId(slug);

    if (!users.contains(userId))
    {
        requestChannelInfo(slug);
    }

    QStringList leftBadges;

    for (const QJsonValue& value : qAsConst(badgesJson))
    {
        leftBadges.append(parseBadge(value.toObject(), "qrc:/resources/images/unknown-badge.png"));
    }

    QList<std::shared_ptr<Message>> messages;
    QList<std::shared_ptr<Author>> authors;

    std::shared_ptr<Author> author = std::make_shared<Author>(
        getServiceType(),
        authorName,
        userId,
        QUrl(),
        QUrl("https://kick.com/" + slug),
        leftBadges, QStringList(), std::set<Author::Flag>(),
        authorColor);

    std::shared_ptr<Message> message = std::make_shared<Message>(
        parseContents(rawContent),
        author,
        publishedTime,
        QDateTime::currentDateTime(),
        generateMessageId(id));

    messages.append(message);
    authors.append(author);

    if (!messages.isEmpty())
    {
        emit readyRead(messages, authors);
    }

    //qDebug() << data; qDebug();
}

void Kick::parseMessageDeletedEvent(const QJsonObject &data)
{
    const QString rawId = data.value("message").toObject().value("id").toString();
    const QString messageId = generateMessageId(rawId);

    QList<std::shared_ptr<Message>> messages;
    QList<std::shared_ptr<Author>> authors;

    static const std::shared_ptr<Author> author = std::make_shared<Author>(getServiceType(), "", "");

    messages.append(std::make_shared<Message>(QList<std::shared_ptr<Message::Content>>(),
                            author,
                            QDateTime::currentDateTime(),
                            QDateTime::currentDateTime(),
                            messageId,
                            std::set<Message::Flag>{Message::Flag::DeleterItem}));

    authors.append(author);

    if (!messages.isEmpty())
    {
        emit readyRead(messages, authors);
    }
}

QString Kick::parseBadge(const QJsonObject &basgeJson, const QString &defaultValue) const
{
    const QString type = basgeJson.value("type").toString();
    QString file = ":/resources/images/kick/badges/" + type + ".svg";
    bool isUrl = false;

    if (type == "subscriber")
    {
        const int count = basgeJson.value("count").toInt();
        for (int i = count; i >= 1; --i)
        {
            if (info.subscriberBadges.contains(i))
            {
                isUrl = true;
                file = info.subscriberBadges.value(i);
                break;
            }
            else
            {
                file = ":/resources/images/kick/badges/subscriber.svg";
            }
        }
    }
    else if (type == "sub_gifter")
    {
        const int count = basgeJson.value("count").toInt();
        if (count >= 1 && count <= 20) // TODO: 1-20...
        {
            file = ":/resources/images/kick/badges/sub_gifter_blue.svg";
        }
        else if (count >= 28 && count <= 43) // TODO: ...28-43...
        {
            file = ":/resources/images/kick/badges/sub_gifter_purple.svg";
        }
        else if (count >= 52 && count <= 85) // TODO: ...52-85...
        {
            file = ":/resources/images/kick/badges/sub_gifter_red.svg";
        }
        else if (count >= 131 && count <= 145) // TODO: ...131-145...
        {
            file = ":/resources/images/kick/badges/sub_gifter_yellow.svg";
        }
        else if (count >= 220 && count <= 631) // TODO: ...220-631...
        {
            file = ":/resources/images/kick/badges/sub_gifter_green.svg";
        }
        else // TODO: implement other count
        {
            qWarning() << "Not found count" << count << ", data =" << basgeJson;
        }
    }

    if (isUrl)
    {
        return file;
    }
    else
    {
        if (QFileInfo::exists(file))
        {
            return "qrc" + file;
        }
    }

    qWarning() << Q_FUNC_INFO << "unknown badge, data =" << basgeJson;

    return defaultValue;
}

QString Kick::extractChannelName(const QString &stream_)
{
    QString stream = stream_.trimmed();
    if (stream.contains('/'))
    {
        stream = AxelChat::simplifyUrl(stream);
        bool ok = false;
        stream = AxelChat::removeFromStart(stream, "kick.com/", Qt::CaseSensitivity::CaseInsensitive, &ok);
        if (!ok)
        {
            return QString();
        }
    }

    if (stream.contains('/'))
    {
        stream = stream.left(stream.indexOf('/'));
    }

    return stream;
}

QList<std::shared_ptr<Message::Content>> Kick::parseContents(const QString &rawText)
{
    QList<std::shared_ptr<Message::Content>> contents;

    QString text;
    for (int i = 0; i < rawText.length(); ++i)
    {
        const QChar& c = rawText[i];

        if (c == '[')
        {
            if (const int lastBrace = rawText.indexOf(']', i); lastBrace != -1)
            {
                const QString part = rawText.mid(i + 1, lastBrace - (i + 1));
                const QStringList parts = part.split(':', Qt::SplitBehaviorFlags::KeepEmptyParts);
                if (parts.count() >= 2 && parts[0] == "emote")
                {
                    if (!text.isEmpty())
                    {
                        contents.append(std::make_shared<Message::Text>(text));
                        text = QString();
                    }

                    const QUrl url = "https://files.kick.com/emotes/" + parts[1] + "/fullsize";

                    contents.append(std::make_shared<Message::Image>(url, EmoteImageHeight, false));

                    i = lastBrace;
                    continue;
                }
            }
        }

        text += c;
    }

    if (!text.isEmpty())
    {
        contents.append(std::make_shared<Message::Text>(text));
        text = QString();
    }

    return contents;
}
