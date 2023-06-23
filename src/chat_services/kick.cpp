#include "kick.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace
{

const static QString APP_ID = "eb1d5f283081a78b932c"; // TODO

}

Kick::Kick(QSettings &settings, const QString &settingsGroupPathParent, QNetworkAccessManager &network_, QObject *parent)
    : ChatService(settings, settingsGroupPathParent, AxelChat::ServiceType::Kick, parent)
    , network(network_)
    , socket("https://kick.com")
{
    getUIElementBridgeBySetting(stream)->setItemProperty("name", tr("Channel"));
    getUIElementBridgeBySetting(stream)->setItemProperty("placeholderText", tr("Link or channel name..."));

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state)
    {
        Q_UNUSED(state)
        qDebug() << Q_FUNC_INFO << ": WebSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &Kick::onWebSocketReceived);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        qDebug() << Q_FUNC_INFO << ": WebSocket connected";

        sendSubscribe("380174", "380090"); // TODO

        emit stateChanged();
    });

    QObject::connect(&socket, &QWebSocket::disconnected, this, [this]()
    {
        qDebug() << Q_FUNC_INFO << ": WebSocket disconnected";

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

    reconnect();
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
        interceptChannelInfo(state.streamId);
    }
}

void Kick::onWebSocketReceived(const QString &rawData)
{
    qDebug("\nreceived: " + rawData.toUtf8() + "\n");

    if (!enabled.get())
    {
        return;
    }

    if (rawData.isEmpty())
    {
        qWarning() << Q_FUNC_INFO << "received data is empty";
        return;
    }
}

void Kick::send(const QJsonObject &object)
{
    const QString data = QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::JsonFormat::Compact));

    qDebug("\nsend: " + data.toUtf8() + "\n");

    socket.sendTextMessage(data);
}

void Kick::sendSubscribe(const QString& channelId, const QString& chatroomId)
{
    send(QJsonObject(
             {
                 { "event", "pusher:subscribe" },
                 { "data", QJsonObject(
                    {
                        { "auth", "" },
                        { "channel", "channel." + channelId }
                    })}
             }));

    send(QJsonObject(
             {
                 { "event", "pusher:subscribe" },
                 { "data", QJsonObject(
                    {
                        { "auth", "" },
                        { "channel", "chatrooms." + chatroomId }
                    })}
             }));
}

void Kick::requestChannelInfo(const QString &channelName)
{
    QNetworkRequest request(QUrl("https://kick.com/api/v1/channels/" + channelName.trimmed().toLower()));
    request.setRawHeader("Accept", "application/json, text/plain, */*");
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:109.0) Gecko/20100101 Firefox/111.0");
    request.setRawHeader("Alt-Used", "kick.com");
    request.setRawHeader("Accept-Language", "en-US;q=0.5,en;q=0.3");
    request.setRawHeader("Cookie", "__cf_bm=u6_YcMA7CXi.lb5w4WEgS09n9eWutv.8Xq0oKbxP.c0-1681414255-0-AVgwpcfcZY83BD8L4So5rPBur6yKacStQQj6oo7aUFtWEsHFNyJo+1cJar7Pkrep6XKn70D3Wzood+GvmgXZnok=");

    QNetworkReply* reply = network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, channelName]()
    {
        const QByteArray data = reply->readAll();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();

        qDebug() << statusCode;

        qDebug() << data;

        const QJsonObject root = QJsonDocument::fromJson(data).object();

        qDebug() << root;
    });
}

void Kick::interceptChannelInfo(const QString &channelName)
{
    static const int Timeout = 5000;

    WebInterceptorHandler::FilterSettings settings;
    settings.urlPrefixes.insert("https://kick.com/api/");
    interceptor.setFilterSettings(settings);

    interceptor.start(false, QUrl("https://kick.com/api/v2/channels/" + channelName), Timeout,
                      [this](const WebInterceptorHandler::Response& response)
    {
        onChannelInfoReply(response.data);
        interceptor.stop();
    });
}

void Kick::onChannelInfoReply(const QByteArray &data)
{
    const QJsonObject root = QJsonDocument::fromJson(data).object();

    qDebug() << root;
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
