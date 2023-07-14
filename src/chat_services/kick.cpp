#include "kick.h"
#include "models/message.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace
{

static const int ReconncectPeriod = 5 * 1000;
static const QString APP_ID = "eb1d5f283081a78b932c"; // TODO

}

Kick::Kick(QSettings &settings, const QString &settingsGroupPathParent, QNetworkAccessManager &network_, cweqt::Manager& web_, QObject *parent)
    : ChatService(settings, settingsGroupPathParent, AxelChat::ServiceType::Kick, parent)
    , network(network_)
    , web(web_)
    , socket("https://kick.com")
{
    getUIElementBridgeBySetting(stream)->setItemProperty("name", tr("Channel"));
    getUIElementBridgeBySetting(stream)->setItemProperty("placeholderText", tr("Link or channel name..."));

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state)
    {
        Q_UNUSED(state)
        //qDebug() << Q_FUNC_INFO << ": WebSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &Kick::onWebSocketReceived);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        //qDebug() << Q_FUNC_INFO << ": WebSocket connected";

        if (info.chatroomId.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "chatroom id is empty";
            socket.close();
            return;
        }

        sendSubscribe(info.chatroomId);

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

    /*QObject::connect(&browser, QOverload<const BrowserHandler::Response&>::of(&BrowserHandler::responsed), this, [this](const BrowserHandler::Response& response)
    {
        onChannelInfoReply(response.data);
    });

    QObject::connect(&browser, &BrowserHandler::initialized, this, [this]()
    {
        qDebug() << Q_FUNC_INFO << "initialized";

        if (enabled.get())
        {
            requestChannelInfo(state.streamId);
        }
    });

    BrowserHandler::CommandLineParameters params;

    BrowserHandler::FilterSettings filter;
    filter.urlPrefixes = { "https://kick.com/api/" };
    filter.mimeTypes = { "text/html", "application/json" };
    params.filterSettings = filter;

    params.showResponses = true;*/

    reconnect();

    QObject::connect(&timerReconnect, &QTimer::timeout, this, [this]()
    {
        if (!enabled.get())
        {
            return;
        }

        if (!state.connected)
        {
            reconnect();
        }
    });
    timerReconnect.start(ReconncectPeriod);
}

ChatService::ConnectionStateType Kick::getConnectionStateType() const
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

QString Kick::getStateDescription() const
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

void Kick::reconnectImpl()
{
    socket.close();

    state = State();
    info = Info();

    state.controlPanelUrl = QUrl(QString("https://kick.com/dashboard/stream"));

    state.streamId = extractChannelName(stream.get());

    if (state.streamId.isEmpty())
    {
        emit stateChanged();
        return;
    }

    state.chatUrl = QUrl(QString("https://kick.com/%1/chatroom").arg(state.streamId));
    state.streamUrl = QUrl(QString("https://kick.com/%1").arg(state.streamId));

    if (enabled.get())
    {
        requestChannelInfo(state.streamId);
    }
}

void Kick::onWebSocketReceived(const QString &rawData)
{
    if (!enabled.get())
    {
        return;
    }

    const QJsonObject root = QJsonDocument::fromJson(rawData.toUtf8()).object();
    const QString type = root.value("event").toString();
    const QJsonObject data = QJsonDocument::fromJson(root.value("data").toString().toUtf8()).object();

    if (type == "App\\Events\\ChatMessageEvent")
    {
        parseChatMessageEvent(data);
    }
    else if (type == "pusher:pong")
    {
        // TODO
    }
    else if (type == "App\\Events\\UserBannedEvent")
    {
        // TODO:
        // {"event":"App\\Events\\UserBannedEvent","data":"{\"id\":\"33667119-a401-4175-bf82-c9040940c4ba\",\"user\":{\"id\":8401465,\"username\":\"2PAC_SHAKUR\",\"slug\":\"2pac-shakur\"},\"banned_by\":{\"id\":8537435,\"username\":\"TheJuicer\",\"slug\":\"thejuicer\"},\"expires_at\":\"2023-06-23T13:09:00+00:00\"}","channel":"chatrooms.668.v2"}
        qWarning() << Q_FUNC_INFO << "unsupported" << type;
        qDebug() << data;
    }
    else if (type == "pusher:connection_established" || type == "pusher_internal:subscription_succeeded")
    {
        if (!state.connected)
        {
            state.connected = true;
            emit connectedChanged(true);
            emit stateChanged();
        }

        sendPing();
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "unknown event type" << type;
        qDebug() << data;
    }
}

