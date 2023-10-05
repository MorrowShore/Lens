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

static const QString NonceAuth = "AUTH";

static const QString ClientID = OBFUSCATE(TROVO_CLIENT_ID);

static const int EmoteImageHeight = 40;

}

Trovo::Trovo(QSettings &settings, const QString &settingsGroupPathParent, QNetworkAccessManager &network_, cweqt::Manager&, QObject *parent)
    : ChatService(settings, settingsGroupPathParent, AxelChat::ServiceType::Trovo, false, parent)
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
        setConnected(false);
    });

    QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError error_)
    {
        qDebug() << "WebSocket error:" << error_ << ":" << socket.errorString();
    });

    timerPing.setInterval(PingPeriod);
    QObject::connect(&timerPing, &QTimer::timeout, this, &Trovo::ping);
    timerPing.start();

    timerUpdateChannelInfo.setInterval(RequestChannelInfoPariod);
    connect(&timerUpdateChannelInfo, &QTimer::timeout, this, [this]()
    {
        if (!enabled.get() || !isConnected())
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
    else if (enabled.get() && !state.streamId.isEmpty())
    {
        return ChatService::ConnectionState::Connecting;
    }
    
    return ChatService::ConnectionState::NotConnected;
}

QString Trovo::getStateDescription() const
{
    switch (getConnectionState())
    {
    case ConnectionState::NotConnected:
        if (state.streamId.isEmpty())
        {
            return tr("Channel not specified");
        }

        return tr("Not connected");
        
    case ConnectionState::Connecting:
        return tr("Connecting...");
        
    case ConnectionState::Connected:
        return tr("Successfully connected!");

    }

    return "<unknown_state>";
}

void Trovo::reconnectImpl()
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
            setConnected(true);
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
                qWarning() << "unknown message type" << type << ", message =" << jsonMessage;
                continue;
            }

            if (content.isEmpty() || authorName.isEmpty() || authorIdNum == 0 || !ok)
            {
                qWarning() << "ignore not valid message, message =" << jsonMessage;
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

            std::shared_ptr<Author> author = std::make_shared<Author>(
                getServiceType(),
                authorName,
                getServiceTypeId(getServiceType()) + QString("/%1").arg(authorIdNum),
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

            QList<std::shared_ptr<Message::Content>> contents;

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
                            contents.append(std::make_shared<Message::Text>(text));
                            text = QString();
                        }

                        contents.append(std::make_shared<Message::Image>(QUrl(smiles.value(emote)), EmoteImageHeight));
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
                contents.append(std::make_shared<Message::Text>(text));
            }

            std::shared_ptr<Message> message = std::make_shared<Message>(
                contents,
                author,
                publishedAt,
                QDateTime::currentDateTime(),
                getServiceTypeId(getServiceType()) + QString("/%1").arg(messageId));

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
