#include "trovo.h"
#include "secrets.h"
#include "utils/QtStringUtils.h"
#include "models/author.h"
#include "crypto/obfuscator.h"
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace
{

static const int RequestChannelInfoPariod = 10000;
static const int PingPeriod = 30 * 1000;

static const QString NonceAuth = "AUTH";

static const QString ClientID = OBFUSCATE(TROVO_CLIENT_ID);

static const int EmoteImageHeight = 40;

static const QColor HighlightedMessageColor = QColor(122, 229, 187);

static bool checkReply(QNetworkReply *reply, const char *tag, QByteArray& resultData)
{
    resultData.clear();

    if (!reply)
    {
        qWarning() << tag << ": !reply";
        return false;
    }

    resultData = reply->readAll();
    reply->deleteLater();
    if (resultData.isEmpty())
    {
        qWarning() << tag << ": data is empty";
        return false;
    }

    const QJsonObject root = QJsonDocument::fromJson(resultData).object();
    if (root.contains("error"))
    {
        qWarning() << tag << "Error:" << resultData << ", request =" << reply->request().url().toString();
        return false;
    }

    return true;
}

enum class ChatMessageType {
    /// Normal chat messages.
    Normal = 0,

    /// Spells, including: mana spells, elixir spells
    Spell = 5,

    /// Magic chat - super cap chat
    MagicSuperCap = 6,

    /// Magic chat - colorful chat
    MagicColorful = 7,

    /// Magic chat - spell chat
    MagicSpell = 8,

    /// Magic chat - bullet screen chat
    MagicBulletScreen = 9,

    /// TODO
    Todo19 = 19,

    /// Subscription message. Shows when someone subscribes to the channel.
    Subscription = 5001,

    /// System message.
    System = 5002,

    /// Follow message. Shows when someone follows the channel.
    Follow = 5003,

    /// Welcome message when viewer joins the channel.
    Welcome = 5004,

    /// Gift sub message. When a user randomly sends gift subscriptions to one or more users in the channel.
    GiftSub = 5005,

    /// Gift sub message. The detailed messages when a user sends a gift subscription to another user.
    GiftSubDetailed = 5006,

    /// Activity / events message. For platform level events.
    Event = 5007,

    /// Welcome message when users join the channel from raid.
    Raid = 5008,

    /// Custom Spells
    CustomSpell = 5009,
};

}

Trovo::Trovo(ChatManager& manager, QSettings &settings, const QString &settingsGroupPathParent, QNetworkAccessManager &network_, cweqt::Manager&, QObject *parent)
    : ChatService(manager, settings, settingsGroupPathParent, ChatServiceType::Trovo, false, parent)
    , network(network_)
{
    ui.findBySetting(stream)->setItemProperty("name", tr("Channel"));
    ui.findBySetting(stream)->setItemProperty("placeholderText", tr("Link or channel name..."));

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state)
    {
        Q_UNUSED(state)
        //qDebug() << "WebSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &Trovo::onWebSocketReceived);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        QJsonObject root;
        root.insert("type", "AUTH");
        root.insert("nonce", NonceAuth);

        QJsonObject data;

        data.insert("token", oauthToken);

        root.insert("data", data);

        sendToWebSocket(QJsonDocument(root));

        ping();
    });

    QObject::connect(&socket, &QWebSocket::disconnected, this, [this]()
    {
        reset();
    });

    QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError error_)
    {
        qDebug() << "WebSocket error:" << error_ << ":" << socket.errorString();
    });

    timerPing.setInterval(PingPeriod);
    QObject::connect(&timerPing, &QTimer::timeout, this, &Trovo::ping);
    timerPing.start();

    timerUpdateChannelInfo.setInterval(RequestChannelInfoPariod);
    QObject::connect(&timerUpdateChannelInfo, &QTimer::timeout, this, [this]()
    {
        if (!isEnabled() || !isConnected())
        {
            return;
        }

        requestChannelInfo();
    });
    timerUpdateChannelInfo.start();
}

ChatService::ConnectionState Trovo::getConnectionState() const
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

QString Trovo::getMainError() const
{
    if (state.streamId.isEmpty())
    {
        return tr("Channel not specified");
    }

    return tr("Not connected");
}

