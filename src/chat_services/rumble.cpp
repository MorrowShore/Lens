#include "rumble.h"
#include <QNetworkRequest>
#include <QNetworkReply>

namespace
{

static const int RequestViewersInterval = 10000;

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

    return true;
}

}

Rumble::Rumble(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network_, QObject *parent)
    : ChatService(settings, settingsGroupPath, AxelChat::ServiceType::Rumble, parent)
    , network(network_)
    , sse(network)
{
    getUIElementBridgeBySetting(stream)->setItemProperty("name", tr("Stream"));
    getUIElementBridgeBySetting(stream)->setItemProperty("placeholderText", tr("Stream link..."));

    QObject::connect(&sse, &SseManager::started, this, [this]()
    {
        //qDebug() << Q_FUNC_INFO << "SSE started";

        if (!state.connected && !state.streamId.isEmpty() && enabled.get())
        {
            state.connected = true;
            emit connectedChanged(true);
            emit stateChanged();
        }
    });

    QObject::connect(&sse, &SseManager::stopped, this, [this]()
    {
        //qDebug() << Q_FUNC_INFO << "SSE stopped";

        state.connected = false;
        emit connectedChanged(false);
        emit stateChanged();

        if (enabled.get())
        {
            startReadChat();
        }
    });

    QObject::connect(&sse, &SseManager::readyRead, this, [this](const QByteArray& data)
    {
        //qDebug() << Q_FUNC_INFO << "SSE received some data";

        if (data == ":" || data == ": -1")
        {
            return;
        }

        QJsonParseError jsonError;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);
        if (jsonError.error != QJsonParseError::ParseError::NoError)
        {
            qWarning() << Q_FUNC_INFO << "json parse error =" << jsonError.errorString() << ", offset =" << jsonError.offset << "data:" << data;
        }

        if (doc.isObject())
        {
            onSseReceived(doc.object());
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "document is not object";
        }
    });

    QObject::connect(&timerRequestViewers, &QTimer::timeout, this, [this]()
    {
        if (!enabled.get())
        {
            return;
        }

        requestViewers();
    });
    timerRequestViewers.start(RequestViewersInterval);

    QObject::connect(&timerReconnect, &QTimer::timeout, this, [this]()
    {
        if (!enabled.get() || state.connected)
        {
            return;
        }

        reconnect();
    });
    timerReconnect.start(5000);

    reconnect();
}

ChatService::ConnectionStateType Rumble::getConnectionStateType() const
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

QString Rumble::getStateDescription() const
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

void Rumble::reconnectImpl()
{
    state = State();
    info = Info();
    sse.stop();

    state.streamId = extractLinkId(stream.get());

    if (state.streamId.isEmpty())
    {
        emit stateChanged();
        return;
    }

    state.streamUrl = QUrl("https://rumble.com/" + state.streamId + ".html");

    if (!enabled.get())
    {
        return;
    }

    requestVideoPage();
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
    if (!enabled.get() || state.streamUrl.isEmpty())
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
            qWarning() << Q_FUNC_INFO << "failed to parse chat id";
        }
        else
        {
            info.chatId = chatId;
        }

        const QString videoId = parseVideoId(data);
        if (videoId.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "failed to parse video id";
        }
        else
        {
            info.videoId = videoId;
        }

        state.chatUrl = "https://rumble.com/chat/popup/" + info.chatId;
        emit stateChanged();

        startReadChat();
        requestViewers();
    });
}

void Rumble::requestViewers()
{
    if (!enabled.get() || info.videoId.isEmpty())
    {
        return;
    }

    QNetworkRequest request("https://wn0.rumble.com/service.php?name=video.watching_now&video=" + info.videoId);
    QNetworkReply* reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            if (state.connected)
            {
                state.connected = false;
                emit connectedChanged(false);
                emit stateChanged();
            }

            return;
        }

        bool ok = false;
        const int64_t viewers = QJsonDocument::fromJson(data).object().value("data").toObject().value("viewer_count").toVariant().toLongLong(&ok);
        if (ok)
        {
            state.viewersCount = viewers;
            emit stateChanged();
        }
    });
}

QString Rumble::parseChatId(const QByteArray &html)
{
    return AxelChat::find(html, "class=\"rumbles-vote rumbles-vote-with-bar\" data-type=\"1\" data-id=\"", '"', 128);
}

QString Rumble::parseVideoId(const QByteArray &html)
{
    return AxelChat::find(html, "<link rel=alternate href=\"https://rumble.com/api/Media/oembed.json?url=https%3A%2F%2Frumble.com%2Fembed%2Fv", '"', 128);
}

void Rumble::startReadChat()
{
    if (!enabled.get() || info.chatId.isEmpty())
    {
        return;
    }

    sse.start("https://web7.rumble.com/chat/api/chat/" + info.chatId + "/stream");
}

void Rumble::onSseReceived(const QJsonObject &root)
{
    qDebug() << root;

    const QString type = root.value("type").toString();
    if (type == "init" || type == "messages")
    {
        const QJsonObject data = root.value("data").toObject();
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
                qWarning() << Q_FUNC_INFO << "user id" << userId << "not found of message" << message;
                continue;
            }

            parseMessage(users.value(userId), message);
        }
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "unknown SSE message type" << type;
    }
}

void Rumble::parseMessage(const QJsonObject &user, const QJsonObject &jsonMessage)
{
    const QString userId = user.value("id").toString();
    const QString userName = user.value("username").toString();
    const QColor userColor = user.value("color").toString();
    const QString userAvatar = user.value("image.1").toString();
    //const bool isFollower = user.value("is_follower").toBool();

    std::set<Message::Flag> flags;
    QHash<Message::ColorRole, QColor> forcedColors;

    const QString messageId = jsonMessage.value("id").toString();
    const QDateTime messagePublishedTime = QDateTime::fromString(jsonMessage.value("time").toString(), Qt::DateFormat::ISODate);
    const QDateTime messageReceivedTime = QDateTime::currentDateTime();

    QList<Message> messages;
    QList<Author> authors;

    Author author(getServiceType(),
                  userName,
                  userId,
                  userAvatar,
                  "https://rumble.com/user/" + userName,
                  {}, {}, {},
                  userColor);

    QList<Message::Content *> contents;

    const QJsonObject rant = jsonMessage.value("rant").toObject();
    if (!rant.isEmpty())
    {
        forcedColors.insert(Message::ColorRole::BodyBackground, QColor(117, 94, 188));

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
            contents.append(new Message::Text(text, style));
        }
    }

    const QJsonArray blocks = jsonMessage.value("blocks").toArray();
    for (const QJsonValue& v : qAsConst(blocks))
    {
        Message::Content* content = parseBlock(v.toObject());
        if (content)
        {
            contents.append(content);
        }
    }

    Message message(contents,
                    author,
                    messagePublishedTime,
                    messageReceivedTime,
                    getServiceTypeId(serviceType) + QString("/%1").arg(messageId),
                    flags,
                    forcedColors);

    messages.append(message);
    authors.append(author);

    if (!messages.isEmpty())
    {
        emit readyRead(messages, authors);
    }
}

Message::Content *Rumble::parseBlock(const QJsonObject &block)
{
    const QJsonObject data = block.value("data").toObject();
    const QString type = block.value("type").toString();
    if (type == "text.1")
    {
        const QString text = data.value("text").toString();

        return new Message::Text(text);
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "unknown block type" << type;
        return nullptr;
    }
}
