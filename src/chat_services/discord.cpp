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

static const int ReconncectPeriod = 3 * 1000;

static const int DispatchOpCode = 0;
static const int HeartbeatOpCode = 1;
static const int IdentifyOpCode = 2;
static const int HelloOpCode = 10;
static const int HeartAckbeatOpCode = 11;

// https://discord.com/developers/docs/topics/gateway#gateway-intents
static const int INTENT_GUILDS = 1 << 0;
static const int INTENT_GUILD_MEMBERS = 1 << 1;
static const int INTENT_GUILD_MODERATION = 1 << 2;
static const int INTENT_GUILD_EMOJIS_AND_STICKERS = 1 << 3;
static const int INTENT_GUILD_INTEGRATIONS = 1 << 4;
static const int INTENT_GUILD_WEBHOOKS = 1 << 5;
static const int INTENT_GUILD_INVITES = 1 << 6;
static const int INTENT_GUILD_VOICE_STATES = 1 << 7;
static const int INTENT_GUILD_PRESENCES = 1 << 8;
static const int INTENT_GUILD_MESSAGES = 1 << 9;
static const int INTENT_GUILD_MESSAGE_REACTIONS = 1 << 10;
static const int INTENT_GUILD_MESSAGE_TYPING = 1 << 11;
static const int INTENT_DIRECT_MESSAGES = 1 << 12;
static const int INTENT_DIRECT_MESSAGE_REACTIONS = 1 << 13;
static const int INTENT_DIRECT_MESSAGE_TYPING = 1 << 14;
static const int INTENT_MESSAGE_CONTENT = 1 << 15;
static const int INTENT_GUILD_SCHEDULED_EVENTS = 1 << 16;
static const int INTENT_AUTO_MODERATION_CONFIGURATION = 1 << 20;
static const int INTENT_AUTO_MODERATION_EXECUTION = 1 << 21;

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
            revokeToken();
        }
        else
        {
            QDesktopServices::openUrl(QUrl(QString("https://discord.com/api/v10/oauth2/authorize?client_id=%1&redirect_uri=http%3A%2F%2Flocalhost%3A8356%2Fchat_service%2Fdiscord%2Fauth_code&&response_type=code&scope=messages.read%20guilds%20identify").arg(ClientID)));
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

    QObject::connect(&timerValidateToken, &QTimer::timeout, this, &Discord::requestMe);
    timerValidateToken.setInterval(60 * 60 * 1000);
    timerValidateToken.start();

    requestMe();
    updateAuthState();

    reconnect();
}

ChatService::ConnectionStateType Discord::getConnectionStateType() const
{
    if (state.connected)
    {
        return ChatService::ConnectionStateType::Connected;
    }
    else if (isAuthorized() && enabled.get())
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
        const QString errorDescription = request.getUrlQuery().queryItemValue("error_description").replace('+', ' ');

        if (code.isEmpty())
        {
            if (errorDescription.isEmpty())
            {
                return TcpReply::createTextHtmlError("Code is empty");
            }
            else
            {
                return TcpReply::createTextHtmlError(errorDescription);
            }
        }

        requestOAuthToken(code);

        return TcpReply::createTextHtmlOK(tr("Now you can close the page and return to %1").arg(QCoreApplication::applicationName()));
    }

    return TcpReply::createTextHtmlError("Unknown path");
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

    QNetworkReply* reply = network.get(QNetworkRequest(QUrl("https://discord.com/api/v10/gateway")));
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const QByteArray data = reply->readAll();
        reply->deleteLater();

        QString url = QJsonDocument::fromJson(data).object().value("url").toString();
        if (url.isEmpty())
        {
            qCritical() << Q_FUNC_INFO << "url is empty";
        }
        else
        {
            if (url.back() != '/')
            {
                url += '/';
            }

            url += "?v=10&encoding=json";

            socket.setProxy(network.proxy());
            socket.open(QUrl(url));
        }
    });
}

