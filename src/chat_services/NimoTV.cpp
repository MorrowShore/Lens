#include "NimoTV.h"
#include <QNetworkAccessManager>

NimoTV::NimoTV(ChatManager &manager, QSettings &settings, const QString &settingsGroupPathParent, QNetworkAccessManager &network_, cweqt::Manager &web, QObject *parent)
    : ChatService(manager, settings, settingsGroupPathParent, AxelChat::ServiceType::NimoTV, false, parent)
    , network(network_)
    , socket("https://www.nimo.tv")
{
    ui.findBySetting(stream)->setItemProperty("name", tr("Channel"));
    ui.findBySetting(stream)->setItemProperty("placeholderText", tr("Link or channel name..."));

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state)
    {
        Q_UNUSED(state)
        qDebug() << "WebSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::binaryMessageReceived, this, &NimoTV::onWebSocketReceived);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        qDebug() << "WebSocket connected";

        const QByteArray data = "AAMdAAEAiwAAAIsQAyw8QP9WBmxhdW5jaGYId3NMYXVuY2h9AABmCAABBgR0UmVxHQAAWQoDAAAB8iTjJ0MWIDBhZDcwYjhlZTUzYTFkNjU0MTAxNDcyZDQ4MmRlYmIxJhB3ZWJoNSYwLjAuMSZuaW1vNgxOSU1PJlJVJjEwNDlKBgAWACYANgBGAAsLjJgMqAwsNgBMXGYA";
        socket.sendBinaryMessage(QByteArray::fromBase64(data));
    });

    QObject::connect(&socket, &QWebSocket::disconnected, this, [this]()
    {
        qDebug() << "WebSocket disconnected";
        setConnected(false);
    });

    QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError error_)
    {
        qCritical() << "WebSocket error:" << error_ << ":" << socket.errorString();
    });
}

ChatService::ConnectionState NimoTV::getConnectionState() const
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

QString NimoTV::getMainError() const
{
    if (state.streamId.isEmpty())
    {
        return tr("Channel not specified");
    }

    return tr("Not connected");
}

void NimoTV::reconnectImpl()
{
    socket.close();

    if (!isEnabled())
    {
        return;
    }

    socket.setProxy(network.proxy());
    socket.open(QUrl("wss://2ffe8363-ws.master.live/?baseinfo=AwAAAfIk4ydDFiAwYWQ3MGI4ZWU1M2ExZDY1NDEwMTQ3MmQ0ODJkZWJiMSYQd2ViaDUmMC4wLjEmbmltbzYMTklNTyZSVSYxMDQ5RgBWAGx2AIYAlgCoDA=="));
}

void NimoTV::onWebSocketReceived(const QByteArray &bin)
{
    qDebug() << bin;
}