void Trovo::resetImpl()
{
    oauthToken.clear();
    channelId.clear();

    socket.close();

    state.streamId = getChannelName(stream.get());

    state.controlPanelUrl = QUrl("https://studio.trovo.live/stream");

    if (!state.streamId.isEmpty())
    {
        state.streamUrl = QUrl("https://trovo.live/s/" + state.streamId);
        state.chatUrl = QUrl("https://trovo.live/chat/" + state.streamId);
    }
}

void Trovo::connectImpl()
{
    requestChannelId();
}

void Trovo::onWebSocketReceived(const QString& rawData)
{
    //qDebug("RECIEVE: " + rawData.toUtf8() + "\n");

    if (!isEnabled())
    {
        return;
    }

    const QJsonObject root = QJsonDocument::fromJson(rawData.toUtf8()).object();

    const QString type = root.value("type").toString();
    const QString nonce = root.value("nonce").toString();
    const QString error = root.value("error").toString();
    const QJsonObject data = root.value("data").toObject();

    if (type == "RESPONSE")
    {
        if (nonce == NonceAuth)
        {
            setConnected();
            requestChannelInfo();
        }
        else
        {
            qWarning() << "unknown nonce" << nonce << ", raw data =" << rawData;
        }
    }
    else if (type == "CHAT")
    {
        QList<std::shared_ptr<Message>> messages;
        QList<std::shared_ptr<Author>> authors;

        const QJsonArray chats = data.value("chats").toArray();
        for (const QJsonValue& v : qAsConst(chats))
        {
            const QJsonObject jsonMessage = v.toObject();
            const int type = jsonMessage.value("type").toInt();
            const QJsonValue content = jsonMessage.value("content");
            const QJsonObject contentData = jsonMessage.value("content_data").toObject();

            const QString authorName = jsonMessage.value("nick_name").toString().trimmed();
            const QString avatar = jsonMessage.value("avatar").toString().trimmed();

            bool ok = false;
            const int64_t authorIdNum = jsonMessage.value("sender_id").toVariant().toLongLong(&ok);

            // https://developer.trovo.live/docs/Chat%20Service.html#_3-4-chat-message-types-and-samples

            /*if ((type >= 5 && type <= 9) || (type >= 5001 && type <= 5009) || (type >= 5012 && type <= 5013))
            {
                //TODO
                continue;
            }
            else if (type != 0)
            {
                qWarning() << "unknown message type" << type << ", message =" << jsonMessage;
                continue;
            }

            if (authorName.isEmpty() || authorIdNum == 0 || !ok)
            {
                qWarning() << "ignore not valid message, message =" << jsonMessage;
                continue;
            }*/

            QUrl avatarUrl;
            if (!avatar.isEmpty())
            {
                if (avatar.startsWith("http", Qt::CaseSensitivity::CaseInsensitive))
                {
                    avatarUrl = QUrl(avatar);
                }
                else
                {
                    avatarUrl = QUrl("https://headicon.trovo.live/user/" + avatar);
                }
            }

            std::shared_ptr<Author> author = std::make_shared<Author>(
                getServiceType(),
                authorName,
                generateAuthorId(QString("%1").arg(authorIdNum)),
                avatarUrl,
                QUrl("https://trovo.live/s/" + authorName));

            QDateTime publishedAt;

            ok = false;
            const int64_t rawSendTime = jsonMessage.value("send_time").toVariant().toLongLong(&ok);
            if (ok)
            {
                publishedAt = QDateTime::fromSecsSinceEpoch(rawSendTime);
            }
            else
            {
                publishedAt = QDateTime::currentDateTime();
            }

            const QString messageId = jsonMessage.value("message_id").toString().trimmed();

            Message::Builder messageBuilder(author, generateMessageId(messageId));

            messageBuilder.setPublishedTime(publishedAt);

            if (type == (int)ChatMessageType::Normal)
            {
                //qDebug() << jsonMessage;
                parseContentAsText(content, messageBuilder, contentData, false, false);
            }
            else if (
                type == (int)ChatMessageType::Subscription ||
                type == (int)ChatMessageType::Follow ||
                type == (int)ChatMessageType::Welcome
                )
            {
                parseContentAsText(content, messageBuilder, contentData, true, true);
            }
            else if (type == (int)ChatMessageType::Todo19)
            {
                //qDebug() << jsonMessage;
                //parseTodo19(content, messageBuilder);

                // TODO: {"avatar":"https://headicon.trovo.live/user/r26pqbiaaaaabjbf35s3bwq7cy.jpeg?ext=gif&t=31","content":"{\"id\":520004217,\"num\":1,\"price\":100}","content_data":{"ace_bullet":{"show_bullet":false,"status":false,"user_type":2},"combo_info":{"end":0,"id":"9PAki2XB","num":1,"t_combo":1,"t_send":1},"gift_price_info":{"currencyType":1,"number":100},"normal_emote_enabled":"","space_fans_ext":{"l":"19","s":"1"}},"custom_role":"[{\"roleName\":\"supermod\",\"roleType\":100005},{\"roleName\":\"subscriber\",\"roleType\":100004},{\"roleName\":\"Streamer Friends\",\"roleType\":200000},{\"roleName\":\"Kitty\",\"roleType\":200000},{\"roleName\":\"Captain\",\"roleType\":200000},{\"roleName\":\"Queens\",\"roleType\":200000},{\"roleName\":\"follower\",\"roleType\":100006}]","decos":["PKC_Normal"],"medals":["ace_knight","sub_L4_T1","editor","supermod","CustomRoleMedal.103326919.CAR-200000-2"],"message_id":"1697862915656548603_103326919_100187278_2887057889_1","nick_name":"Cubanees","roles":["supermod","subscriber","Streamer Friends","Kitty","Captain","Queens","follower"],"send_time":1697862915,"sender_id":100187278,"sub_lv":"L4","sub_tier":"1","type":19,"uid":100187278,"user_name":"Cubanees"}
                continue;
            }
            else if (type == (int)ChatMessageType::Event)
            {
                //qDebug() << jsonMessage;
                // TODO:
                continue;
            }
            else if (type == (int)ChatMessageType::Spell || type == (int)ChatMessageType::CustomSpell)
            {
                parseSpell(content, messageBuilder);
            }
            else
            {
                qWarning() << "unknown message type" << type << ", message =" << jsonMessage;

                if (content.isString())
                {
                    parseContentAsText(content, messageBuilder, contentData, true, true);
                }
                else
                {
                    parseContentAsText("[OBJECT]", messageBuilder, contentData, true, true);
                }
            }

            messages.append(messageBuilder.build());
            authors.append(author);
        }

        if (!messages.isEmpty())
        {
            emit readyRead(messages, authors);
        }
    }
    else if (type == "PONG")
    {
        //
    }
    else
    {
        qWarning() << "unknown message type" << type << ", raw data =" << rawData;
    }

    if (!error.isEmpty())
    {
        qWarning() << "error" << error << ", raw data =" << rawData;
    }
}

