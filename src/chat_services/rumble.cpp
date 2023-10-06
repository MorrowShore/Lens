#include "rumble.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRandomGenerator>

namespace
{

static const int RequestViewersInterval = 10000;
static const QColor DefaultHighlightedMessageColor = QColor(117, 94, 188);
static const int EmoteImageHeight = 40;

static bool checkReply(QNetworkReply *reply, const char *tag, QByteArray& resultData)
{
    resultData.clear();

    if (!reply)
    {
        qCritical() << tag << "reply is null";
        return false;
    }

    resultData = reply->readAll();
    reply->deleteLater();

    if (resultData.isEmpty())
    {
        qCritical() << tag << "data is empty";
        return false;
    }

    return true;
}

}

Rumble::Rumble(ChatHandler& manager, QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network_, cweqt::Manager&, QObject *parent)
    : ChatService(manager, settings, settingsGroupPathParent, AxelChat::ServiceType::Rumble, false, parent)
    , network(network_)
    , sse(network)
{
    ui.findBySetting(stream)->setItemProperty("name", tr("Stream"));
    ui.findBySetting(stream)->setItemProperty("placeholderText", tr("Stream link..."));

    QObject::connect(&sse, &SseManager::started, this, [this]()
    {
        //qDebug() << "SSE started";

        if (!isConnected() && !state.streamId.isEmpty() && isEnabled())
        {
            setConnected(true);
        }
    });

    QObject::connect(&sse, &SseManager::stopped, this, [this]()
    {
        //qDebug() << "SSE stopped";

        setConnected(false);

        if (isEnabled())
        {
            startReadChat();
        }
    });

    QObject::connect(&sse, &SseManager::readyRead, this, [this](const QByteArray& data)
    {
        //qDebug() << "SSE received some data";

        if (data == ":" || data == ": -1")
        {
            return;
        }

        QJsonParseError jsonError;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);
        if (jsonError.error != QJsonParseError::ParseError::NoError)
        {
            qCritical() << "json parse error =" << jsonError.errorString() << ", offset =" << jsonError.offset << "data:" << data;
        }

        if (doc.isObject())
        {
            onSseReceived(doc.object());
        }
        else
        {
            qCritical() << "document is not object";
        }
    });

    QObject::connect(&timerRequestViewers, &QTimer::timeout, this, [this]()
    {
        if (!isEnabled())
        {
            return;
        }

        requestViewers();
    });
    timerRequestViewers.start(RequestViewersInterval);
}

ChatService::ConnectionState Rumble::getConnectionState() const
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

QString Rumble::getMainError() const
{
    if (stream.get().isEmpty())
    {
        return tr("Channel not specified");
    }

    if (state.streamId.isEmpty())
    {
        return tr("The channel is not correct");
    }

    return tr("Not connected");
}

void Rumble::reconnectImpl()
{
    info = Info();
    sse.stop();

    state.streamId = extractLinkId(stream.get());

    if (state.streamId.isEmpty())
    {
        return;
    }

    state.streamUrl = QUrl("https://rumble.com/" + state.streamId + ".html");

    if (!isEnabled())
    {
        return;
    }

    info.viewerId = generateViewerId();

    requestVideoPage();
}

QString Rumble::generateViewerId()
{
    static const QString possibleCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
    static const int Length = 8;

    QRandomGenerator random;

    QString result;
    for (int i = 0; i< Length; ++i)
    {
        const int index = QRandomGenerator::global()->generate() % possibleCharacters.length();
        const QChar c = possibleCharacters.at(index);
        result.append(c);
    }

    return result;
}

QString Rumble::extractLinkId(const QString &rawLink)
{
    QString result = rawLink.trimmed();

    if (rawLink.startsWith("http", Qt::CaseSensitivity::CaseInsensitive) || rawLink.startsWith("rumble.com/", Qt::CaseSensitivity::CaseInsensitive))
    {
        result = AxelChat::simplifyUrl(rawLink);
    }

    result = AxelChat::removeFromStart(result, "rumble.com/", Qt::CaseSensitivity::CaseInsensitive);
    result = AxelChat::removeFromEnd(result, ".html", Qt::CaseSensitivity::CaseInsensitive);

    if (result.contains('/'))
    {
        return QString();
    }

    return result.trimmed();
}

void Rumble::requestVideoPage()
{
    if (!isEnabled() || state.streamUrl.isEmpty())
    {
        return;
    }

    QNetworkRequest request(state.streamUrl);
    QNetworkReply* reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        const QString chatId = parseChatId(data);
        if (chatId.isEmpty())
        {
            qCritical() << "failed to parse chat id";
        }
        else
        {
            info.chatId = chatId;
        }

        const QString videoId = parseVideoId(data);
        if (videoId.isEmpty())
        {
            qCritical() << "failed to parse video id";
        }
        else
        {
            info.videoId = videoId;
        }

        state.chatUrl = "https://rumble.com/chat/popup/" + info.chatId;
        emit stateChanged();

        requestEmotes();
        startReadChat();
        requestViewers();
    });
}

