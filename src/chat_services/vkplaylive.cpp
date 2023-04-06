#include "vkplaylive.h"
#include "models/message.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace
{

static const int RequestStreamInterval = 20000;
static const int HeartbeatAcknowledgementTimeout = 40 * 1000;
static const int HeartbeatSendTimeout = 30 * 1000;
}

VkPlayLive::VkPlayLive(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network_, QObject *parent)
    : ChatService(settings, settingsGroupPath, AxelChat::ServiceType::VkPlayLive, parent)
    , network(network_)
    , socket("https://vkplay.live")
{
    getUIElementBridgeBySetting(stream)->setItemProperty("name", tr("Channel"));
    getUIElementBridgeBySetting(stream)->setItemProperty("placeholderText", tr("Link or channel name..."));

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state)
    {
        Q_UNUSED(state)
        //qDebug() << Q_FUNC_INFO << ": WebSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &VkPlayLive::onWebSocketReceived);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        //qDebug() << Q_FUNC_INFO << ": WebSocket connected";

        heartbeatAcknowledgementTimer.setInterval(HeartbeatAcknowledgementTimeout);
        heartbeatAcknowledgementTimer.start();

        if (state.connected)
        {
            state.connected = false;
            emit connectedChanged(false);
        }

        if (info.token.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "token is empty";
            return;
        }

        sendParams(QJsonObject(
                       {
                           {"token", info.token},
                           {"name", "js"}
                       }));

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

    QObject::connect(&timerRequestToken, &QTimer::timeout, this, [this]()
    {
        if (!enabled.get())
        {
            return;
        }

        if (!info.token.isEmpty())
        {
            if (socket.state() == QAbstractSocket::SocketState::UnconnectedState)
            {
                socket.setProxy(network.proxy());
                socket.open(QUrl("wss://pubsub.vkplay.live/connection/websocket"));
            }

            return;
        }

        if (state.chatUrl.isEmpty())
        {
            return;
        }

        QNetworkRequest request(state.chatUrl);
        QNetworkReply* reply = network.get(request);
        if (reply)
        {
            connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
                const QByteArray data = reply->readAll();
                reply->deleteLater();

                // token
                {
                    info.token = QString();

                    std::unique_ptr<QJsonDocument> json;
                    int startPosition = 0;
                    do
                    {
                        json = AxelChat::findJson(data, "websocket", QJsonValue::Type::Object, startPosition, startPosition);
                        if (json)
                        {
                            info.token = json->object().value("token").toString();
                            if (!info.token.isEmpty())
                            {
                                break;
                            }
                        }
                    }
                    while(json);
                }

                // wsChannel
                {
                    std::unique_ptr<QJsonDocument> json;
                    int startPosition = 0;
                    do
                    {
                        json = AxelChat::findJson(data, "blog", QJsonValue::Type::Object, startPosition, startPosition);
                        if (json)
                        {
                            QString publicWebSocketChannel = json->object().value("data").toObject().value("publicWebSocketChannel").toString();
                            if (!publicWebSocketChannel.isEmpty())
                            {
                                const QStringList parts = publicWebSocketChannel.split(':');
                                if (!parts.isEmpty())
                                {
                                    publicWebSocketChannel = "public-chat:" + parts.last();
                                }

                                info.wsChannel = publicWebSocketChannel;
                                break;
                            }
                        }
                    }
                    while(json);
                }

                parseStreamInfo(data);

                emit stateChanged();
            });
        }
    });
    timerRequestToken.setInterval(2000);
    timerRequestToken.start();

    QObject::connect(&timerRequestChatPage, &QTimer::timeout, this, [this]()
    {
        if (!enabled.get())
        {
            return;
        }

        QNetworkRequest request(state.chatUrl);
        QNetworkReply* reply = network.get(request);
        if (reply)
        {
            connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
                parseStreamInfo(reply->readAll());
                reply->deleteLater();
            });
        }
    });
    timerRequestChatPage.start(RequestStreamInterval);

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

    QObject::connect(&heartbeatTimer, &QTimer::timeout, this, &VkPlayLive::sendHeartbeat);
    heartbeatTimer.setInterval(HeartbeatSendTimeout);
    heartbeatTimer.start();

    reconnect();
}

ChatService::ConnectionStateType VkPlayLive::getConnectionStateType() const
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

QString VkPlayLive::getStateDescription() const
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

void VkPlayLive::reconnectImpl()
{
    socket.close();

    state = State();
    info = Info();

    state.streamId = extractChannelName(stream.get().trimmed());

    if (state.streamId.isEmpty())
    {
        emit stateChanged();
        return;
    }

    state.chatUrl = QUrl(QString("https://vkplay.live/%1/only-chat").arg(state.streamId));
    state.streamUrl = QUrl(QString("https://vkplay.live/%1").arg(state.streamId));
    state.controlPanelUrl = QUrl(QString("https://vkplay.live/%1/studio").arg(state.streamId));
}