void Trovo::sendToWebSocket(const QJsonDocument &data)
{
    //qDebug("SEND:" + data.toJson(QJsonDocument::JsonFormat::Compact) + "\n");
    socket.sendTextMessage(QString::fromUtf8(data.toJson()));
}

void Trovo::ping()
{
    if (socket.state() == QAbstractSocket::SocketState::ConnectedState)
    {
        QJsonObject object;
        object.insert("type", "PING");
        object.insert("nonce", "ping");
        sendToWebSocket(QJsonDocument(object));
    }
}

QString Trovo::getChannelName(const QString &stream)
{
    QString channelName = stream.toLower().trimmed();

    if (channelName.startsWith("https://trovo.live/s/", Qt::CaseSensitivity::CaseInsensitive))
    {
        channelName = QtStringUtils::simplifyUrl(channelName);
        channelName = QtStringUtils::removeFromStart(channelName, "trovo.live/s/", Qt::CaseSensitivity::CaseInsensitive);

        if (channelName.contains("/"))
        {
            channelName = channelName.left(channelName.indexOf("/"));
        }
    }

    QRegExp rx("^[a-zA-Z0-9_\\-]+$", Qt::CaseInsensitive);
    if (rx.indexIn(channelName) == -1)
    {
        return QString();
    }

    return channelName;
}

void Trovo::requestChannelId()
{
    QNetworkRequest request(QUrl("https://open-api.trovo.live/openplatform/getusers"));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Content-type", "application/json");
    request.setRawHeader("Client-ID", ClientID.toUtf8());

    QJsonObject object;
    QJsonArray usersNames;
    usersNames.append(state.streamId);
    object.insert("user", usersNames);

    QNetworkReply* reply = network.post(request, QJsonDocument(object).toJson());
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        const QJsonArray users = QJsonDocument::fromJson(data).object().value("users").toArray();
        if (users.isEmpty())
        {
            qWarning() << "users is empty";
            return;
        }

        const QJsonObject user = users.first().toObject();
        const QString channelId_ = user.value("channel_id").toString().trimmed();
        if (channelId_.isEmpty() || channelId_ == "0")
        {
            return;
        }

        channelId = channelId_;

        requsetSmiles();
        requestChatToken();
    });
}

