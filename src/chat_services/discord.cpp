#include "discord.h"
#include "secrets.h"
#include "string_obfuscator/obfuscator.hpp"
#include <QDesktopServices>
#include <QTcpSocket>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSysInfo>

namespace
{

static const QString ClientID = OBFUSCATE(DISCORD_CLIENT_ID);
static const int ServerPort = 8356;

static const int ReconncectPeriod = 3 * 1000;

static const int DispatchOpCode = 0;
static const int HeartbeatOpCode = 1;
static const int IdentifyOpCode = 2;
static const int HelloOpCode = 10;
static const int HeartAckbeatOpCode = 11;

static const QByteArray makeHttpResponse(const QByteArray& data)
{
    return QString("HTTP/1.1 200 OK\n"
           "Content-Length: %1\n"
           "Content-Type: text/html;charset=UTF-8\n"
           "\n").arg(data.length()).toUtf8() + data;
}

}

Discord::Discord(QSettings &settings_, const QString &settingsGroupPath, QNetworkAccessManager &network_, QObject *parent)
    : ChatService(settings_, settingsGroupPath, AxelChat::ServiceType::Discord, parent)
    , settings(settings_)
    , network(network_)
    , authStateInfo(UIElementBridge::createLabel("Loading..."))
    , oauthToken(settings_, settingsGroupPath + "/oauth_token")
{
    getUIElementBridgeBySetting(stream)->setItemProperty("visible", false);

    addUIElement(authStateInfo);

    loginButton = std::shared_ptr<UIElementBridge>(UIElementBridge::createButton(tr("Login"), [this]()
    {
        if (isAuthorized())
        {
            oauthToken.set(QString());
        }
        else
        {
            QDesktopServices::openUrl(QUrl(QString("https://discord.com/api/oauth2/authorize?client_id=%1&redirect_uri=http%3A%2F%2Flocalhost%3A8356%2Fchat_service%2Fdiscord%2Fauth_code&&response_type=code&scope=guilds%20messages.read").arg(ClientID)));
        }

        updateAuthState();
    }));
    addUIElement(loginButton);

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &Discord::onWebSocketReceived);

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state)
    {
        Q_UNUSED(state)
        //qDebug() << Q_FUNC_INFO << ": WebSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        qDebug() << Q_FUNC_INFO << ": WebSocket connected";

        heartbeatAcknowledgementTimer.setInterval(60 * 10000);
        heartbeatAcknowledgementTimer.start();
    });

    QObject::connect(&socket, &QWebSocket::disconnected, this, [this]()
    {
        qDebug() << Q_FUNC_INFO << ": WebSocket disconnected";
        processDisconnected();
    });

    QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError error_)
    {
        qDebug() << Q_FUNC_INFO << ": WebSocket error:" << error_ << ":" << socket.errorString();
    });

    QObject::connect(&heartbeatTimer, &QTimer::timeout, this, &Discord::sendHeartbeat);

    QObject::connect(&heartbeatAcknowledgementTimer, &QTimer::timeout, this, [this]()
    {
        if (socket.state() != QAbstractSocket::SocketState::ConnectedState)
        {
            heartbeatAcknowledgementTimer.stop();
            return;
        }

        qDebug() << Q_FUNC_INFO << "heartbeat acknowledgement timeout, disconnect";
        socket.close();
    });

    QObject::connect(&timerReconnect, &QTimer::timeout, this, [this]()
    {
        if (!enabled.get())
        {
            return;
        }

        if (socket.state() == QAbstractSocket::SocketState::ConnectedState ||
            socket.state() == QAbstractSocket::SocketState::ConnectingState)
        {
            return;
        }

        if (!state.connected && isAuthorized())
        {
            reconnect();
        }
    });
    timerReconnect.start(ReconncectPeriod);

    updateAuthState();

    reconnect();
}

ChatService::ConnectionStateType Discord::getConnectionStateType() const
{
    if (state.connected)
    {
        return ChatService::ConnectionStateType::Connected;
    }
    else if (isAuthorized() && !state.streamId.isEmpty())
    {
        return ChatService::ConnectionStateType::Connecting;
    }

    return ChatService::ConnectionStateType::NotConnected;
}

