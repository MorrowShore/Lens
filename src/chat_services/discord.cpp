#include "discord.h"
#include "secrets.h"
#include "models/message.h"
#include "string_obfuscator/obfuscator.hpp"
#include <QDesktopServices>
#include <QTcpSocket>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSysInfo>

namespace
{

static const int ReconncectPeriod = 3 * 1000;

static const int DispatchOpCode = 0;
static const int HeartbeatOpCode = 1;
static const int IdentifyOpCode = 2;
static const int InvalidSessionOpCode = 9;
static const int HelloOpCode = 10;
static const int HeartAckbeatOpCode = 11;

// https://discord.com/developers/docs/topics/gateway#gateway-intents
static const int INTENT_GUILD_MESSAGES = 1 << 9;
static const int INTENT_MESSAGE_CONTENT = 1 << 15;

}

Discord::Discord(QSettings &settings_, const QString &settingsGroupPath, QNetworkAccessManager &network_, QObject *parent)
    : ChatService(settings_, settingsGroupPath, AxelChat::ServiceType::Discord, parent)
    , settings(settings_)
    , network(network_)
    , applicationId(settings_, settingsGroupPath + "/client_id")
    , botToken(settings_, settingsGroupPath + "/bot_token")
{
    getUIElementBridgeBySetting(stream)->setItemProperty("visible", false);

    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLabel("1. " + tr("Create an app in the Discord Developer Portal"))));

    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createButton(tr("Open Discord Developer Portal"), []()
    {
        QDesktopServices::openUrl(QUrl("https://discord.com/developers"));
    })));

    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLabel("2. " + tr("Find and paste the Application ID below"))));
    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLineEdit(&applicationId, tr("Application ID"), "0000000000000000000", true)));

    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLabel("3. " + tr("Create a bot (in Bot section)"))));
    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLabel("4. " + tr("Allow the bot to read the message content (Message Content Intent checkbox)"))));
    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLabel("5. " + tr("Reset the token (button Reset Token). The bot's previous token will become invalid"))));

    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLabel("6. " + tr("Copy the token and paste below. DON'T DISCLOSE THE BOT'S TOKEN!"))));
    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLineEdit(&botToken, tr("Bot token"), "0000000AAAAAAAAAAA0000000000BBBBBBBBBBBB000000000CCCCCCCCCC0000000000000", true)));

    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLabel("7. " + tr("Add the bot to the servers you need, while allowing reading messages"))));
    connectBotToGuild = std::shared_ptr<UIElementBridge>(UIElementBridge::createButton(tr("Add bot to server"), [this]()
    {
        QDesktopServices::openUrl(QUrl("https://discord.com/api/oauth2/authorize"
                                       "?permissions=1024"
                                       "&scope=bot"
                                       "&client_id=" + applicationId.get().trimmed()));
    }));
    addUIElement(connectBotToGuild);

    applicationId.setCallbackValueChanged([this](const QString&) { updateUI(); });
    botToken.setCallbackValueChanged([this](const QString&) { updateUI(); });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &Discord::onWebSocketReceived);

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state)
    {
        Q_UNUSED(state)
        //qDebug() << Q_FUNC_INFO << ": WebSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        //qDebug() << Q_FUNC_INFO << ": WebSocket connected";

        heartbeatAcknowledgementTimer.setInterval(60 * 10000);
        heartbeatAcknowledgementTimer.start();
    });

    QObject::connect(&socket, &QWebSocket::disconnected, this, [this]()
    {
        //qDebug() << Q_FUNC_INFO << ": WebSocket disconnected";
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

        if (!state.connected)
        {
            reconnect();
        }
    });
    timerReconnect.start(ReconncectPeriod);

    reconnect();
}

ChatService::ConnectionStateType Discord::getConnectionStateType() const
{
    if (state.connected)
    {
        return ChatService::ConnectionStateType::Connected;
    }
    else if (isCanConnect() && enabled.get())
    {
        return ChatService::ConnectionStateType::Connecting;
    }

    return ChatService::ConnectionStateType::NotConnected;
}

