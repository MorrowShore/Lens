#include "discord.h"
#include "secrets.h"
#include "models/message.h"
#include "crypto/obfuscator.h"
#include <QDesktopServices>
#include <QTcpSocket>
#include <QSysInfo>

namespace
{

static const int ReconncectPeriod = 3 * 1000;

static const int DispatchOpCode = 0;
static const int HeartbeatOpCode = 1;
static const int IdentifyOpCode = 2;
static const int InvalidSessionOpCode = 9;
static const int HelloOpCode = 10;
static const int HeartAckbeatOpCode = 11;

// https://discord.com/developers/docs/topics/gateway#gateway-intents
static const int INTENT_GUILD_MESSAGES = 1 << 9;
static const int INTENT_MESSAGE_CONTENT = 1 << 15;

static const QString ApiUrlPrefix = "https://discord.com/api/v10";

static const int MESSAGE_TYPE_DEFAULT = 0;
static const int MESSAGE_TYPE_RECIPIENT_ADD = 1;
static const int MESSAGE_TYPE_RECIPIENT_REMOVE = 2;
static const int MESSAGE_TYPE_CALL = 3;
static const int MESSAGE_TYPE_CHANNEL_NAME_CHANGE = 4;
static const int MESSAGE_TYPE_CHANNEL_ICON_CHANGE = 5;
static const int MESSAGE_TYPE_CHANNEL_PINNED_MESSAGE = 6;
static const int MESSAGE_TYPE_USER_JOIN = 7;
static const int MESSAGE_TYPE_GUILD_BOOST = 8;
static const int MESSAGE_TYPE_GUILD_BOOST_TIER_1 = 9;
static const int MESSAGE_TYPE_GUILD_BOOST_TIER_2 = 10;
static const int MESSAGE_TYPE_GUILD_BOOST_TIER_3 = 11;
static const int MESSAGE_TYPE_CHANNEL_FOLLOW_ADD = 12;
static const int MESSAGE_TYPE_GUILD_DISCOVERY_DISQUALIFIED = 14;
static const int MESSAGE_TYPE_GUILD_DISCOVERY_REQUALIFIED = 15;
static const int MESSAGE_TYPE_GUILD_DISCOVERY_GRACE_PERIOD_INITIAL_WARNING = 16;
static const int MESSAGE_TYPE_GUILD_DISCOVERY_GRACE_PERIOD_FINAL_WARNING = 17;
static const int MESSAGE_TYPE_THREAD_CREATED = 18;
static const int MESSAGE_TYPE_REPLY = 19;
static const int MESSAGE_TYPE_CHAT_INPUT_COMMAND = 20;
static const int MESSAGE_TYPE_THREAD_STARTER_MESSAGE = 21;
static const int MESSAGE_TYPE_GUILD_INVITE_REMINDER = 22;
static const int MESSAGE_TYPE_CONTEXT_MENU_COMMAND = 23;
static const int MESSAGE_TYPE_AUTO_MODERATION_ACTION = 24;
static const int MESSAGE_TYPE_ROLE_SUBSCRIPTION_PURCHASE = 25;
static const int MESSAGE_TYPE_INTERACTION_PREMIUM_UPSELL = 26;
static const int MESSAGE_TYPE_GUILD_APPLICATION_PREMIUM_SUBSCRIPTION = 32;

}

