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

static const int HeartbeatAcknowledgementTimeout = 40 * 1000;
static const int HeartbeatSendTimeout = 30 * 1000;
}

VkPlayLive::VkPlayLive(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network_, cweqt::Manager&, QObject *parent)
    : ChatService(settings, settingsGroupPathParent, AxelChat::ServiceType::VkPlayLive, false, parent)
    , network(network_)
    , socket("https://vkplay.live")
{
    ui.findBySetting(stream)->setItemProperty("name", tr("Channel"));
    ui.findBySetting(stream)->setItemProperty("placeholderText", tr("Link or channel name..."));

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state)
    {
        Q_UNUSED(state)
        //qDebug() << "WebSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &VkPlayLive::onWebSocketReceived);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        //qDebug() << "WebSocket connected";

        heartbeatAcknowledgementTimer.setInterval(HeartbeatAcknowledgementTimeout);
        heartbeatAcknowledgementTimer.start();

        if (state.connected)
        {
            state.connected = false;
            emit connectedChanged(false);
        }

        if (info.token.isEmpty())
        {
            qWarning() << "token is empty";
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
        //qDebug() << "WebSocket disconnected";

        if (state.connected)
        {
            state.connected = false;
            emit stateChanged();
            emit connectedChanged(false);
        }
    });

    QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError error_)
    {
        qDebug() << "WebSocket error:" << error_ << ":" << socket.errorString();
    });

    QObject::connect(&timerRequestToken, &QTimer::timeout, this, [this]()
    {
        if (!enabled.get())
        {
            return;
        }

        if (!info.token.isEmpty() && !info.wsChannel.isEmpty())
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

        {
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

                    emit stateChanged();
                });
            }
        }

        {
            QNetworkRequest request("https://api.vkplay.live/v1/blog/" + state.streamId.toLower());
            QNetworkReply* reply = network.get(request);
            if (reply)
            {
                connect(reply, &QNetworkReply::finished, this, [this, reply]()
                {
                    const QByteArray data = reply->readAll();
                    reply->deleteLater();

                    const QString raw = QJsonDocument::fromJson(data).object().value("publicWebSocketChannel").toString();
                    info.wsChannel = raw.mid(raw.indexOf(':') + 1);

                    emit stateChanged();
                });
            }
        }
    });
    timerRequestToken.setInterval(2000);
    timerRequestToken.start();

    QObject::connect(&heartbeatAcknowledgementTimer, &QTimer::timeout, this, [this]()
    {
        if (socket.state() != QAbstractSocket::SocketState::ConnectedState)
        {
            heartbeatAcknowledgementTimer.stop();
            return;
        }

        qDebug() << "heartbeat acknowledgement timeout, disconnect";
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

        if (!state.connected && !state.streamId.isEmpty() && !info.wsChannel.isEmpty() && !info.token.isEmpty())
        {
            state.connected = true;

            emit connectedChanged(true);
            emit stateChanged();
        }

        if (info.version != "3.2.3")
        {
            qWarning() << "unsupported version" << version;
        }

        if (info.wsChannel.isEmpty())
        {
            qWarning() << "ws channel is empty";
        }
        else
        {
            sendParams(QJsonObject({{"channel", "public-stream:" + info.wsChannel}}), 1);
            sendParams(QJsonObject({{"channel", "public-viewers:" + info.wsChannel}}), 1);
            sendParams(QJsonObject({{"channel", "public-chat:" + info.wsChannel}}), 1);
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
        else if (type == "delete_message")
        {
            const QString messageId = generateMessageId(QString("%1").arg(data.value("id").toVariant().toLongLong()));
            auto deleter = Message::Builder::createDeleter(getServiceType(), messageId);

            emit readyRead({ deleter.second }, { deleter.first });
        }
        else if (type == "stream_online_status")
        {
            if (data.contains("viewers"))
            {
                state.viewersCount = data.value("viewers").toInt(-1);
                emit stateChanged();
            }
            else
            {
                qWarning() << "viewers not found in, result =" << result;
            }
        }
        else if (type == "stream_like_counter")
        {
            // TODO: result = QJsonObject({"channel":"public-stream:6639759","data":{"data":{"counter":331,"type":"stream_like_counter","userId":0},"offset":38493}}
        }
        else if (type == "prediction")
        {
            // TODO: result = QJsonObject({"channel":"public-stream:12281640","data":{"data":{"data":{"createdAt":1696271132,"decisions":[{"biggestWinAmount":0,"id":32977,"isWinner":false,"name":"0-2000","order":0,"pointsBank":0,"proportion":0,"userAmount":0},{"biggestWinAmount":0,"id":32978,"isWinner":false,"name":"2001-3000","order":1,"pointsBank":0,"proportion":0,"userAmount":0},{"biggestWinAmount":0,"id":32979,"isWinner":false,"name":"3001-4000","order":2,"pointsBank":0,"proportion":0,"userAmount":0},{"biggestWinAmount":0,"id":32980,"isWinner":false,"name":"4001-5000","order":3,"pointsBank":0,"proportion":0,"userAmount":0},{"biggestWinAmount":0,"id":32981,"isWinner":false,"name":"5001+","order":4,"pointsBank":0,"proportion":0,"userAmount":0}],"duration":60,"id":8698,"pointsBank":0,"state":"active","title":"Сколько Джов настреляет в этом бою?"},"type":"prediction"},"offset":189133}})
        }
        else if (type == "chat_pinned_message_reaction")
        {
            // TODO: {"channel":"public-chat:16569531","data":{"data":{"id":12200,"reactions":[{"count":119,"type":"like"},{"count":4,"type":"fire"},{"count":6,"type":"laught"},{"count":28,"type":"poo"},{"count":33,"type":"clown"}],"type":"chat_pinned_message_reaction"},"offset":263370}}
        }
        else
        {
            qWarning() << "unknown receive type" << type << ", result =" << result;
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
    //qDebug() << data;

    const QJsonObject rawAuthor = data.value("author").toObject();

    const QString authorName = rawAuthor.value("displayName").toString();
    const QString authorId = generateAuthorId(QString("%1").arg(rawAuthor.value("id").toVariant().toLongLong()));

    Author::Builder authorBuilder(getServiceType(), authorId, authorName);

    authorBuilder.setAvatar(rawAuthor.value("avatarUrl").toString());

    const QJsonArray badges = rawAuthor.value("badges").toArray();
    for (const QJsonValue& v : qAsConst(badges))
    {
        const QString url = v.toObject().value("smallUrl").toString().trimmed();
        if (!url.isEmpty())
        {
            authorBuilder.addLeftBadge(url);
        }
    }

    authorBuilder.setPage(rawAuthor.value("vkplayProfileLink").toString());

    const bool isFirstMessage = data.value("flags").toObject().value("isFirstMessage").toBool();

    const QJsonArray roles = rawAuthor.value("roles").toArray();
    for (const QJsonValue& v : qAsConst(roles))
    {
        const int priority = v.toObject().value("priority").toInt(-1);//TODO
        Q_UNUSED(priority)

        const QString url = v.toObject().value("smallUrl").toString().trimmed();
        if (!url.isEmpty())
        {
            authorBuilder.addLeftBadge(url);
            break;//TODO
        }
    }

    const int colorIndex = rawAuthor.value("nickColor").toInt();

    static const QMap<int, QColor> Colors =
    {
        { 0,    QColor(214, 110, 52 ) },
        { 1,    QColor(184, 170, 255) },
        { 2,    QColor(0,   87,  159) },
        { 3,    QColor(139, 63,  253) },
        { 4,    QColor(89,  168, 64 ) },
        { 5,    QColor(212, 81,  36 ) },
        { 6,    QColor(222, 100, 137) },
        { 7,    QColor(32,  187, 161) },
        { 8,    QColor(248, 179, 1  ) },
        { 9,    QColor(0,   153, 187) },
        { 10,   QColor(123, 190, 255) },
        { 11,   QColor(229, 66,  255) },
        { 12,   QColor(163, 108, 89 ) },
        { 13,   QColor(139, 162, 89 ) },
        { 14,   QColor(0,   169, 255) },
        { 15,   QColor(162, 11,  255) },
    };

    if (Colors.contains(colorIndex))
    {
        authorBuilder.setCustomNicknameColor(Colors.value(colorIndex));
    }
    else
    {
        qWarning() << "unknown color index" << colorIndex << ", author name =" << authorName;
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

    const QString messageId = generateMessageId(QString("%1").arg(data.value("id").toVariant().toLongLong()));

    const auto author = authorBuilder.build();

    Message::Builder messageBuilder(author, messageId);
    messageBuilder.setPublishedTime(publishedAt);

    if (isFirstMessage)
    {
        Message::TextStyle style;
        style.bold = true;
        messageBuilder.addText(tr("New chat member") + "\n\n", style);

        messageBuilder.setForcedColor(Message::ColorRole::BodyBackgroundColorRole, QColor(22, 167, 255));
    }

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
                qWarning() << "text content string is empty:" << QJsonDocument(data).toJson();
                continue;
            }

            const QJsonArray rawContents = QJsonDocument::fromJson(rawContent.toUtf8()).array();
            if (rawContents.isEmpty())
            {
                qWarning() << "text content array is empty:" << QJsonDocument(data).toJson();
                continue;
            }

            const QString text = rawContents.at(0).toString();
            if (text.isEmpty())
            {
                qWarning() << "text text is empty:" << QJsonDocument(data).toJson();
                continue;
            }

            messageBuilder.addText(text);
        }
        else if (type == "mention")
        {
            const QString text = data.value("displayName").toString() + " ";
            if (text.isEmpty())
            {
                qWarning() << "display name is empty:" << QJsonDocument(data).toJson();
                continue;
            }

            Message::TextStyle style;
            style.bold = true;
            messageBuilder.addText(text, style);
        }
        else if (type == "smile")
        {
            const QString url = data.value("smallUrl").toString();
            if (!url.isEmpty())
            {
                messageBuilder.addImage(url);
            }
            else
            {
                qWarning() << "image url is empty:" << QJsonDocument(data).toJson();
            }
        }
        else if (type == "link")
        {
            const QString rawContent = data.value("content").toString();
            if (rawContent.isEmpty())
            {
                qWarning() << "link content is empty:" << QJsonDocument(data).toJson();
                continue;
            }

            const QJsonArray rawContents = QJsonDocument::fromJson(rawContent.toUtf8()).array();
            if (rawContents.isEmpty())
            {
                qWarning() << "link content string is empty:" << QJsonDocument(data).toJson();
                continue;
            }

            const QString text = rawContents.at(0).toString();
            const QUrl url = data.value("url").toString();

            if (text.isEmpty())
            {
                qWarning() << "link text is empty:" << QJsonDocument(data);
                continue;
            }

            if (url.isEmpty())
            {
                qWarning() << "link url is empty:" << QJsonDocument(data).toJson();
                continue;
            }

            messageBuilder.addHyperlink(text, url);
        }
        else
        {
            qDebug() << "unknown content type" << type << ", message:" << QJsonDocument(data).toJson();
        }
    }

    emit readyRead({ messageBuilder.build() }, { author });
}
