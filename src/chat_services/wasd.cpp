#include "wasd.h"
#include "secrets.h"
#include "crypto/obfuscator.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonArray>

namespace
{

static const QString ApiToken = OBFUSCATE(WASD_API_TOKEN);
static const int ReconncectPeriod = 3 * 1000;
static const int PingPeriod = 3 * 1000;

};

Wasd::Wasd(QSettings &settings, const QString &settingsGroupPath, QNetworkAccessManager &network_, QObject *parent)
    : ChatService(settings, settingsGroupPath, AxelChat::ServiceType::Wasd, parent)
    , network(network_)
    , socket("https://wasd.tv/")
{
    getUIElementBridgeBySetting(stream)->setItemProperty("name", tr("Channel"));
    getUIElementBridgeBySetting(stream)->setItemProperty("placeholderText", tr("Link or channel name..."));

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state)
    {
        Q_UNUSED(state)
        //qDebug() << Q_FUNC_INFO << ": WebSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &Wasd::onWebSocketReceived);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        qDebug() << Q_FUNC_INFO << ": WebSocket connected";

        if (info.jwtToken.isEmpty())
        {
            qDebug() << Q_FUNC_INFO << "jwt token is empty, disconnect";
            socket.close();
            return;
        }

        sendJoin("1380680", "842797", info.jwtToken); // TODO
        sendPing();

        emit stateChanged();
    });

    QObject::connect(&socket, &QWebSocket::disconnected, this, [this]()
    {
        qDebug() << Q_FUNC_INFO << ": WebSocket disconnected";

        if (state.connected)
        {
            state.connected = false;
            emit stateChanged();
            emit connectedChanged(false, QString());
        }
    });

    QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError error_)
    {
        qDebug() << Q_FUNC_INFO << ": WebSocket error:" << error_ << ":" << socket.errorString();
    });

    QObject::connect(&timerReconnect, &QTimer::timeout, this, [this]()
    {
        if (socket.state() == QAbstractSocket::SocketState::UnconnectedState && !info.jwtToken.isEmpty())
        {
            socket.setProxy(network.proxy());
            socket.open(QUrl("wss://chat.wasd.tv/socket.io/?EIO=3&transport=websocket"));
        }
    });
    timerReconnect.start(ReconncectPeriod);

    QObject::connect(&timerReconnect, &QTimer::timeout, this, [this]()
    {
        if (socket.state() == QAbstractSocket::SocketState::UnconnectedState && !info.jwtToken.isEmpty())
        {
            socket.setProxy(network.proxy());
            socket.open(QUrl("wss://chat.wasd.tv/socket.io/?EIO=3&transport=websocket"));
        }
    });
    timerReconnect.start(ReconncectPeriod);


    QObject::connect(&timerPing, &QTimer::timeout, this, [this]()
    {
        if (socket.state() == QAbstractSocket::SocketState::ConnectedState)
        {
            sendPing();
        }
    });
    timerPing.start(PingPeriod);

    reconnect();
}

ChatService::ConnectionStateType Wasd::getConnectionStateType() const
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

QString Wasd::getStateDescription() const
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

void Wasd::reconnectImpl()
{
    socket.close();

    state = State();
    info = Info();

    state.controlPanelUrl = QUrl(QString("https://wasd.tv/stream-settings"));

    state.streamId = extractChannelName(stream.get());

    if (state.streamId.isEmpty())
    {
        emit stateChanged();
        return;
    }

    state.chatUrl = QUrl(QString("https://wasd.tv/chat?channel_name=%1").arg(state.streamId));
    state.streamUrl = QUrl(QString("https://wasd.tv/%1").arg(state.streamId));

    if (enabled.get())
    {
        requestTokenJWT();
    }
}

void Wasd::onWebSocketReceived(const QString &rawData)
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

    bool ok = false;
    const SocketIO2Type type = (SocketIO2Type)QString(rawData[0]).toInt(&ok);
    if (!ok)
    {
        qWarning() << Q_FUNC_INFO << "failed to get type";
        return;
    }

    QByteArray payload;

    for (int i = 1; i < rawData.length(); i++)
    {
        if (!rawData[i].isNumber())
        {
            payload = rawData.mid(i).toUtf8();
            break;
        }
    }

    QJsonParseError jsonError;
    const QJsonDocument doc = payload.isEmpty() ? QJsonDocument() : QJsonDocument::fromJson(payload, &jsonError);
    if (jsonError.error != QJsonParseError::ParseError::NoError)
    {
        qWarning() << Q_FUNC_INFO << "json parse error =" << jsonError.errorString() << ", offset =" << jsonError.offset << payload;
    }

    switch(type)
    {
    case SocketIO2Type::CONNECT:
        break;

    case SocketIO2Type::DISCONNECT:
        break;

    case SocketIO2Type::EVENT:
        break;

    case SocketIO2Type::ACK:
        break;

    case SocketIO2Type::CONNECT_ERROR:
        break;

    case SocketIO2Type::BINARY_EVENT:
        break;

    case SocketIO2Type::BINARY_ACK:
        break;

    default:
        qWarning() << Q_FUNC_INFO << "unknown message type" << (int)type;
    }
}

void Wasd::requestTokenJWT()
{
    QNetworkRequest request(QUrl("https://wasd.tv/api/auth/chat-token"));
    request.setRawHeader("Authorization", QString("Token %1").arg(ApiToken).toUtf8());

    QNetworkReply* reply = network.post(request, QByteArray());
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const QByteArray data = reply->readAll();
        reply->deleteLater();

        const QJsonObject root = QJsonDocument::fromJson(data).object();

        info.jwtToken = root.value("result").toString();

        if (info.jwtToken.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "failed to get jwt token";
        }
    });
}

void Wasd::send(const SocketIO2Type type, const QByteArray& id, const QJsonDocument& payload)
{
    const int typeNum = (int)type;

    QString message = QString("%1").arg(typeNum) + id + QString::fromUtf8(payload.toJson(QJsonDocument::JsonFormat::Compact));

    qDebug("\nsend: " + message.toUtf8() + "\n");

    socket.sendTextMessage(message);
}

void Wasd::sendJoin(const QString &streamId, const QString &channelId, const QString &jwt)
{
    const bool excludeStickers = true; // TODO

    QJsonArray array =
    {
        "join",
        QJsonObject(
        {
            { "streamId", streamId },
            { "channelId", channelId },
            { "jwt", jwt },
            { "excludeStickers", excludeStickers }
        })
    };

    send(SocketIO2Type::CONNECT_ERROR, "2", QJsonDocument(array));
}

void Wasd::sendPing()
{
    send(SocketIO2Type::EVENT);
}

QString Wasd::extractChannelName(const QString &stream)
{
    QString channelName = stream.toLower().trimmed();

    if (channelName.startsWith("https://wasd.tv/", Qt::CaseSensitivity::CaseInsensitive))
    {
        channelName = channelName.mid(16);

        if (channelName.contains('?'))
        {
            channelName = channelName.left(channelName.indexOf('?'));
        }
    }

    QRegExp rx("^[a-zA-Z0-9_\\-]+$", Qt::CaseInsensitive);
    if (rx.indexIn(channelName) != -1)
    {
        return channelName;
    }

    return QString();
}
