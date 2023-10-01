#include "dlive.h"
#include <QNetworkRequest>
#include <QNetworkReply>

namespace
{

static const QString Hash256Sha = "950c61faccae0df49c8e19a3a0e741ccb39fd322c850bca52a7562bfa63f49c1";

static bool checkReply(QNetworkReply *reply, const char *tag, QByteArray &resultData)
{
    resultData.clear();

    if (!reply)
    {
        qWarning() << tag << ": !reply";
        return false;
    }

    int statusCode = 200;
    const QVariant rawStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    resultData = reply->readAll();
    const QUrl requestUrl = reply->request().url();
    reply->deleteLater();

    if (rawStatusCode.isValid())
    {
        statusCode = rawStatusCode.toInt();

        if (statusCode != 200)
        {
            qWarning() << tag << ": status code:" << statusCode;
        }
    }

    if (resultData.isEmpty() && statusCode != 200)
    {
        qWarning() << tag << ": data is empty";
        return false;
    }

    return true;
}

}

DLive::DLive(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network_, cweqt::Manager&, QObject *parent)
    : ChatService(settings, settingsGroupPathParent, AxelChat::ServiceType::DLive, false, parent)
    , network(network_)
    , socket("https://dlive.tv")
{
    ui.findBySetting(stream)->setItemProperty("name", tr("Channel"));
    ui.findBySetting(stream)->setItemProperty("placeholderText", tr("Link or channel name..."));

    // wss://graphigostream.prd.dlive.tv/

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state)
    {
        Q_UNUSED(state)
        //qDebug() << "WebSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &DLive::onWebSocketReceived);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        qDebug() << "WebSocket connected";

        send("connection_init");
    });

    QObject::connect(&socket, &QWebSocket::disconnected, this, [this]()
    {
        qDebug() << "WebSocket disconnected";

        if (state.connected)
        {
            state.connected = false;
            emit connectedChanged(false);
            emit stateChanged();
        }

        state.viewersCount = -1;
    });

    QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError error_)
    {
        qDebug() << "WebSocket error:" << error_ << ":" << socket.errorString();
    });

    reconnect();
}

ChatService::ConnectionStateType DLive::getConnectionStateType() const
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

QString DLive::getStateDescription() const
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

void DLive::reconnectImpl()
{
    state = State();
    info = Info();

    socket.close();

    state.controlPanelUrl = "https://dlive.tv/s/dashboard#0";

    state.streamId = extractChannelName(stream.get());

    if (state.streamId.isEmpty())
    {
        return;
    }

    state.streamUrl = "https://dlive.tv/" + state.streamId;

    if (!enabled.get())
    {
        return;
    }

    requestLivestreamPage(state.streamId);
}

void DLive::send(const QString &type, const QJsonObject &payload, const int64_t id)
{
    QJsonObject object({
        { "type", type },
        { "payload", payload },
    });

    if (id != -1)
    {
        object.insert("id", id);
    }

    socket.sendTextMessage(QString::fromUtf8(QJsonDocument(object).toJson()));
}

void DLive::onWebSocketReceived(const QString &raw)
{
    const QJsonObject root = QJsonDocument::fromJson(raw.toUtf8()).object();
    qDebug() << "received:" << root;

    const QString type = root.value("type").toString();

    if (type == "connection_ack")
    {
        //
    }
    else if (type == "ka")
    {
        //
    }
    else
    {
        qWarning() << "unknown message type" << type << ", root =" << root;
    }
}

void DLive::requestLivestreamPage(const QString &displayName_)
{
    QString displayName = displayName_.trimmed();
    if (displayName.isEmpty())
    {
        qWarning() << "display name is empty";
        return;
    }

    if (!enabled.get())
    {
        return;
    }

    const QByteArray body = QJsonDocument(QJsonObject(
        {
            { "operationName", "LivestreamPage" },
            { "variables", QJsonObject(
                {
                    { "displayname", displayName },
                    { "add", false },
                    { "isLoggedIn", false },
                    { "isMe", false },
                    { "showUnpicked", false },
                    { "order", "PickTime" },
                })
            },
            { "extensions", QJsonObject(
                {
                    { "persistedQuery", QJsonObject(
                        {
                            { "version", 1 },
                            { "sha256Hash", Hash256Sha },
                        })
                    }
                })
            }
        }
    )).toJson(QJsonDocument::JsonFormat::Compact);

    QNetworkRequest request(QUrl("https://graphigo.prd.dlive.tv/"));
    request.setRawHeader("Content-Type", "application/json");

    QNetworkReply* reply = network.post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, displayName]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        UserInfo user;

        const QJsonObject root = QJsonDocument::fromJson(data).object();

        const QJsonObject jsonUser = root
            .value("data").toObject()
            .value("userByDisplayName").toObject();

        user.userName = jsonUser.value("username").toString();
        user.avatar = jsonUser.value("avatar").toString();
        user.displayName = jsonUser.value("displayname").toString();

        if (user.userName.isEmpty())
        {
            qWarning() << "user name is empty, display name =" << displayName;
            return;
        }

        users.insert(user.userName, user);

        if (user.displayName.trimmed().toLower() == state.streamId.trimmed().toLower())
        {
            const QJsonObject jsonLivestream = jsonUser.value("livestream").toObject();
            if (jsonLivestream.isEmpty())
            {
                qDebug() << "livestream not started";
                return;
            }

            info.owner = user;

            info.permalink = jsonLivestream.value("permlink").toString();

            state.viewersCount = jsonLivestream.value("watchingCount").toInt(-1);

            state.chatUrl = "https://dlive.tv/c/" + user.displayName + "/" + info.permalink;

            if (!state.connected)
            {
                socket.open(QUrl("wss://graphigostream.prd.dlive.tv/"));
            }

            emit stateChanged();
        }
    });
}

QString DLive::extractChannelName(const QString &stream)
{
    QRegExp rx;

    const QString simpleUserSpecifiedUserChannel = AxelChat::simplifyUrl(stream);
    rx = QRegExp("^dlive.tv/([^/]*)$", Qt::CaseInsensitive);
    if (rx.indexIn(simpleUserSpecifiedUserChannel) != -1)
    {
        return rx.cap(1);
    }

    rx = QRegExp("^[a-zA-Z0-9_]+$", Qt::CaseInsensitive);
    if (rx.indexIn(stream) != -1)
    {
        return stream;
    }

    return QString();
}