void VkPlayLive::onWebSocketReceived(const QString &rawData)
{
    //qDebug("\nreceived: " + rawData.toUtf8() + "\n");

    if (!enabled.get())
    {
        return;
    }

    heartbeatAcknowledgementTimer.setInterval(HeartbeatAcknowledgementTimeout);
    heartbeatAcknowledgementTimer.start();

    const QJsonObject result = QJsonDocument::fromJson(rawData.toUtf8()).object().value("result").toObject();

    const QString version = result.value("version").toString();
    if (!version.isEmpty())
    {
        info.version = version;

        if (info.version != "3.2.3")
        {
            qWarning() << Q_FUNC_INFO << "unsupported version" << version;
        }

        if (info.wsChannel.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "ws channel is empty";
        }
        else
        {
            sendParams(QJsonObject({{"channel", info.wsChannel}}), 1);
        }

        if (!state.connected && !state.streamId.isEmpty() && !info.wsChannel.isEmpty() && !info.token.isEmpty())
        {
            state.connected = true;

            emit connectedChanged(true);
            emit stateChanged();
        }
    }
    else if (!result.isEmpty())
    {
        const QJsonObject data = result.value("data").toObject().value("data").toObject();
        const QString type = data.value("type").toString();

        if (type == "message")
        {
            parseMessage(data.value("data").toObject());
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "unknown receive type" << type;
        }
    }
}

QString VkPlayLive::extractChannelName(const QString &stream_)
{
    const QString stream = stream_.toLower().trimmed();

    if (stream.startsWith("http", Qt::CaseSensitivity::CaseInsensitive))
    {
        const QString simpleUrl = AxelChat::simplifyUrl(stream_);

        if (simpleUrl.startsWith("vkplay.live/", Qt::CaseSensitivity::CaseInsensitive))
        {
            QString result = simpleUrl.mid(12);
            if (result.contains("/"))
            {
                result = result.left(result.indexOf("/"));
            }

            if (!result.isEmpty())
            {
                return result;
            }
        }
    }

    const QRegExp rx("^[a-zA-Z0-9_\\-]+$", Qt::CaseInsensitive);
    if (rx.indexIn(stream) != -1)
    {
        return stream;
    }

    return QString();
}

void VkPlayLive::send(const QJsonDocument &data)
{
    //qDebug() << "send:" << data;
    socket.sendTextMessage(QString::fromUtf8(data.toJson(QJsonDocument::JsonFormat::Compact)));
}

void VkPlayLive::sendParams(const QJsonObject &params, int method)
{
    info.lastMessageId++;

    QJsonObject object(
        {
            { "params", params },
            { "id", info.lastMessageId },
        });

    if (method != -1)
    {
        object.insert("method", method);
    }

    send(QJsonDocument(object));
}

void VkPlayLive::sendHeartbeat()
{
    if (socket.state() != QAbstractSocket::SocketState::ConnectedState)
    {
        return;
    }

    static const int MethodHeartbeat = 7;

    info.lastMessageId++;

    send(QJsonDocument(QJsonObject(
                           {
                               { "method", MethodHeartbeat },
                               { "id", info.lastMessageId },
                           })));
}

