#include "rumble.h"
#include "models/message.h"
#include <QNetworkRequest>
#include <QNetworkReply>

namespace
{

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
    // https://wn0.rumble.com/service.php?video=2rhh6c&name=video.watching_now
    // https://rumble.com/chat/popup/167097396

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

        if (data == ":")
        {
            return;
        }

        QJsonParseError jsonError;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);
        if (jsonError.error != QJsonParseError::ParseError::NoError)
        {
            qWarning() << Q_FUNC_INFO << "json parse error =" << jsonError.errorString() << ", offset =" << jsonError.offset << "data:" << data;
        }

        if (!doc.isObject())
        {
            qWarning() << Q_FUNC_INFO << "document is not object";
        }

        onSseReceived(doc.object());
    });

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
            return;
        }

        info.chatId = chatId;
        state.chatUrl = "https://rumble.com/chat/popup/" + chatId;
        emit stateChanged();

        startReadChat();
    });
}

QString Rumble::parseChatId(const QByteArray &html)
{
    static const QByteArray Prefix = "class=\"rumbles-vote rumbles-vote-with-bar\" data-type=\"1\" data-id=\"";

    const int prefixStartPos = html.indexOf(Prefix);
    if (prefixStartPos == -1)
    {
        qWarning() << "failed to find prefix";
        return QString();
    }

    const int resultStartPos = prefixStartPos + Prefix.length();

    int resultLastPos = -1;
    for (int i = resultStartPos; i < std::min(html.length(), resultStartPos + 128); ++i)
    {
        const QChar& c = html[i];
        if (c == '"')
        {
            resultLastPos = i;
            break;
        }
    }

    if (resultLastPos == -1)
    {
        qDebug() << Q_FUNC_INFO << "not found '\"'";
        return QString();
    }

    return QString::fromUtf8(html.mid(resultStartPos, resultLastPos - resultStartPos));
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

    const QString messageId = jsonMessage.value("id").toString();
    const QDateTime messagePublishedTime = QDateTime::fromString(jsonMessage.value("time").toString(), Qt::DateFormat::ISODate);
    const QDateTime messageReceivedTime = QDateTime::currentDateTime();

    QList<Message> messages;
    QList<Author> authors;
    QList<Message::Content*> contents;

    Author author(getServiceType(),
                  userName,
                  userId,
                  userAvatar,
                  "https://rumble.com/user/" + userName,
                  {}, {}, {},
                  userColor);

    Message message(contents, author, messagePublishedTime, messageReceivedTime, getServiceTypeId(serviceType) + QString("/%1").arg(messageId));

    messages.append(message);
    authors.append(author);

    if (!messages.isEmpty())
    {
        emit readyRead(messages, authors);
    }
}