Discord::Discord(QSettings &settings, const QString &settingsGroupPath, QNetworkAccessManager &network_, QObject *parent)
    : ChatService(settings, settingsGroupPath, AxelChat::ServiceType::Discord, parent)
    , network(network_)
    , applicationId(settings, settingsGroupPath + "/client_id")
    , botToken(settings, settingsGroupPath + "/bot_token")
    , showNsfwChannels(settings, settingsGroupPath + "/show_nsfw_channels", false)
    , showGuildName(settings, settingsGroupPath + "/show_guild_name", true)
    , showChannelName(settings, settingsGroupPath + "/show_channel_name", true)
{
    getUIElementBridgeBySetting(stream)->setItemProperty("visible", false);

    authStateInfo = std::shared_ptr<UIElementBridge>(UIElementBridge::createLabel("Loading..."));
    addUIElement(authStateInfo);

    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLineEdit(&applicationId, tr("Application ID"), "0000000000000000000", true)));
    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLineEdit(&botToken, tr("Bot token"), "AbCdEfGhIjKlMnOpQrStUvWxYz.0123456789", true)));
    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLabel("1. " + tr("Create an app in the Discord Developer Portal"))));
    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createButton(tr("Open Discord Developer Portal"), []()
    {
        QDesktopServices::openUrl(QUrl("https://discord.com/developers"));
    })));
    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLabel("2. " + tr("Copy the Application ID and paste above"))));
    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLabel("3. " + tr("Create a bot (in Bot section)"))));
    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLabel("4. " + tr("Allow the bot to read the message content (Message Content Intent checkbox)"))));
    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLabel("5. " + tr("Reset the token (button Reset Token). The bot's previous token will become invalid"))));
    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLabel("6. " + tr("Copy the bot token and paste above") + ". <b><font color=\"red\">" + tr("DON'T DISCLOSE THE BOT'S TOKEN!") + "</b></font>")));
    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLabel("7. " + tr("Add the bot to the servers you need"))));
    connectBotToGuild = std::shared_ptr<UIElementBridge>(UIElementBridge::createButton(tr("Add bot to server"), [this]()
    {
        QDesktopServices::openUrl(QUrl("https://discord.com/api/oauth2/authorize"
                                       "?permissions=1024"
                                       "&scope=bot"
                                       "&client_id=" + applicationId.get().trimmed()));
    }));
    addUIElement(connectBotToGuild);
    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLabel(tr("To display private chats/channels, add the bot\n"
                                                                                  "to these chats/channels in access rights (at your own risk)"))));
    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createSwitch(&showNsfwChannels, tr("Show NSFW channels (at your own risk). Restart %1 if channel status is changed in Discord").arg(QCoreApplication::applicationName()))));
    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createSwitch(&showGuildName, tr("Show server name"))));
    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createSwitch(&showChannelName, tr("Show channel name"))));

    applicationId.setCallbackValueChanged([this](const QString&) { updateUI(); });
    botToken.setCallbackValueChanged([this](const QString&) { updateUI(); });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &Discord::onWebSocketReceived);

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state)
    {
        Q_UNUSED(state)
        //qDebug() << Q_FUNC_INFO << ": WebSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        //qDebug() << Q_FUNC_INFO << ": WebSocket connected";

        heartbeatAcknowledgementTimer.setInterval(60 * 1000);
        heartbeatAcknowledgementTimer.start();
    });

    QObject::connect(&socket, &QWebSocket::disconnected, this, [this]()
    {
        //qDebug() << Q_FUNC_INFO << ": WebSocket disconnected";
        processDisconnected();
    });

    QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError error_)
    {
        qDebug() << Q_FUNC_INFO << ": WebSocket error:" << error_ << ":" << socket.errorString();
    });

    QObject::connect(&heartbeatTimer, &QTimer::timeout, this, &Discord::sendHeartbeat);

    QObject::connect(&heartbeatAcknowledgementTimer, &QTimer::timeout, this, [this]()
    {
        if (socket.state() != QAbstractSocket::SocketState::ConnectedState)
        {
            heartbeatAcknowledgementTimer.stop();
            return;
        }

        qDebug() << Q_FUNC_INFO << "heartbeat acknowledgement timeout, disconnect";
        socket.close();
    });

    QObject::connect(&timerReconnect, &QTimer::timeout, this, [this]()
    {
        if (!enabled.get())
        {
            return;
        }

        if (socket.state() == QAbstractSocket::SocketState::ConnectedState ||
            socket.state() == QAbstractSocket::SocketState::ConnectingState)
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

    updateUI();
}

ChatService::ConnectionStateType Discord::getConnectionStateType() const
{
    if (state.connected)
    {
        return ChatService::ConnectionStateType::Connected;
    }
    else if (isCanConnect() && enabled.get())
    {
        return ChatService::ConnectionStateType::Connecting;
    }

    return ChatService::ConnectionStateType::NotConnected;
}

QString Discord::getStateDescription() const
{
    if (applicationId.get().isEmpty())
    {
        return tr("Application ID not specified");
    }

    if (botToken.get().isEmpty())
    {
        return tr("Bot token not specified");
    }

    switch (getConnectionStateType())
    {
    case ConnectionStateType::NotConnected:
        return tr("Not connected");

    case ConnectionStateType::Connecting:
        return tr("Connecting...");

    case ConnectionStateType::Connected:
        return tr("Successfully connected!");

    }

    return "<unknown_state>";
}

void Discord::onUiElementChangedImpl(const std::shared_ptr<UIElementBridge> element)
{
    if (!element)
    {
        qCritical() << Q_FUNC_INFO << "!element";
        return;
    }

    if (Setting<QString>* setting = element->getSettingString(); setting)
    {
        if (*&setting == &applicationId || *&setting == &botToken)
        {
            reconnect();
        }
    }

    if (Setting<bool>* setting = element->getSettingBool(); setting)
    {
        if (*&setting == &showNsfwChannels)
        {
            guilds.clear();
            channels.clear();
        }
    }
}

void Discord::reconnectImpl()
{
    socket.close();

    state = State();
    info = Info();

    processDisconnected();

    if (!isCanConnect() || !enabled.get())
    {
        return;
    }

    QNetworkReply* reply = network.get(createRequestAsBot(QUrl(ApiUrlPrefix + "/gateway/bot")));
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }
        reply->deleteLater();

        const QJsonObject root = QJsonDocument::fromJson(data).object();

        QString url = root.value("url").toString();
        if (url.isEmpty())
        {
            qCritical() << Q_FUNC_INFO << "url is empty";
        }
        else
        {
            if (url.back() != '/')
            {
                url += '/';
            }

            url += "?v=10&encoding=json";

            socket.setProxy(network.proxy());
            socket.open(QUrl(url));
        }
    });
}