void Rumble::requestViewers()
{
    if (!isEnabled() || info.videoId.isEmpty())
    {
        return;
    }

    QNetworkRequest request(QUrl("https://wn0.rumble.com/service.php?name=video.watching-now"));

    request.setRawHeader("Content-type", "application/x-www-form-urlencoded");
\
    const QByteArray data = "video_id=" + info.chatId.toUtf8() + "&viewer_id=" + info.viewerId.toUtf8();

    QNetworkReply* reply = network.post(request, data);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            setConnected(false);
            return;
        }

        bool ok = false;
        const int64_t viewers = QJsonDocument::fromJson(data).object().value("data").toObject().value("viewer_count").toVariant().toLongLong(&ok);
        if (ok)
        {
            setViewers(viewers);
        }
    });
}

void Rumble::requestEmotes()
{
    if (!isEnabled() || info.chatId.isEmpty())
    {
        return;
    }

    QNetworkRequest request("https://rumble.com/service.php?name=emote.list&chat_id=" + info.chatId);
    QNetworkReply* reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        const QJsonArray items = QJsonDocument::fromJson(data).object().value("data").toObject().value("items").toArray();
        for (const QJsonValue& v : qAsConst(items))
        {
            const QJsonArray emotes = v.toObject().value("emotes").toArray();
            for (const QJsonValue& v : qAsConst(emotes))
            {
                const QJsonObject emote = v.toObject();
                const QString name = emote.value("name").toString();
                const QString url = emote.value("file").toString();
                emotesUrls.insert(name, url);
            }
        }
    });
}

QString Rumble::parseChatId(const QByteArray &html)
{
    return AxelChat::find(html, "data-id=\"", '"', 128);
}

QString Rumble::parseVideoId(const QByteArray &html)
{
    return AxelChat::find(html, "<link rel=alternate href=\"https://rumble.com/api/Media/oembed.json?url=https%3A%2F%2Frumble.com%2Fembed%2Fv", '"', 128);
}

void Rumble::startReadChat()
{
    if (!isEnabled() || info.chatId.isEmpty())
    {
        return;
    }

    sse.start("https://web7.rumble.com/chat/api/chat/" + info.chatId + "/stream");
}

void Rumble::onSseReceived(const QJsonObject &root)
{
    const QString type = root.value("type").toString();
    if (type == "init" || type == "messages")
    {
        const QJsonObject data = root.value("data").toObject();

        if (type == "init")
        {
            parseConfig(data.value("config").toObject());
        }

        const QJsonArray rawUsers = data.value("users").toArray();

        QMap<QString, QJsonObject> users;

        for (const QJsonValue& v : qAsConst(rawUsers))
        {
            const QJsonObject user = v.toObject();
            const QString id = user.value("id").toString();
            users.insert(id, user);
        }

        const QJsonArray messages = data.value("messages").toArray();
        for (const QJsonValue& v : qAsConst(messages))
        {
            const QJsonObject message = v.toObject();
            const QString userId = message.value("user_id").toString();
            if (!users.contains(userId))
            {
                qWarning() << "user id" << userId << "not found of message" << message;
                continue;
            }

            parseMessage(users.value(userId), message);
        }
    }
    else
    {
        qWarning() << "unknown SSE message type" << type;
    }
}

void Rumble::parseMessage(const QJsonObject &user, const QJsonObject &jsonMessage)
{
    //qDebug("==========================");
    //qDebug() << jsonMessage;
    //qDebug("==========================");

    const QString userId = user.value("id").toString();
    const QString userName = user.value("username").toString();
    const QColor userColor = user.value("color").toString();
    const QString userAvatar = user.value("image.1").toString();
    //const bool isFollower = user.value("is_follower").toBool();

    QStringList rightBadges;
    const QJsonArray jsonBadges = user.value("badges").toArray();
    for (const QJsonValue& v : qAsConst(jsonBadges))
    {
        const QString badgeName = v.toString();
        if (badgesUrls.contains(badgeName))
        {
            rightBadges.append(badgesUrls.value(badgeName));
        }
        else
        {
            qWarning() << "unknown badge" << badgeName;
            rightBadges.append(UnknownBadge);
        }
    }

    std::set<Message::Flag> flags;
    QHash<Message::ColorRole, QColor> forcedColors;

    const QString messageId = jsonMessage.value("id").toString();
    const QDateTime messagePublishedTime = QDateTime::fromString(jsonMessage.value("time").toString(), Qt::DateFormat::ISODate);
    const QDateTime messageReceivedTime = QDateTime::currentDateTime();

    QList<std::shared_ptr<Message>> messages;
    QList<std::shared_ptr<Author>> authors;

    std::shared_ptr<Author> author = std::make_shared<Author>(
        getServiceType(),
        userName,
        userId,
        userAvatar,
        "https://rumble.com/user/" + userName,
        QStringList(), rightBadges,
        std::set<Author::Flag>(),
        userColor);

    QList<std::shared_ptr<Message::Content>> contents;

    const QJsonObject rant = jsonMessage.value("rant").toObject();
    if (!rant.isEmpty())
    {
        bool ok = false;
        const int64_t priceCents = rant.value("price_cents").toVariant().toLongLong(&ok);
        if (ok)
        {
            QString text = "$";
            const int64_t dollars = priceCents / 100;
            const int cents = priceCents % 100;

            text += QString("%1").arg(dollars);
            if (cents > 0)
            {
                text += QString(".%1").arg(cents);
            }

            text += "\n";

            Message::TextStyle style;
            style.bold = true;
            contents.append(std::make_shared<Message::Text>(text, style));

            forcedColors.insert(Message::ColorRole::BodyBackgroundColorRole, getDonutColor(dollars));
        }
        else
        {
            qCritical() << "failed to get price, message =" << jsonMessage;
            forcedColors.insert(Message::ColorRole::BodyBackgroundColorRole, DefaultHighlightedMessageColor);
        }
    }

    const QJsonArray blocks = jsonMessage.value("blocks").toArray();
    for (const QJsonValue& v : qAsConst(blocks))
    {
        contents.append(parseBlock(v.toObject()));
    }

    std::shared_ptr<Message> message = std::make_shared<Message>(
        contents,
        author,
        messagePublishedTime,
        messageReceivedTime,
        getServiceTypeId(getServiceType()) + QString("/%1").arg(messageId),
        flags,
        forcedColors);

    messages.append(message);
    authors.append(author);

    if (!messages.isEmpty())
    {
        emit readyRead(messages, authors);
    }
}

