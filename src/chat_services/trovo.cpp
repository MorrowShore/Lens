#include "trovo.h"
#include <QNetworkAccessManager>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

Trovo::Trovo(QSettings &settings_, const QString &settingsGroupPath, QNetworkAccessManager &network_, QObject *parent)
    : ChatService(settings_, settingsGroupPath, AxelChat::ServiceType::Trovo, parent)
    , settings(settings_)
    , network(network_)
{
    getParameter(stream)->setPlaceholder(tr("Link or channel name..."));

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state)
    {
        Q_UNUSED(state)
        qDebug() << Q_FUNC_INFO << ": WebSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &Trovo::onWebSocketReceived);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        if (state.connected)
        {
            state.connected = false;
            emit connectedChanged(false, lastConnectedChannelName);
        }

        QJsonObject root;
        root.insert("type", "AUTH");
        root.insert("nonce", "nonce");

        QJsonObject data;

        data.insert("token", "token");

        root.insert("data", data);

        sendToWebSocket(QJsonDocument(root));

        emit stateChanged();
    });

    QObject::connect(&socket, &QWebSocket::disconnected, this, [this]()
    {
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

    reconnect();
}

ChatService::ConnectionStateType Trovo::getConnectionStateType() const
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

QString Trovo::getStateDescription() const
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

void Trovo::reconnect()
{
    socket.close();

    state = State();

    state.streamId = getStreamId(stream.get());

    if (state.streamId.isEmpty())
    {
        emit stateChanged();
        return;
    }

    socket.setProxy(network.proxy());
    socket.open(QUrl("wss://open-chat.trovo.live/chat"));

    emit stateChanged();
}

void Trovo::onWebSocketReceived(const QString& rawData)
{
    qDebug("RECIEVE: " + rawData.toUtf8() + "\n");

    const QJsonObject root = QJsonDocument::fromJson(rawData.toUtf8()).object();

    const QString typeMessage = root.value("type").toString();
    const QString error = root.value("error").toString();

    if (typeMessage == "RESPONSE")
    {

    }
    else
    {
        qWarning() << Q_FUNC_INFO << ": unknown message type" << typeMessage << ", raw data =" << rawData;
    }

    if (!error.isEmpty())
    {
        qWarning() << Q_FUNC_INFO << ": error" << error << ", raw data =" << rawData;
    }
}

void Trovo::sendToWebSocket(const QJsonDocument &data)
{
    qDebug("SEND:" + data.toJson(QJsonDocument::JsonFormat::Compact) + "\n");
    socket.sendTextMessage(QString::fromUtf8(data.toJson()));
}

QString Trovo::getStreamId(const QString &stream)
{
    //TODO
    return stream.toLower().trimmed();
}
