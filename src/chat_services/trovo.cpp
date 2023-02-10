#include "trovo.h"
#include "secrets.h"
#include "models/message.h"
#include "models/author.h"
#include "crypto/obfuscator.h"
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

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

    const QJsonObject root = QJsonDocument::fromJson(resultData).object();
    if (root.contains("error"))
    {
        qWarning() << tag << "Error:" << resultData << ", request =" << reply->request().url().toString();
        return false;
    }

    return true;
}

static const int RequestChannelInfoPariod = 10000;
static const int PingPeriod = 30 * 1000;
static const int ReconncectPeriod = 3 * 1000;

static const QString NonceAuth = "AUTH";

static const QString ClientID = OBFUSCATE(TROVO_CLIENT_ID);

}

Trovo::Trovo(QSettings &settings, const QString &settingsGroupPath, QNetworkAccessManager &network_, QObject *parent)
    : ChatService(settings, settingsGroupPath, AxelChat::ServiceType::Trovo, parent)
    , network(network_)
{
    getUIElementBridgeBySetting(stream)->setItemProperty("name", tr("Channel"));
    getUIElementBridgeBySetting(stream)->setItemProperty("placeholderText", tr("Link or channel name..."));

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state)
    {
        Q_UNUSED(state)
        //qDebug() << Q_FUNC_INFO << ": WebSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &Trovo::onWebSocketReceived);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        if (state.connected)
        {
            state.connected = false;
            emit connectedChanged(false, lastConnectedChannelName);
        }

        QJsonObject root;
        root.insert("type", "AUTH");
        root.insert("nonce", NonceAuth);

        QJsonObject data;

        data.insert("token", oauthToken);

        root.insert("data", data);

        sendToWebSocket(QJsonDocument(root));

        ping();

        emit stateChanged();
    });

    QObject::connect(&socket, &QWebSocket::disconnected, this, [this]()
    {
        if (state.connected)
        {
            state.connected = false;

            emit stateChanged();
            emit connectedChanged(false, lastConnectedChannelName);
        }
    });

    QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError error_)
    {
        qDebug() << Q_FUNC_INFO << ": WebSocket error:" << error_ << ":" << socket.errorString();
    });

    timerPing.setInterval(PingPeriod);
    QObject::connect(&timerPing, &QTimer::timeout, this, &Trovo::ping);
    timerPing.start();

    QObject::connect(&timerReconnect, &QTimer::timeout, this, [this]()
    {
        if (socket.state() == QAbstractSocket::SocketState::UnconnectedState)
        {
            reconnect();
        }
    });
    timerReconnect.start(ReconncectPeriod);

    timerUpdateChannelInfo.setInterval(RequestChannelInfoPariod);
    connect(&timerUpdateChannelInfo, &QTimer::timeout, this, [this]()
    {
        if (!enabled.get() || !state.connected)
        {
            return;
        }

        requestChannelInfo();
    });
    timerUpdateChannelInfo.start();

    reconnect();
}

ChatService::ConnectionStateType Trovo::getConnectionStateType() const
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

QString Trovo::getStateDescription() const
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

void Trovo::reconnectImpl()
{
    oauthToken.clear();
    channelId.clear();

    socket.close();

    state = State();

    state.streamId = getChannelName(stream.get());

    state.controlPanelUrl = QUrl("https://studio.trovo.live/stream");

    if (!state.streamId.isEmpty())
    {
        state.streamUrl = QUrl("https://trovo.live/s/" + state.streamId);
        state.chatUrl = QUrl("https://trovo.live/chat/" + state.streamId);
    }

    if (enabled.get())
    {
        requestChannelId();
    }
}

void Trovo::onWebSocketReceived(const QString& rawData)
{
    //qDebug("RECIEVE: " + rawData.toUtf8() + "\n");

    if (!enabled.get())
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
            state.connected = true;
            lastConnectedChannelName = state.streamId;
            emit connectedChanged(true, state.streamId);
            emit stateChanged();

            requestChannelInfo();
        }
        else
        {
            qWarning() << Q_FUNC_INFO << ": unknown nonce" << nonce << ", raw data =" << rawData;
        }
    }
    else if (type == "CHAT")
    {
        QList<Message> messages;
        QList<Author> authors;

        const QJsonArray chats = data.value("chats").toArray();
        for (const QJsonValue& v : qAsConst(chats))
        {
            const QJsonObject jsonMessage = v.toObject();
            const int type = jsonMessage.value("type").toInt();
            const QString content = jsonMessage.value("content").toString().trimmed();
            const QString authorName = jsonMessage.value("nick_name").toString().trimmed();
            const QString avatar = jsonMessage.value("avatar").toString().trimmed();

            bool ok = false;
            const int64_t authorIdNum = jsonMessage.value("sender_id").toVariant().toLongLong(&ok);

            // https://developer.trovo.live/docs/Chat%20Service.html#_3-4-chat-message-types-and-samples

            if ((type >= 5 && type <= 9) || (type >= 5001 && type <= 5009) || (type >= 5012 && type <= 5013))
            {
                //TODO
                continue;
            }
            else if (type != 0)
            {
                qWarning() << Q_FUNC_INFO << ": unknown message type" << type << ", message =" << jsonMessage;
                continue;
            }

            if (content.isEmpty() || authorName.isEmpty() || authorIdNum == 0 || !ok)
            {
                qWarning() << Q_FUNC_INFO << ": ignore not valid message, message =" << jsonMessage;
                continue;
            }

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

            Author author(getServiceType(),
                          authorName,
                          getServiceTypeId(serviceType) + QString("/%1").arg(authorIdNum),
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

            QList<Message::Content*> contents;

            QStringList chunks;

            QString chunk;
            bool foundColon = false;
            for (const QChar& c : content)
            {
                if (c == ":")
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
                        if (SmilesValidSymbols.contains(c))
                        {
                            chunk += c;
                        }
                        else
                        {
                            if (!chunk.isEmpty())
                            {
                                chunks.append(chunk);
                                chunk.clear();
                            }

                            chunk += c;

                            foundColon = false;
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

            QString text;
            for (const QString& chunk : chunks)
            {
                if (chunk.startsWith(":"))
                {
                    const QString emote = AxelChat::removeFromStart(chunk, ":", Qt::CaseSensitivity::CaseInsensitive);
                    if (smiles.contains(emote))
                    {
                        if (!text.isEmpty())
                        {
                            contents.append(new Message::Text(text));
                            text = QString();
                        }

                        contents.append(new Message::Image(QUrl(smiles.value(emote)), 40));
                    }
                    else
                    {
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
                contents.append(new Message::Text(text));
            }

            Message message(contents, author, publishedAt, QDateTime::currentDateTime(), getServiceTypeId(serviceType) + QString("/%1").arg(messageId));

            messages.append(message);
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
        qWarning() << Q_FUNC_INFO << ": unknown message type" << type << ", raw data =" << rawData;
    }

    if (!error.isEmpty())
    {
        qWarning() << Q_FUNC_INFO << ": error" << error << ", raw data =" << rawData;
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
        channelName = AxelChat::simplifyUrl(channelName);
        channelName = AxelChat::removeFromStart(channelName, "trovo.live/s/", Qt::CaseSensitivity::CaseInsensitive);

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
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        const QJsonArray users = QJsonDocument::fromJson(data).object().value("users").toArray();
        if (users.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "users is empty";
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
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        const QString token = QJsonDocument::fromJson(data).object().value("token").toString().trimmed();
        if (token.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "token is empty";
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
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
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

        state.viewersCount = viewersCount;

        emit stateChanged();
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
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
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
