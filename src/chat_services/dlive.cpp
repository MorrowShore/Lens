#include "dlive.h"
#include <QNetworkRequest>
#include <QNetworkReply>

namespace
{

static const int UpdateStreamInfoPeriod = 10 * 1000;

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

    static const QString Hash = "5f7d24cc4ec8e7fb23fdc3a16ee0db8694fb75937b45551498c82dbec7d1e2e7"; // TODO

    const QJsonObject payload = generateQuery("StreamMessageSubscription", Hash, {{ "streamer", info.userName }}, Query);

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

    static const QString Hash = "5f7d24cc4ec8e7fb23fdc3a16ee0db8694fb75937b45551498c82dbec7d1e2e7"; // TODO

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

        if (!state.connected)
        {
            socket.open(QUrl("wss://graphigostream.prd.dlive.tv/"));
        }

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

    static const QString Hash = "950c61faccae0df49c8e19a3a0e741ccb39fd322c850bca52a7562bfa63f49c1"; // TODO

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
        auto pair = parseMessage(v.toObject());

        if (pair.first && pair.second)
        {
            messages.append(pair.first);
            authors.append(pair.second);
        }
    }

    emit readyRead(messages, authors);
}

QPair<std::shared_ptr<Message>, std::shared_ptr<Author>> DLive::parseMessage(const QJsonObject &json)
{
    const QString typeName =  json.value("__typename").toString();
    const QString type = json.value("type").toString();

    if (typeName != "ChatText")
    {
        qWarning() << "unknown type name" << typeName << ", json =" << json;
        return QPair<std::shared_ptr<Message>, std::shared_ptr<Author>>();
    }

    if (type != "Message")
    {
        qWarning() << "unknown type" << type << ", json =" << json;
        return QPair<std::shared_ptr<Message>, std::shared_ptr<Author>>();
    }

    const QJsonObject sender = json.value("sender").toObject();
    const QString userName = sender.value("username").toString();
    const QString displayName = sender.value("displayname").toString();

    Author::Builder authorBuilder(getServiceType(), generateAuthorId(userName), displayName);
    authorBuilder.setAvatar(sender.value("avatar").toString());
    authorBuilder.setPage("https://dlive.tv/" + displayName);
    //TODO: badges

    auto author = authorBuilder.build();

    Message::Builder messageBuilder(author, generateMessageId(json.value("id").toString()));

    {
        QString raw = json.value("createdAt").toString();
        raw = raw.left(raw.length() - 6); // nanoseconds to milliseconds

        bool ok = false;
        const qint64 sinceEpoch = raw.toLongLong(&ok);
        if (ok)
        {
            const QDateTime dt = QDateTime::fromMSecsSinceEpoch(sinceEpoch);
            messageBuilder.setPublishedTime(dt);
        }
    }

    messageBuilder.addText(json.value("content").toString());

    //TODO: emotes

    return { messageBuilder.build(), author };
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