void Trovo::requestChatToken()
{
    QNetworkRequest request(QUrl("https://open-api.trovo.live/openplatform/chat/channel-token/" + channelId));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Client-ID", ClientID.toUtf8());
    QNetworkReply* reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        const QString token = QJsonDocument::fromJson(data).object().value("token").toString().trimmed();
        if (token.isEmpty())
        {
            qWarning() << "token is empty";
            return;
        }

        oauthToken = token;

        socket.setProxy(network.proxy());
        socket.open(QUrl("wss://open-chat.trovo.live/chat"));
    });
}

void Trovo::requestChannelInfo()
{
    QNetworkRequest request(QUrl("https://open-api.trovo.live/openplatform/channels/id"));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Content-type", "application/json");
    request.setRawHeader("Client-ID", ClientID.toUtf8());

    QJsonObject object;
    object.insert("channel_id", channelId);

    QNetworkReply* reply = network.post(request, QJsonDocument(object).toJson());
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        bool ok = false;
        const int64_t viewersCount = QJsonDocument::fromJson(data).object().value("current_viewers").toVariant().toLongLong(&ok);
        if (!ok)
        {
            return;
        }
        
        setViewers(viewersCount);
    });
}

void Trovo::requsetSmiles()
{
    QNetworkRequest request(QUrl("https://open-api.trovo.live/openplatform/getemotes"));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Content-type", "application/json");
    request.setRawHeader("Client-ID", ClientID.toUtf8());

    QJsonObject object;
    QJsonArray channelIds;
    channelIds.append(channelId);
    object.insert("channel_id", channelIds);
    object.insert("emote_type", 0);

    QNetworkReply* reply = network.post(request, QJsonDocument(object).toJson());
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        const QJsonObject channels = QJsonDocument::fromJson(data).object().value("channels").toObject();

        const QJsonArray channelsArray = channels.value("customizedEmotes").toObject().value("channel").toArray();
        if (!channelsArray.isEmpty())
        {
            const QJsonArray emotes = channelsArray.first().toObject().value("emotes").toArray();
            for (const QJsonValue& v : qAsConst(emotes))
            {
                const QJsonObject emote = v.toObject();
                const QString name = emote.value("name").toString();
                const QUrl url(emote.value("url").toString());

                if (name.isEmpty() || url.isEmpty())
                {
                    continue;
                }

                smiles.insert(name, url);
            }
        }

        const QJsonArray eventEmotes = channels.value("eventEmotes").toArray();
        for (const QJsonValue& v : qAsConst(eventEmotes))
        {
            const QJsonObject emote = v.toObject();
            const QString name = emote.value("name").toString();
            const QUrl url(emote.value("url").toString());

            if (name.isEmpty() || url.isEmpty())
            {
                continue;
            }

            smiles.insert(name, url);
        }

        const QJsonArray globalEmotes = channels.value("globalEmotes").toArray();
        for (const QJsonValue& v : qAsConst(globalEmotes))
        {
            const QJsonObject emote = v.toObject();
            const QString name = emote.value("name").toString();
            const QUrl url(emote.value("url").toString());

            if (name.isEmpty() || url.isEmpty())
            {
                continue;
            }

            smiles.insert(name, url);
        }
    });
}