void Discord::onWebSocketReceived(const QString &rawData)
{
    //qDebug("received:\n" + rawData.toUtf8() + "\n");

    if (!enabled.get())
    {
        return;
    }

    const QJsonObject root = QJsonDocument::fromJson(rawData.toUtf8()).object();
    if (root.contains("s"))
    {
        const QJsonValue s = root.value("s");
        if (!s.isNull())
        {
            info.lastSequence = s;
        }
    }

    const int opCode = root.value("op").toInt();

    if (opCode == DispatchOpCode)
    {
        parseDispatch(root.value("t").toString(), root.value("d").toObject());
    }
    else if (opCode == HeartbeatOpCode)
    {
        sendHeartbeat();
    }
    else if (opCode == InvalidSessionOpCode)
    {
        parseInvalidSession(root.value("d").toBool());
    }
    else if (opCode == HelloOpCode)
    {
        parseHello(root.value("d").toObject());
    }
    else if (opCode == HeartAckbeatOpCode)
    {
        heartbeatAcknowledgementTimer.setInterval(info.heartbeatInterval * 1.5);
        heartbeatAcknowledgementTimer.start();
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "unknown op code" << opCode;
    }
}

void Discord::sendHeartbeat()
{
    if (socket.state() != QAbstractSocket::SocketState::ConnectedState)
    {
        return;
    }

    send(HeartbeatOpCode, info.lastSequence);
}

void Discord::sendIdentify()
{
    if (!isCanConnect() || socket.state() != QAbstractSocket::SocketState::ConnectedState)
    {
        return;
    }

    const QJsonObject data =
    {
        { "token", botToken.get() },
        { "properties", QJsonObject(
          {
              { "os", QSysInfo::productType() },
              { "browser", QCoreApplication::applicationName() },
              { "device", QCoreApplication::applicationName() },
          })
        },

        { "intents", INTENT_GUILD_MESSAGES | INTENT_MESSAGE_CONTENT },
    };

    send(IdentifyOpCode, data);
}

QNetworkRequest Discord::createRequestAsBot(const QUrl &url) const
{
    if (!isCanConnect())
    {
        qWarning() << Q_FUNC_INFO << "can not connect";
        return QNetworkRequest();
    }

    QNetworkRequest request(url);

    request.setRawHeader("Authorization", ("Bot " + botToken.get().trimmed()).toUtf8());
    request.setRawHeader("User-Agent", ("DiscordBot (" + QCoreApplication::applicationName() + ", " + QCoreApplication::applicationVersion() + ")").toUtf8());

    return request;
}