QString Discord::getStateDescription() const
{
    if (!isAuthorized())
    {
        return tr("Not authorized");
    }

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

TcpReply Discord::processTcpRequest(const TcpRequest &request)
{
    const QString path = request.getUrl().path().toLower();

    if (path == "/auth_code")
    {
        const QString code = request.getUrlQuery().queryItemValue("code");
        if (code.isEmpty())
        {
            return TcpReply::createTextHtmlOK("Error: code is empty");
        }

        oauthToken.set(code);
        updateAuthState();

        return TcpReply::createTextHtmlOK(tr("Now you can close the page and return to %1").arg(QCoreApplication::applicationName()));
    }

    return TcpReply::createTextHtmlOK("Unknown path");
}

void Discord::reconnectImpl()
{
    socket.close();

    state = State();
    info = Info();

    processDisconnected();

    if (!isAuthorized() || !enabled.get())
    {
        return;
    }

    socket.setProxy(network.proxy());
    socket.open(QUrl("wss://gateway.discord.gg/?v=10&encoding=json"));
}

void Discord::onWebSocketReceived(const QString &rawData)
{
    qDebug("\nreceived:\n" + rawData.toUtf8() + "\n");

    if (!enabled.get())
    {
        return;
    }

    const QJsonObject root = QJsonDocument::fromJson(rawData.toUtf8()).object();
    if (root.contains("s"))
    {
        const QJsonValue s = root.value("s");
        if (!s.isNull())
        {
            info.lastSequence = s;
        }
    }

    const int opCode = root.value("op").toInt();

    if (opCode == DispatchOpCode)
    {
        parseDispatch(root.value("t").toString(), root.value("d").toObject());
    }
    else if (opCode == HelloOpCode)
    {
        parseHello(root.value("d").toObject());
    }
    else if (opCode == HeartAckbeatOpCode)
    {
        heartbeatAcknowledgementTimer.setInterval(info.heartbeatInterval * 1.5);
        heartbeatAcknowledgementTimer.start();
    }
    else if (opCode == HeartbeatOpCode)
    {
        sendHeartbeat();
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "unknown op code" << opCode;
    }
}

void Discord::sendHeartbeat()
{
    if (socket.state() != QAbstractSocket::SocketState::ConnectedState)
    {
        return;
    }

    send(HeartbeatOpCode, info.lastSequence);
}

void Discord::sendIdentify()
{
    if (socket.state() != QAbstractSocket::SocketState::ConnectedState)
    {
        return;
    }

    const QJsonObject data =
    {
        { "token", QString(OBFUSCATE(DISCORD_BOT_TOKEN)) }, //oauthToken.get() },
        { "properties", QJsonObject(
          {
              { "os", QSysInfo::productType() },
              { "browser", QCoreApplication::applicationName() },
              { "device", QCoreApplication::applicationName() },
          })
        },

        { "intents", 1024 },
    };

    send(IdentifyOpCode, data);
}

bool Discord::isAuthorized() const
{
    return !oauthToken.get().trimmed().isEmpty();
}

void Discord::updateAuthState()
{
    if (!authStateInfo)
    {
        qCritical() << Q_FUNC_INFO << "!authStateInfo";
    }

    if (isAuthorized())
    {
        authStateInfo->setItemProperty("text", tr("Authorized"));
        loginButton->setItemProperty("text", tr("Logout"));
    }
    else
    {
        authStateInfo->setItemProperty("text", tr("Not authorized"));
        loginButton->setItemProperty("text", tr("Login"));
    }

    emit stateChanged();
}

void Discord::processDisconnected()
{
    if (state.connected)
    {
        state.connected = false;
        emit stateChanged();
        emit connectedChanged(false, QString());
    }
}

void Discord::send(const int opCode, const QJsonValue &data)
{
    QJsonObject message =
    {
        { "op", opCode},
        { "d", data },
    };

    qDebug("\nsend:\n" + QJsonDocument(message).toJson() + "\n");

    socket.sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson()));
}

void Discord::parseHello(const QJsonObject &data)
{
    info.heartbeatInterval = data.value("heartbeat_interval").toInt(30000);
    if (info.heartbeatInterval < 1000)
    {
        info.heartbeatInterval = 1000;
    }

    heartbeatTimer.setInterval(info.heartbeatInterval);
    heartbeatTimer.start();

    sendHeartbeat();

    sendIdentify();
}

void Discord::parseDispatch(const QString &eventType, const QJsonObject &data)
{
    if (eventType == "READY")
    {

    }
    else
    {
        qWarning() << Q_FUNC_INFO << "unkown event type" << eventType;
    }
}