void Trovo::parseContentAsText(const QJsonValue& jsonContent, Message::Builder& builder, const QJsonObject& contentData, const bool bold, const bool toUpperFirstChar) const
{
    Message::TextStyle style;
    style.bold = bold;

    const QString raw = jsonContent.toString();

    QStringList chunks;

    QString chunk;
    bool foundColon = false;
    for (const QChar& c : raw)
    {
        if (c == ':')
        {
            if (!chunk.isEmpty())
            {
                chunks.append(chunk);
                chunk.clear();
            }

            chunk += c;
            foundColon = true;
        }
        else
        {
            if (foundColon)
            {
                if (c == ' ')
                {
                    if (!chunk.isEmpty())
                    {
                        chunks.append(chunk);
                        chunk.clear();
                    }

                    chunk += c;

                    foundColon = false;
                }
                else
                {
                    chunk += c;
                }
            }
            else
            {
                chunk += c;
            }
        }
    }

    if  (!chunk.isEmpty())
    {
        chunks.append(chunk);
    }

    bool isFirstChunk = false;

    QString text;
    for (int i = 0; i < chunks.count(); i++)
    {
        const QString* prevChunk = i > 0 ? &chunks[i - 1] : nullptr;
        const QString& chunk = chunks[i];

        if (chunk.startsWith(':') && isEmote(chunk, prevChunk))
        {
            const QString emote = QtStringUtils::removeFromStart(chunk, ":", Qt::CaseSensitivity::CaseInsensitive);
            if (smiles.contains(emote))
            {
                if (!text.isEmpty())
                {
                    replaceWithData(text, contentData);

                    if (isFirstChunk && toUpperFirstChar)
                    {
                        QtStringUtils::toUpperFirstChar(text);
                    }

                    builder.addText(text, style);
                    text = QString();

                    isFirstChunk = false;
                }

                builder.addImage(QUrl(smiles.value(emote)), EmoteImageHeight);
            }
            else
            {
                qWarning() << "unknown emote" << chunk << ", raw =" << raw;
                text += chunk;
            }
        }
        else
        {
            text += chunk;
        }
    }

    if (!text.isEmpty())
    {
        if (isFirstChunk && toUpperFirstChar)
        {
            QtStringUtils::toUpperFirstChar(text);
        }

        replaceWithData(text, contentData);
        builder.addText(text, style);
    }
}

void Trovo::parseTodo19(const QJsonValue &jsonContent, Message::Builder& builder) const
{
    builder.setForcedColor(Message::ColorRole::BodyBackgroundColorRole, HighlightedMessageColor);

    Message::TextStyle style;
    style.bold = true;

    const QJsonObject root = QJsonDocument::fromJson(jsonContent.toString().toUtf8()).object();

    const int price = root.value("price").toInt();

    builder.addText(tr("Price: %1").arg(price), style);
}

void Trovo::parseSpell(const QJsonValue &jsonContent, Message::Builder &builder) const
{
    Message::TextStyle boldStyle;
    boldStyle.bold = true;

    const QJsonObject root = QJsonDocument::fromJson(jsonContent.toString().toUtf8()).object();

    const QString gift = root.value("gift").toString();
    const int num = root.value("num").toInt();
    const int giftValue = root.value("gift_value").toInt();
    const QString valueType = root.value("value_type").toString();

    QString valueName;

    if (valueType == "Elixir")
    {
        valueName = tr("Elixir");

        builder.setForcedColor(Message::ColorRole::BodyBackgroundColorRole, QColor(100, 105, 239));
    }
    else if (valueType == "Mana")
    {
        valueName = tr("Mana");

        builder.setForcedColor(Message::ColorRole::BodyBackgroundColorRole, QColor(122, 206, 226));
    }
    else
    {
        valueName = valueType;

        builder.setForcedColor(Message::ColorRole::BodyBackgroundColorRole, HighlightedMessageColor);

        qWarning() << "unknown value type" << valueType << ", content =" << jsonContent;
    }

    builder.addText(valueName + QString(": %1").arg(giftValue), boldStyle);

    if (num > 1)
    {
        builder.addText(QString(" x%1").arg(num), boldStyle);
    }

    builder.addText("\n");

    builder.addText("(" + gift + ")");
}

void Trovo::replaceWithData(QString &text, const QJsonObject &contentData)
{
    const QStringList keys = contentData.keys();
    for (const QString& key : keys)
    {
        QString value;

        const QJsonValue v = contentData.value(key);
        if (v.isString())
        {
            value = v.toString();
        }
        else if (v.isDouble())
        {
            value = QString("%1").arg(v.toDouble());
        }
        else
        {
            continue;
        }

        const QString keyChunk = "{" + key + "}";

        text.replace(keyChunk, value);
    }
}

bool Trovo::isEmote(const QString &chunk, const QString *prevChunk)
{
    if (chunk.isEmpty() || chunk == ":")
    {
        return false;
    }

    if (chunk.startsWith("://") && prevChunk && (
            prevChunk->endsWith("https", Qt::CaseSensitivity::CaseInsensitive) ||
            prevChunk->endsWith("http", Qt::CaseSensitivity::CaseInsensitive)
        ))
    {
        return false;
    }

    return true;
}