bool Discord::checkReply(QNetworkReply *reply, const char *tag, QByteArray &resultData)
{
    resultData.clear();

    if (!reply)
    {
        qWarning() << tag << ": !reply";
        return false;
    }

    int statusCode = 200;
    const QVariant rawStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (rawStatusCode.isValid())
    {
        statusCode = rawStatusCode.toInt();
        if (statusCode != 200)
        {
            qWarning() << tag << ": status code:" << statusCode;
        }
    }

    resultData = reply->readAll();
    if (resultData.isEmpty() && statusCode != 200)
    {
        qWarning() << tag << ": data is empty";
        return false;
    }

    if (resultData.startsWith("error code:"))
    {
        qWarning() << tag << ":" << resultData;
        return false;
    }

    return true;
}

bool Discord::isCanConnect() const
{
    return !applicationId.get().isEmpty() && !botToken.get().isEmpty();
}

void Discord::processDisconnected()
{
    if (state.connected)
    {
        state.connected = false;
        emit stateChanged();
        emit connectedChanged(false, QString());
    }

    updateUI();
}

void Discord::processConnected()
{
    if (!state.connected)
    {
        state.connected = true;
        emit stateChanged();
        emit connectedChanged(true, QString());
    }

    updateUI();
}

void Discord::send(const int opCode, const QJsonValue &data)
{
    QJsonObject message =
    {
        { "op", opCode},
        { "d", data },
    };

    //qDebug("send:\n" + QJsonDocument(message).toJson() + "\n");

    socket.sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson()));
}

void Discord::parseHello(const QJsonObject &data)
{
    info.heartbeatInterval = data.value("heartbeat_interval").toInt(30000);
    if (info.heartbeatInterval < 1000)
    {
        info.heartbeatInterval = 1000;
    }

    heartbeatTimer.setInterval(info.heartbeatInterval);
    heartbeatTimer.start();

    sendHeartbeat();

    sendIdentify();
}

void Discord::parseDispatch(const QString &eventType, const QJsonObject &data)
{
    if (eventType == "READY")
    {
        processConnected();

        const QJsonObject jsonUser = data.value("user").toObject();
        info.userName = jsonUser.value("username").toString();
        info.userDiscriminator = jsonUser.value("discriminator").toString();

        updateUI();
    }
    else if (eventType == "MESSAGE_CREATE")
    {
        parseMessageCreate(data);
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "unkown event type" << eventType;
    }
}

void Discord::parseInvalidSession(const bool resumableSession)
{
    Q_UNUSED(resumableSession) //TODO
    reconnect();
}

void Discord::parseMessageCreate(const QJsonObject &jsonMessage)
{
    const int messageType = jsonMessage.value("type").toInt();
    if (messageType != MESSAGE_TYPE_DEFAULT)
    {
        qWarning() << Q_FUNC_INFO << "unknown message type" << messageType;
    }

    const QString guildId = jsonMessage.value("guild_id").toString();
    const QString channelId = jsonMessage.value("channel_id").toString();

    const QJsonObject jsonAuthor = jsonMessage.value("author").toObject();

    const QString userName = jsonAuthor.value("username").toString();
    const QString displayName = jsonAuthor.value("display_name").toString();
    const QString authorName = displayName.isEmpty() ? userName : displayName;
    const QString userId = jsonAuthor.value("id").toString();
    const QString avatarHash = jsonAuthor.value("avatar").toString();

    //TODO: timestamp
    //TODO: flags
    //TODO: etc

    static const int AvatarSize = 256;

    const Author author(getServiceType(),
                  authorName,
                  userId,
                  QUrl(QString("https://cdn.discordapp.com/avatars/%1/%2.png?size=%3").arg(userId, avatarHash).arg(AvatarSize)),
                  QUrl("https://discordapp.com/users/" + userId));

    const QString messageId = jsonMessage.value("id").toString();

    const QDateTime dateTime = QDateTime::fromString(jsonMessage.value("timestamp").toString(), Qt::ISODateWithMs).toLocalTime();

    QList<Message::Content*> contents;

    {
        //text
        contents.append(new Message::Text(jsonMessage.value("content").toString()));
    }

    {
        //embeds
        const QJsonArray embeds = jsonMessage.value("embeds").toArray();
        for (const QJsonValue& v : embeds)
        {
            const QList<Message::Content*> embedContents = parseEmbed(v.toObject());
            if (!embedContents.isEmpty())
            {
                if (!contents.isEmpty()) { contents.append(new Message::Text("\n")); }
            }

            for (Message::Content* embedContent : qAsConst(embedContents))
            {
                contents.append(embedContent);
            }
        }
    }

    {
        //attachments
        const QJsonArray attachments = jsonMessage.value("attachments").toArray();
        for (const QJsonValue& v : attachments)
        {
            const QList<Message::Content*> attachmentContents = parseAttachment(v.toObject());
            if (!attachmentContents.isEmpty())
            {
                if (!contents.isEmpty()) { contents.append(new Message::Text("\n")); }
            }

            for (Message::Content* embedContent : qAsConst(attachmentContents))
            {
                contents.append(embedContent);
            }
        }
    }

    {
        //stickers
        const QJsonArray stickerItems = jsonMessage.value("sticker_items").toArray();
        if (!stickerItems.isEmpty())
        {
            if (!contents.isEmpty()) { contents.append(new Message::Text("\n")); }

            Message::Text::Style style;
            style.italic = true;
            contents.append(new Message::Text("[" + tr("Sticker(s)") + "]", style));
        }
    }

    if (contents.isEmpty())
    {
        qWarning() << Q_FUNC_INFO << "contents is empty";
        return;
    }

    Message message(contents, author, dateTime, QDateTime::currentDateTime(), messageId);

    bool needDeffered = false;

    if (!guilds.contains(guildId))
    {
        needDeffered = true;
        requestGuild(guildId);
    }

    if (!channels.contains(channelId))
    {
        needDeffered = true;
        requestChannel(channelId);
    }

    if (needDeffered)
    {
        const QPair<QString, QString> key = { guildId, channelId };
        const QPair<Message, Author> value = { message, author };

        if (deferredMessages.contains(key))
        {
            deferredMessages[key].append(value);
        }
        else
        {
            deferredMessages.insert(key, { value });
        }

        requestedGuildsChannels.insert(guildId, channelId);
    }
    else
    {
        const Guild& guild = guilds[guildId];
        const Channel& channel = channels[channelId];

        message.setDestination(getDestination(guild, channel));

        if (isValidForShow(message, author, guild, channel))
        {
            QList<Message> messages({ message });
            QList<Author> authors({ author });

            emit readyRead(messages, authors);
        }
    }
}

