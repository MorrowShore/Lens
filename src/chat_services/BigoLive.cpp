#include "BigoLive.h"

BigoLive::BigoLive(ChatManager &manager, QSettings &settings, const QString &settingsGroupPathParent, QNetworkAccessManager &network_, cweqt::Manager &web, QObject *parent)
    : ChatService(manager, settings, settingsGroupPathParent, AxelChat::ServiceType::BigoLive, true, parent)
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
    });

    QObject::connect(&socket, &QWebSocket::disconnected, this, [this]()
    {
        qDebug() << "webSocket disconnected";

        setConnected(false);
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

void BigoLive::reconnectImpl()
{
    socket.close();

    if (!isEnabled())
    {
        return;
    }

    socket.setProxy(network.proxy());
    socket.open(QUrl("wss://wss.bigolive.tv/bigo/official/web"));
}

void BigoLive::onWebSocketReceived(const QString &raw)
{
    qDebug() << raw;
}