void Discord::onWebSocketReceived(const QString &rawData)
{
    qDebug("received:\n" + rawData.toUtf8() + "\n");

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
    if (!isAuthorized() || socket.state() != QAbstractSocket::SocketState::ConnectedState)
    {
        return;
    }

    const QJsonObject data =
    {
        { "token", oauthToken.get() },
        { "properties", QJsonObject(
          {
              { "os", QSysInfo::productType() },
              { "browser", QCoreApplication::applicationName() },
              { "device", QCoreApplication::applicationName() },
          })
        },

        { "intents", INTENT_GUILD_MESSAGES },
    };

    send(IdentifyOpCode, data);
}

bool Discord::isAuthorized() const
{
    return !oauthToken.get().trimmed().isEmpty() && requestedMeSuccess;
}

void Discord::updateAuthState()
{
    if (!authStateInfo)
    {
        qCritical() << Q_FUNC_INFO << "!authStateInfo";
    }

    if (isAuthorized())
    {
        QString text = tr("Authorized");
        if (!userName.isEmpty())
        {
            text = tr("Authorized as %1").arg("<b>" + userName + "</b>");
        }

        authStateInfo->setItemProperty("text", "<img src=\"qrc:/resources/images/tick.svg\" width=\"20\" height=\"20\"> " + text);
        loginButton->setItemProperty("text", tr("Logout"));
    }
    else
    {
        authStateInfo->setItemProperty("text", "<img src=\"qrc:/resources/images/error-alt-svgrepo-com.svg\" width=\"20\" height=\"20\"> " + tr("Not authorized"));
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

void Discord::requestOAuthToken(const QString &code)
{
    QNetworkRequest request(QUrl("https://discord.com/api/v10/oauth2/token"));
    request.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader, "application/x-www-form-urlencoded");

    QNetworkReply* reply = network.post(request,
                                        ("client_id=" + ClientID +
                                        "&client_secret=" + OBFUSCATE(DISCORD_SECRET) +
                                        "&code=" + code +
                                        "&grant_type=authorization_code"
                                        "&redirect_uri=" + getRedirectUri()).toUtf8());

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const QByteArray data = reply->readAll();
        reply->deleteLater();

        const QJsonObject root = QJsonDocument::fromJson(data).object();

        const QString token = root.value("access_token").toString();
        if (token.isEmpty())
        {
            qCritical() << Q_FUNC_INFO << "token ith empty";
            return;
        }

        oauthToken.set(token);
        requestMe();
    });
}

void Discord::requestMe()
{
    QNetworkRequest request(QUrl("https://discord.com/api/v10/oauth2/@me"));
    request.setRawHeader("Authorization", QByteArray("Bearer ") + oauthToken.get().toUtf8());
    QNetworkReply* reply = network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const QByteArray data = reply->readAll();
        reply->deleteLater();

        const QJsonObject root = QJsonDocument::fromJson(data).object();

        requestedMeSuccess = root.contains("application");
        if (!requestedMeSuccess)
        {
            revokeToken();
        }

        if (root.contains("user"))
        {
            userName = root.value("user").toObject().value("username").toString();
        }

        updateAuthState();
    });
}

QString Discord::getRedirectUri() const
{
    return "http://localhost:" + QString("%1").arg(TcpServer::Port) + "/chat_service/" + getServiceTypeId(getServiceType()) + "/auth_code";
}

void Discord::revokeToken()
{
    if (!oauthToken.get().isEmpty())
    {
        QNetworkRequest request(QUrl("https://discord.com/api/v10/oauth2/token/revoke"));
        request.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader, "application/x-www-form-urlencoded");

        QNetworkReply* reply = network.post(request,
                                            ("token=" + oauthToken.get()).toUtf8());
        connect(reply, &QNetworkReply::finished, this, [this, reply]()
        {
            const QByteArray data = reply->readAll();
            reply->deleteLater();

            // TODO: {"error": "invalid_client"}
        });
    }

    oauthToken.set(QString());
    userName = QString();
    requestedMeSuccess = false;

    updateAuthState();
}

void Discord::send(const int opCode, const QJsonValue &data)
{
    QJsonObject message =
    {
        { "op", opCode},
        { "d", data },
    };

    qDebug("send:\n" + QJsonDocument(message).toJson() + "\n");

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