void Discord::updateUI()
{
    connectBotToGuild->setItemProperty("enabled", isCanConnect());

    QString botStatus = tr("Bot status") + ": ";

    if (state.connected)
    {
        QString displayName = "<b>" + info.userName + "</b>";
        if (!info.userDiscriminator.isEmpty())
        {
            displayName += "#" + info.userDiscriminator;
        }

        botStatus += tr("authorized as %1").arg(displayName);

        authStateInfo->setItemProperty("text", "<img src=\"qrc:/resources/images/tick.svg\" width=\"20\" height=\"20\"> " + botStatus);
    }
    else
    {
        botStatus += tr("not authorized");
        authStateInfo->setItemProperty("text", "<img src=\"qrc:/resources/images/error-alt-svgrepo-com.svg\" width=\"20\" height=\"20\"> " + botStatus);
    }
}

void Discord::requestGuild(const QString &guildId)
{
    QNetworkReply* reply = network.get(createRequestAsBot(ApiUrlPrefix + "/guilds/" + guildId));
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }
        reply->deleteLater();

        const QJsonObject root = QJsonDocument::fromJson(data).object();

        Guild guild;

        guild.id = root.value("id").toString();
        guild.name = root.value("name").toString();

        guilds.insert(guild.id, guild);

        processDeferredMessages(guild.id, std::nullopt);
    });
}

void Discord::requestChannel(const QString &channelId)
{
    QNetworkReply* reply = network.get(createRequestAsBot(ApiUrlPrefix + "/channels/" + channelId));
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }
        reply->deleteLater();

        const QJsonObject root = QJsonDocument::fromJson(data).object();

        Channel channel;

        channel.id = root.value("id").toString();
        channel.name = root.value("name").toString();
        channel.nsfw = root.value("nsfw").toBool();

        channels.insert(channel.id, channel);

        processDeferredMessages(std::nullopt, channel.id);
    });
}