void VkPlayLive::parseMessage(const QJsonObject &data)
{
    //qDebug("\n" + QJsonDocument(data).toJson() + "\n");

    QList<Message> messages;
    QList<Author> authors;

    QStringList leftBadges;

    const QJsonObject rawAuthor = data.value("author").toObject();

    const QString authorName = rawAuthor.value("displayName").toString();
    const QString authorAvatarUrl = rawAuthor.value("avatarUrl").toString();
    const QString authorId = QString("%1").arg(rawAuthor.value("id").toVariant().toLongLong());

    const QJsonArray badges = rawAuthor.value("badges").toArray();
    for (const QJsonValue& v : qAsConst(badges))
    {
        const QString url = v.toObject().value("smallUrl").toString().trimmed();
        if (!url.isEmpty())
        {
            leftBadges.append(url);
        }
    }

    QDateTime publishedAt;

    bool ok = false;
    const int64_t rawSendTime = data.value("createdAt").toVariant().toLongLong(&ok);
    if (ok)
    {
        publishedAt = QDateTime::fromSecsSinceEpoch(rawSendTime);
    }
    else
    {
        publishedAt = QDateTime::currentDateTime();
    }

    const QString messageId = QString("%1").arg(data.value("id").toVariant().toLongLong());

    QList<Message::Content*> contents;

    const QJsonArray messageData = data.value("data").toArray();
    for (const QJsonValue& value : qAsConst(messageData))
    {
        const QJsonObject data = value.toObject();
        const QString type = data.value("type").toString();
        const QString modificator = data.value("modificator").toString();
        if (modificator == "BLOCK_END")
        {
            continue;
        }

        if (type == "text")
        {
            const QString rawContent = data.value("content").toString();
            if (rawContent.isEmpty())
            {
                qWarning() << Q_FUNC_INFO << "text content string is empty:";
                qDebug("\n" + QJsonDocument(data).toJson() + "\n");
                continue;
            }

            const QJsonArray rawContents = QJsonDocument::fromJson(rawContent.toUtf8()).array();
            if (rawContents.isEmpty())
            {
                qWarning() << Q_FUNC_INFO << "text content array is empty:";
                qDebug("\n" + QJsonDocument(data).toJson() + "\n");
                continue;
            }

            const QString text = rawContents.at(0).toString();
            if (text.isEmpty())
            {
                qWarning() << Q_FUNC_INFO << "text text is empty:";
                qDebug("\n" + QJsonDocument(data).toJson() + "\n");
                continue;
            }

            contents.append(new Message::Text(text));
        }
        else if (type == "mention")
        {
            const QString text = data.value("displayName").toString() + ", ";
            if (text.isEmpty())
            {
                qWarning() << Q_FUNC_INFO << "display name is empty:";
                qDebug("\n" + QJsonDocument(data).toJson() + "\n");
                continue;
            }

            Message::TextStyle style;
            style.bold = true;
            contents.append(new Message::Text(text, style));
        }
        else if (type == "smile")
        {
            const QString url = data.value("smallUrl").toString();
            if (!url.isEmpty())
            {
                contents.append(new Message::Image(url));
            }
            else
            {
                qWarning() << Q_FUNC_INFO << "image url is empty:";
                qDebug("\n" + QJsonDocument(data).toJson() + "\n");
            }
        }
        else if (type == "link")
        {
            const QString rawContent = data.value("content").toString();
            if (rawContent.isEmpty())
            {
                qWarning() << Q_FUNC_INFO << "link content is empty:";
                qDebug("\n" + QJsonDocument(data).toJson() + "\n");
                continue;
            }

            const QJsonArray rawContents = QJsonDocument::fromJson(rawContent.toUtf8()).array();
            if (rawContents.isEmpty())
            {
                qWarning() << Q_FUNC_INFO << "link content string is empty:";
                qDebug("\n" + QJsonDocument(data).toJson() + "\n");
                continue;
            }

            const QString text = rawContents.at(0).toString();
            const QUrl url = data.value("url").toString();

            if (text.isEmpty())
            {
                qWarning() << Q_FUNC_INFO << "link text is empty:";
                qDebug("\n" + QJsonDocument(data).toJson() + "\n");
                continue;
            }

            if (url.isEmpty())
            {
                qWarning() << Q_FUNC_INFO << "link url is empty:";
                qDebug("\n" + QJsonDocument(data).toJson() + "\n");
                continue;
            }

            contents.append(new Message::Hyperlink(text, url));
        }
        else
        {
            qDebug() << Q_FUNC_INFO << "unknown content type" << type << ", message:";

            qDebug("\n" + QJsonDocument(data).toJson() + "\n");
        }
    }

    Author author(getServiceType(),
                  authorName,
                  authorId,
                  authorAvatarUrl,
                  QUrl("https://vkplay.live/" + authorName),
                  leftBadges);

    Message message(contents, author, publishedAt, QDateTime::currentDateTime(), getServiceTypeId(serviceType) + QString("/%1").arg(messageId));

    messages.append(message);
    authors.append(author);

    if (!messages.isEmpty())
    {
        emit readyRead(messages, authors);
    }
}

void VkPlayLive::parseStreamInfo(const QByteArray &data)
{
    if (info.wsChannel.isEmpty())
    {
        return;
    }

    std::unique_ptr<QJsonDocument> json;
    int startPosition = 0;
    do
    {
        json = AxelChat::findJson(data, "stream", QJsonValue::Type::Object, startPosition, startPosition);
        if (json)
        {
            const QJsonObject stream = json->object().value("data").toObject().value("stream").toObject();
            QString wsStreamViewersChannel = stream.value("wsStreamViewersChannel").toString();
            if (!wsStreamViewersChannel.isEmpty())
            {
                const QStringList parts = wsStreamViewersChannel.split(':');
                if (!parts.isEmpty())
                {
                    wsStreamViewersChannel = "public-chat:" + parts.last();
                }

                if (info.wsChannel == wsStreamViewersChannel)
                {
                    state.viewersCount = stream.value("count").toObject().value("viewers").toInt(-1);
                    break;
                }
            }
        }
    }
    while(json);

    emit stateChanged();
}
