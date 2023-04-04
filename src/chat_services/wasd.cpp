#include "wasd.h"
#include "secrets.h"
#include "crypto/obfuscator.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace
{

static const QString ApiToken = OBFUSCATE(WASD_API_TOKEN);
static const int ReconncectPeriod = 3 * 1000;

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
        qDebug() << Q_FUNC_INFO << ": WebSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &Wasd::onWebSocketReceived);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        qDebug() << Q_FUNC_INFO << ": WebSocket connected";

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
            socket.open(QUrl("wss://chat.wasd.tv"));
        }
    });
    timerReconnect.start(ReconncectPeriod);

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