void Discord::processDeferredMessages(const std::optional<QString> &guildId_, const std::optional<QString> &channelId_)
{
    if (guildId_ && channelId_)
    {
        qWarning() << Q_FUNC_INFO << "guildId_ && channelId_";
        return;
    }

    QString guildId;
    QString channelId;

    if (guildId_)
    {
        guildId = *guildId_;

        if (requestedGuildsChannels.contains(*guildId_))
        {
            channelId = requestedGuildsChannels.value(*guildId_);
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "channel of guild not exists";
            return;
        }
    }
    else if (channelId_)
    {
        channelId = *channelId_;

        const QList<QString> values = requestedGuildsChannels.values();
        if (values.contains(*channelId_))
        {
            guildId = requestedGuildsChannels.key(*channelId_);
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "guild of channel not exists";
            return;
        }
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "!guildId_ && !channelId_";
        return;
    }

    if (guildId.isEmpty())
    {
        qWarning() << Q_FUNC_INFO << "guildId is empty";
        return;
    }

    if (channelId.isEmpty())
    {
        qWarning() << Q_FUNC_INFO << "channelId is empty";
        return;
    }

    if (!guilds.contains(guildId) || !channels.contains(channelId))
    {
        return;
    }

    requestedGuildsChannels.remove(guildId);

    const QPair<QString, QString> key = { guildId, channelId };
    const QList<QPair<Message, Author>> currentDeferredMessages = deferredMessages.value(key);
    deferredMessages.remove(key);

    const Guild& guild = guilds.value(guildId);
    const Channel& channel = channels.value(channelId);

    QList<Message> messages;
    QList<Author> authors;

    for (const QPair<Message, Author>& messageAuthor : qAsConst(currentDeferredMessages))
    {
        Message message = messageAuthor.first;
        message.setDestination(getDestination(guild, channel));

        const Author& author = messageAuthor.second;

        if (isValidForShow(message, author, guild, channel))
        {
            messages.append(message);
            authors.append(author);
        }
    }

    if (!messages.isEmpty() && !authors.isEmpty())
    {
        emit readyRead(messages, authors);
    }
}

QString Discord::getDestination(const Guild &guild, const Channel &channel) const
{
    QString result;

    if (showGuildName.get())
    {
        result += guild.name;
    }

    if (showChannelName.get())
    {
        if (!result.isEmpty())
        {
            result += " / ";
        }

        result += channel.name;
    }

    return result;
}

bool Discord::isValidForShow(const Message &message, const Author &author, const Guild &guild, const Channel &channel) const
{
    Q_UNUSED(message)
    Q_UNUSED(author)
    Q_UNUSED(guild)

    if (!showNsfwChannels.get() && channel.nsfw)
    {
        return false;
    }

    return true;
}

QString Discord::getEmbedTypeName(const QString& type)
{
    if (type == "rich") { return tr("generic"); }
    if (type == "image") { return tr("image"); }
    if (type == "video") { return tr("video"); }
    if (type == "gifv") { return tr("gif-animation"); }
    if (type == "article") { return tr("article"); }
    if (type == "link") { return tr("link"); }

    return tr("unknown \"%1\"").arg(type);
}

QList<Message::Content *> Discord::parseEmbed(const QJsonObject &jsonEmbed)
{
    QList<Message::Content *> contents;

    const QString type = jsonEmbed.value("type").toString();
    QString typeName = getEmbedTypeName(type);

    if (!typeName.isEmpty())
    {
        typeName.replace(0, 1, typeName.at(0).toUpper());
    }

    Message::Text::Style style;
    style.italic = true;

    if (type != "link")
    {
        const QString title = jsonEmbed.value("title").toString();
        const QString description = jsonEmbed.value("description").toString();

        if (!title.isEmpty())
        {
            if (!contents.isEmpty()) { contents.append(new Message::Text("\n")); }
            contents.append(new Message::Text(title, style));
        }

        if (!description.isEmpty())
        {
            if (!contents.isEmpty()) { contents.append(new Message::Text("\n")); }
            contents.append(new Message::Text(description, style));
        }
    }

    if (!contents.isEmpty()) { contents.append(new Message::Text("\n")); }
    contents.append(new Message::Text("[" + typeName + "]", style));

    return contents;
}

QList<Message::Content *> Discord::parseAttachment(const QJsonObject &jsonAttachment)
{
    QList<Message::Content *> contents;

    const QString fileName = jsonAttachment.value("filename").toString();

    Message::Text::Style style;
    style.italic = true;

    if (!fileName.isEmpty())
    {
        if (!contents.isEmpty()) { contents.append(new Message::Text("\n")); }
        contents.append(new Message::Text("[" + tr("File: %1").arg(fileName) + "]", style));
    }

    return contents;
}