void Rumble::parseConfig(const QJsonObject &config)
{
    badgesUrls.clear();
    donutColors.clear();

    const QJsonObject badges = config.value("badges").toObject();
    const QStringList badgesNames = badges.keys();
    for (const QString& badgeName : qAsConst(badgesNames))
    {
        QString url;

        const QJsonObject badge = badges.value(badgeName).toObject();
        const QJsonObject icons = badge.value("icons").toObject();
        if (icons.contains("48"))
        {
            url = icons.value("48").toString();
        }
        else
        {
            if (const QStringList keys = icons.keys(); !keys.isEmpty())
            {
                url = icons.value(keys.first()).toString();
            }
        }

        if (!url.isEmpty())
        {
            badgesUrls.insert(badgeName, "https://rumble.com/" + url);
        }
        else
        {
            qWarning() << "filed to find badge url for badge" << badgeName;
        }
    }

    const QJsonArray donutLevels = config.value("rants").toObject().value("levels").toArray();
    for (const QJsonValue& v : qAsConst(donutLevels))
    {
        const QJsonObject level = v.toObject();

        DonutColor donutColor;
        donutColor.color = level.value("colors").toObject().value("main").toString();
        donutColor.priceDollars = level.value("price_dollars").toVariant().toLongLong();
        donutColors.append(donutColor);
    }

    std::sort(donutColors.begin(), donutColors.end(), [](const DonutColor& a, const DonutColor& b)
    {
        return a.priceDollars < b.priceDollars;
    });
}

QColor Rumble::getDonutColor(const int64_t priceDollars) const
{
    if (donutColors.isEmpty())
    {
        qWarning() << "donut colors is empty";
        return DefaultHighlightedMessageColor;
    }

    QColor color = donutColors.first().color;

    for (const DonutColor& donutColor : qAsConst(donutColors))
    {
        if (priceDollars >= donutColor.priceDollars)
        {
            color = donutColor.color;
        }
        else
        {
            break;
        }
    }

    return color;
}

QList<std::shared_ptr<Message::Content>> Rumble::parseBlock(const QJsonObject &block) const
{
    QList<std::shared_ptr<Message::Content>> contents;

    const QJsonObject data = block.value("data").toObject();
    const QString type = block.value("type").toString();
    if (type == "text.1")
    {
        const QString rawText = data.value("text").toString();

        QString currentText;
        QString currentEmote;
        int emoteStartPos = -1;

        for (int i = 0; i < rawText.length(); i++)
        {
            const QChar c = rawText[i];

            if (emoteStartPos == -1)
            {
                if (c == ':')
                {
                    emoteStartPos = i + 1;
                    currentEmote = QString();

                    if (!currentText.isEmpty())
                    {
                        contents.append(std::make_shared<Message::Text>(currentText));
                        currentText = QString();
                    }
                }
                else
                {
                    currentText += c;
                }
            }
            else
            {
                if (c == ':')
                {
                    if (emotesUrls.contains(currentEmote))
                    {
                        contents.append(std::make_shared<Message::Image>(emotesUrls.value(currentEmote), EmoteImageHeight, true));
                    }
                    else
                    {
                        currentText = ":" + currentEmote + ":";
                        if (!currentEmote.isEmpty())
                        {
                            qWarning() << "emote" << currentEmote << "not found";
                        }
                    }

                    currentEmote = QString();
                    emoteStartPos = -1;
                }
                else
                {
                    currentEmote += c;
                }
            }
        }

        if (emoteStartPos != -1)
        {
            currentText = ":" + currentEmote;
        }

        if (!currentText.isEmpty())
        {
            contents.append(std::make_shared<Message::Text>(currentText));
        }
    }
    else
    {
        qWarning() << "unknown block type" << type;

    }

    return contents;
}