QString Discord::getStateDescription() const
{
    if (applicationId.get().isEmpty())
    {
        return tr("Application ID not specified");
    }

    if (botToken.get().isEmpty())
    {
        return tr("Bot token not specified");
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

void Discord::onUiElementChangedImpl(const std::shared_ptr<UIElementBridge> element)
{
    if (!element)
    {
        qCritical() << Q_FUNC_INFO << "!element";
        return;
    }

    Setting<QString>* setting = element->getSettingString();
    if (!setting)
    {
        return;
    }

    if (*&setting == &applicationId || *&setting == &botToken)
    {
        reconnect();
    }
}

void Discord::reconnectImpl()
{
    socket.close();

    state = State();
    info = Info();

    processDisconnected();

    if (!isCanConnect() || !enabled.get())
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
    else if (opCode == HeartbeatOpCode)
    {
        sendHeartbeat();
    }
    else if (opCode == InvalidSessionOpCode)
    {
        parseInvalidSession(root.value("d").toBool());
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
    if (!isCanConnect() || socket.state() != QAbstractSocket::SocketState::ConnectedState)
    {
        return;
    }

    const QJsonObject data =
    {
        { "token", botToken.get() },
        { "properties", QJsonObject(
          {
              { "os", QSysInfo::productType() },
              { "browser", QCoreApplication::applicationName() },
              { "device", QCoreApplication::applicationName() },
          })
        },

        { "intents", INTENT_GUILD_MESSAGES | INTENT_MESSAGE_CONTENT },
    };

    send(IdentifyOpCode, data);
}

bool Discord::isCanConnect() const
{
    return !applicationId.get().isEmpty() && !botToken.get().isEmpty();
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

void Discord::processConnected()
{
    if (!state.connected)
    {
        state.connected = true;
        emit stateChanged();
        emit connectedChanged(true, QString());
    }
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
        processConnected();
    }
    else if (eventType == "MESSAGE_CREATE")
    {
        parseMessageCreate(data);
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "unkown event type" << eventType;
    }
}

void Discord::parseInvalidSession(const bool resumableSession)
{
    //TODO
    reconnect();
}

void Discord::parseMessageCreate(const QJsonObject &jsonMessage)
{
    QList<Message> messages;
    QList<Author> authors;

    const QJsonObject jsonAuthor = jsonMessage.value("author").toObject();

    const QString userName = jsonAuthor.value("username").toString();
    const QString displayName = jsonAuthor.value("display_name").toString();
    const QString authorName = displayName.isEmpty() ? userName : displayName;
    const QString userId = jsonAuthor.value("id").toString();
    const QString avatarHash = jsonAuthor.value("avatar").toString();

    //TODO: user page
    //TODO: timestamp
    //TODO: type
    //TODO: flags
    //TODO: etc
    //TODO: channel name
    //TODO: guild name

    static const int AvatarSize = 256;

    const Author author(getServiceType(),
                  authorName,
                  userId,
                  QUrl(QString("https://cdn.discordapp.com/avatars/%1/%2.png?size=%3").arg(userId, avatarHash).arg(AvatarSize)),
                  QUrl("https://discordapp.com/users/" + userId));
    authors.append(author);

    const QString messageId = jsonMessage.value("id").toString();
    const QString messageContent = jsonMessage.value("content").toString();

    if (messageContent.isEmpty())
    {
        qWarning() << Q_FUNC_INFO << "message content is empty";
        qDebug() << jsonMessage;
        return;
    }

    QList<Message::Content*> contents;
    contents.append(new Message::Text(messageContent));

    messages.append(Message(contents, author, QDateTime::currentDateTime(), QDateTime::currentDateTime(), messageId));

    emit readyRead(messages, authors);
}

void Discord::updateUI()
{
    connectBotToGuild->setItemProperty("enabled", isCanConnect());
}
