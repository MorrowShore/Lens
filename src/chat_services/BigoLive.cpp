#include "BigoLive.h"

BigoLive::BigoLive(ChatManager &manager, QSettings &settings, const QString &settingsGroupPathParent, QNetworkAccessManager &network_, cweqt::Manager &web, QObject *parent)
    : ChatService(manager, settings, settingsGroupPathParent, ChatServiceType::BigoLive, true, parent)
    , network(network_)
    , socket("https://www.bigo.tv")
{
    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state)
    {
        Q_UNUSED(state)
        //qDebug() << "webSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &BigoLive::onWebSocketReceived);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        qDebug() << "webSocket connected";

        sendStart();
    });

    QObject::connect(&socket, &QWebSocket::disconnected, this, [this]()
    {
        qDebug() << "webSocket disconnected";
        reset();
    });

    QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError error_)
    {
        qDebug() << "webSocket error:" << error_ << ":" << socket.errorString();
    });
}

ChatService::ConnectionState BigoLive::getConnectionState() const
{
    if (isConnected())
    {
        return ChatService::ConnectionState::Connected;
    }
    else if (isEnabled() && !state.streamId.isEmpty())
    {
        return ChatService::ConnectionState::Connecting;
    }

    return ChatService::ConnectionState::NotConnected;
}

QString BigoLive::getMainError() const
{
    if (state.streamId.isEmpty())
    {
        return tr("Channel not specified");
    }

    return tr("Not connected");
}

void BigoLive::resetImpl()
{
    socket.close();
}

void BigoLive::connectImpl()
{
    socket.setProxy(network.proxy());
    socket.open(QUrl("wss://wss.bigolive.tv/bigo/official/web"));
}

void BigoLive::onWebSocketReceived(const QString &raw)
{
    qDebug() << raw;
}

void BigoLive::sendStart()
{
    //socket.sendTextMessage("512279{\"uid\":\"1246863887\",\"cookie\":\"BAAAAJliD6cFQQCepMP9Ri/p2PYdaBaukHlBrlOHiXU6+9ZO+01+fVlqRzPLKC8zIs1PogW+Pob0oFKqDoADbPz7KH+dfh2IVq5TeV/AjGfvTR6NZOMoShaOaf7mVXquZMXrNMwPMfY=\",\"secret\":\"0\",\"userName\":\"0\",\"deviceId\":\"web_6280920789e5e26ebdcc607b90295027\",\"userFlag\":\"0\",\"status\":\"0\",\"password\":\"0\",\"sdkVersion\":\"0\",\"displayType\":\"0\",\"pbVersion\":\"0\",\"lang\":\"cn\",\"loginLevel\":\"0\",\"clientVersionCode\":\"0\",\"clientType\":\"8\",\"clientOsVer\":\"0\",\"netConf\":{\"clientIp\":\"0\",\"proxySwitch\":\"0\",\"proxyTimestamp\":\"0\",\"mcc\":\"0\",\"mnc\":\"0\",\"countryCode\":\"CN\"}}");
    //socket.sendTextMessage("1304{\"secretKey\":\"\",\"seqId\":\"1697643725448\",\"roomId\":\"6597447910794533486\",\"reserver\":\"1\",\"clientVersion\":\"0\",\"clientType\":\"7\",\"version\":\"15\",\"deviceid\":\"web_6280920789e5e26ebdcc607b90295027\",\"other\":[]}");

    {
        const QString MessageId = "512535";

        QJsonObject();

        socket.sendTextMessage(MessageId + "\t");
    }
}