void Kick::send(const QJsonObject &object)
{
    const QString data = QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::JsonFormat::Compact));

    //qDebug("\nsend: " + data.toUtf8() + "\n");

    socket.sendTextMessage(data);
}

void Kick::sendSubscribe(const QString& chatroomId)
{
    send(QJsonObject(
             {
                 { "event", "pusher:subscribe" },
                 { "data", QJsonObject(
                    {
                        { "auth", "" },
                        { "channel", "chatrooms." + chatroomId + ".v2" }
                    })}
             }));
}

void Kick::sendPing()
{
    send(QJsonObject(
             {
                 { "event", "pusher:ping" },
                 { "data", QJsonObject()}
             }));
}

void Kick::requestChannelInfo(const QString &channelName)
{
    qWarning() << "============= TODO";
    /*if (!web.isInitialized())
    {
        return;
    }

    browser.openBrowser("https://kick.com/api/v2/channels/" + channelName);*/
}

void Kick::onChannelInfoReply(const QByteArray &data)
{
    const QJsonObject root = QJsonDocument::fromJson(data).object();
    const QJsonObject chatroom = root.value("chatroom").toObject();

    info.chatroomId = QString("%1").arg(chatroom.value("id").toVariant().toLongLong());

    socket.open("wss://ws-us2.pusher.com/app/" + APP_ID + "?protocol=7&client=js&version=7.6.0&flash=false");
}

void Kick::parseChatMessageEvent(const QJsonObject &data)
{
    const QString type = data.value("type").toString();
    const QString id = data.value("id").toString();
    const QString rawContent = data.value("content").toString();
    const QDateTime publishedTime = QDateTime::fromString(data.value("created_at").toString(), Qt::DateFormat::ISODate);

    const QJsonObject sender = data.value("sender").toObject();
    const QString slug = sender.value("slug").toString();
    const QString authorName = sender.value("username").toString();

    const QJsonObject identity = sender.value("identity").toObject();
    const QColor authorColor = identity.value("color").toString();
    const QJsonArray badgesJson = identity.value("badges").toArray();

    if (type == "message")
    {
        //
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "unkown message type" << type;
        qDebug() << data;
    }

    if (rawContent.isEmpty())
    {
        return;
    }

    QStringList leftBadges;

    QList<Message> messages;
    QList<Author> authors;

    Author author(getServiceType(),
                  authorName,
                  getServiceTypeId(getServiceType()) + "_" + slug,
                  QUrl(""),
                  "https://kick.com/" + slug,
                  leftBadges, {}, {},
                  authorColor);

    QList<Message::Content *> contents;

    contents.append(new Message::Text(rawContent));

    Message message(contents,
                    author,
                    publishedTime,
                    QDateTime::currentDateTime(),
                    getServiceTypeId(getServiceType()) + QString("/%1").arg(id));

    messages.append(message);
    authors.append(author);

    if (!messages.isEmpty())
    {
        emit readyRead(messages, authors);
    }

    qDebug() << data;
}

QString Kick::extractChannelName(const QString &stream_)
{
    QString stream = stream_.trimmed();
    if (stream.contains('/'))
    {
        stream = AxelChat::simplifyUrl(stream);
        bool ok = false;
        stream = AxelChat::removeFromStart(stream, "kick.com/", Qt::CaseSensitivity::CaseInsensitive, &ok);
        if (!ok)
        {
            return QString();
        }
    }

    if (stream.contains('/'))
    {
        stream = stream.left(stream.indexOf('/'));
    }

    return stream;
}
