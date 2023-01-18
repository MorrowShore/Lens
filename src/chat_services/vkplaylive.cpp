#include "vkplaylive.h"
#include "models/message.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

VkPlayLive::VkPlayLive(QSettings& settings_, const QString& settingsGroupPath, QNetworkAccessManager& network_, QObject *parent)
    : ChatService(settings_, settingsGroupPath, AxelChat::ServiceType::VkPlayLive, parent)
    , settings(settings_)
    , network(network_)
    , socket("https://vkplay.live")
{
    getParameter(stream)->setPlaceholder(tr("Link or channel name..."));

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state)
    {
        Q_UNUSED(state)
        //qDebug() << Q_FUNC_INFO << ": WebSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &VkPlayLive::onWebSocketReceived);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        //qDebug() << Q_FUNC_INFO << ": WebSocket connected";

        if (state.connected)
        {
            state.connected = false;
            emit connectedChanged(false, lastConnectedChannelName);
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
            emit connectedChanged(false, lastConnectedChannelName);
        }
    });

    QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError error_)
    {
        qDebug() << Q_FUNC_INFO << ": WebSocket error:" << error_ << ":" << socket.errorString();
    });

    QObject::connect(&timerRequestToken, &QTimer::timeout, this, [this]()
    {
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

                    std::unique_ptr<QJsonObject> object;
                    int startPosition = 0;
                    do
                    {
                        object = AxelChat::findJsonObject(data, "websocket", startPosition, startPosition);
                        if (object)
                        {
                            info.token = object->value("token").toString();
                            if (!info.token.isEmpty())
                            {
                                break;
                            }
                        }
                    }
                    while(object);
                }

                // wsChannel
                {
                    std::unique_ptr<QJsonObject> object;
                    int startPosition = 0;
                    do
                    {
                        object = AxelChat::findJsonObject(data, "blog", startPosition, startPosition);
                        if (object)
                        {
                            QString publicWebSocketChannel = object->value("data").toObject().value("publicWebSocketChannel").toString();
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
                    while(object);
                }
            });
        }
    });
    timerRequestToken.setInterval(2000);
    timerRequestToken.start();

    reconnect();
}

ChatService::ConnectionStateType VkPlayLive::getConnectionStateType() const
{
    if (state.connected)
    {
        return ChatService::ConnectionStateType::Connected;
    }
    else if (!state.streamId.isEmpty())
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
            return tr("Stream not specified");
        }

        if (state.streamId.isEmpty())
        {
            return tr("The stream is not correct");
        }

        return tr("Not connected");

    case ConnectionStateType::Connecting:
        return tr("Connecting...");

    case ConnectionStateType::Connected:
        return tr("Successfully connected!");

    }

    return "<unknown_state>";
}

void VkPlayLive::reconnect()
{
    socket.close();

    state = State();
    info = Info();

    state.connected = false;

    state.streamId = extractChannelName(stream.get().trimmed());

    if (state.streamId.isEmpty())
    {
        emit stateChanged();
        return;
    }

    lastConnectedChannelName = state.streamId;

    state.chatUrl = QUrl(QString("https://vkplay.live/%1/only-chat").arg(state.streamId));
    state.streamUrl = QUrl(QString("https://vkplay.live/%1").arg(state.streamId));
    state.controlPanelUrl = QUrl(QString("https://vkplay.live/%1/studio").arg(state.streamId));

    emit stateChanged();
}

void VkPlayLive::onWebSocketReceived(const QString &rawData)
{
    //qDebug("\nreceived: " + rawData.toUtf8() + "\n");

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

            emit connectedChanged(true, state.streamId);
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
            {"params", params},
            {"id", info.lastMessageId},
        });

    if (method != -1)
    {
        object.insert("method", method);
    }

    send(QJsonDocument(object));
}

void VkPlayLive::parseMessage(const QJsonObject &data)
{
    QList<Message> messages;
    QList<Author> authors;

    const QJsonObject rawAuthor = data.value("author").toObject();

    const QString authorName = rawAuthor.value("displayName").toString();
    const QString authorAvatarUrl = rawAuthor.value("avatarUrl").toString();
    const QString authorId = rawAuthor.value("id").toString();

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

        if (type == "text")
        {
            const QString rawContent = data.value("content").toString();
            if (rawContent.isEmpty())
            {
                continue;
            }

            const QJsonArray rawContents = QJsonDocument::fromJson(rawContent.toUtf8()).array();
            if (rawContents.isEmpty())
            {
                continue;
            }

            const QString text = rawContents.at(0).toString();

            contents.append(new Message::Text(text));
        }
        else if (type == "mention")
        {
            const QString text = data.value("displayName").toString() + ", ";

            Message::Text::Style style;
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
        else
        {
            qDebug() << Q_FUNC_INFO << "unknown content type" << type << ", message:";

            qDebug("\n" + QJsonDocument(data).toJson() + "\n");
        }
    }

    Author author(getServiceType(),
                  authorName,
                  getServiceTypeId(serviceType) + QString("/%1").arg(authorId),
                  authorAvatarUrl,
                  QUrl("https://vkplay.live/" + authorName));

    Message message(contents, author, publishedAt, QDateTime::currentDateTime(), getServiceTypeId(serviceType) + QString("/%1").arg(messageId));

    messages.append(message);
    authors.append(author);

    if (!messages.isEmpty())
    {
        emit readyRead(messages, authors);
    }
}
