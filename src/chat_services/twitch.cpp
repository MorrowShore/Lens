#include "twitch.h"
#include "secrets.h"
#include "crypto/crypto.h"
#include "models/messagesmodel.h"
#include "models/author.h"
#include "models/message.h"
#include "crypto/obfuscator.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDesktopServices>
#include <QAbstractSocket>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QColor>

namespace
{

static const QString TwitchIRCHost = "tmi.twitch.tv";
static const QString FolderLogs = "logs_twitch";
static const QString ClientID = OBFUSCATE(TWITCH_CLIENT_ID);

static const int ReconncectPeriod = 5 * 1000;
static const int PingPeriod = 60 * 1000;
static const int PongTimeout = 5 * 1000;
static const int UpdateStreamInfoPeriod = 10 * 1000;

}

Twitch::Twitch(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network_, cweqt::Manager&, QObject *parent)
  : ChatService(settings, settingsGroupPathParent, AxelChat::ServiceType::Twitch, true, parent)
  , network(network_)
    , authStateInfo(UIBridgeElement::createLabel("Loading..."))
  , auth(settings, getSettingsGroupPath() + "/auth", network)
{
    uiBridge.findBySetting(stream)->setItemProperty("name", tr("Channel"));
    uiBridge.findBySetting(stream)->setItemProperty("placeholderText", tr("Link or channel name..."));

    uiBridge.addElement(authStateInfo);

    OAuth2::Config config;
    config.flowType = OAuth2::FlowType::AuthorizationCode;
    config.clientId = ClientID;
    config.clientSecret = OBFUSCATE(TWITCH_SECRET);
    config.authorizationPageUrl = "https://id.twitch.tv/oauth2/authorize";
    config.redirectUrl = "http://localhost:" + QString("%1").arg(TcpServer::Port) + "/chat_service/" + getServiceTypeId(getServiceType()) + "/auth_code";
    config.scope = "openid+chat:read";
    config.requestTokenUrl = "https://id.twitch.tv/oauth2/token";
    config.validateTokenUrl = "https://id.twitch.tv/oauth2/validate";
    config.refreshTokenUrl = "https://id.twitch.tv/oauth2/token";
    config.revokeTokenUrl = "https://id.twitch.tv/oauth2/revoke";
    auth.setConfig(config);
    QObject::connect(&auth, &OAuth2::stateChanged, this, &Twitch::onAuthStateChanged);
    
    loginButton = std::shared_ptr<UIBridgeElement>(UIBridgeElement::createButton(tr("Login"), [this]()
    {
        if (auth.isLoggedIn())
        {
            auth.logout();
        }
        else
        {
            auth.login();
        }
    }));
    uiBridge.addElement(loginButton);
    
    connect(&uiBridge, QOverload<const std::shared_ptr<UIBridgeElement>&>::of(&UIBridge::elementChanged), this, [](const std::shared_ptr<UIBridgeElement>& element)
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
    });

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state){
        Q_UNUSED(state)
        //qDebug() << Q_FUNC_INFO << "webSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &Twitch::onIRCMessage);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]()
    {
        //qDebug() << Q_FUNC_INFO << "webSocket connected";

        if (state.connected)
        {
            state.connected = false;
            emit stateChanged();
        }

        state.viewersCount = -1;

        sendIRCMessage("CAP REQ :twitch.tv/tags twitch.tv/commands");
        sendIRCMessage("PASS SCHMOOPIIE");
        sendIRCMessage("NICK justinfan12348");
        sendIRCMessage("USER justinfan12348 8 * :justinfan12348");
        sendIRCMessage("JOIN #" + state.streamId);
        sendIRCMessage("PING :" + TwitchIRCHost);

        requestUserInfo(state.streamId);
        requestStreamInfo(state.streamId);
    });

    QObject::connect(&socket, &QWebSocket::disconnected, this, [this]()
    {
        //qDebug() << Q_FUNC_INFO << "webSocket disconnected";

        if (state.connected)
        {
            state.connected = false;
            emit connectedChanged(false);
            emit stateChanged();
        }

        state.viewersCount = -1;
    });

    QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError error_){
        qDebug() << Q_FUNC_INFO << "webSocket error:" << error_ << ":" << socket.errorString();
    });

    QObject::connect(&timerReconnect, &QTimer::timeout, this, [this]()
    {
        if (!enabled.get())
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

    QObject::connect(&timerCheckPong, &QTimer::timeout, this, [this]()
    {
        if (!enabled.get())
        {
            return;
        }

        if (state.connected)
        {
            qWarning() << Q_FUNC_INFO << "Pong timeout! Reconnection...";

            auto author = Author::getSoftwareAuthor();

            QList<std::shared_ptr<Message::Content>> contents = { std::make_shared<Message::Text>(tr("Ping timeout! Reconnection...")) };

            std::shared_ptr<Message> message = std::make_shared<Message>(contents, author);

            QList<std::shared_ptr<Message>> messages;
            messages.append(message);

            QList<std::shared_ptr<Author>> authors;
            authors.append(author);

            emit readyRead(messages, authors);

            state.connected = false;

            emit connectedChanged(false);
            emit stateChanged();
            reconnect();
        }
    });

    QObject::connect(&timerPing, &QTimer::timeout, this, [this]()
    {
        if (!enabled.get())
        {
            return;
        }

        if (state.connected)
        {
            sendIRCMessage(QString("PING :") + TwitchIRCHost);
            timerCheckPong.start(PongTimeout);
        }
    });
    timerPing.start(PingPeriod);

    QObject::connect(&timerUpdaetStreamInfo, &QTimer::timeout, this, [this]()
    {
        if (!enabled.get())
        {
            return;
        }

        if (state.connected)
        {
            requestStreamInfo(state.streamId);
        }
    });
    timerUpdaetStreamInfo.start(UpdateStreamInfoPeriod);

    onAuthStateChanged();
}

ChatService::ConnectionStateType Twitch::getConnectionStateType() const
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

QString Twitch::getStateDescription() const
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

TcpReply Twitch::processTcpRequest(const TcpRequest &request)
{
    const QString path = request.getUrl().path().toLower();

    if (path == "/auth_code")
    {
        return auth.processRedirect(request);
    }

    return TcpReply::createTextHtmlError("Unknown path");
}

void Twitch::sendIRCMessage(const QString &message)
{
    //qDebug() << Q_FUNC_INFO << "send:" << message.toUtf8() << "\n";
    socket.sendTextMessage(message);
}

void Twitch::reconnectImpl()
{
    socket.close();

    state = State();
    info = Info();

    stream.set(stream.get().toLower().trimmed());

    state.controlPanelUrl = QUrl(QString("https://dashboard.twitch.tv/stream-manager"));

    if (stream.get().isEmpty())
    {
        emit stateChanged();
        return;
    }

    QRegExp rx;

    // user channel

    const QString simpleUserSpecifiedUserChannel = AxelChat::simplifyUrl(stream.get());
    rx = QRegExp("^twitch.tv/([^/]*)$", Qt::CaseInsensitive);
    if (rx.indexIn(simpleUserSpecifiedUserChannel) != -1)
    {
        state.streamId = rx.cap(1);
    }

    if (state.streamId.isEmpty())
    {
        rx = QRegExp("^[a-zA-Z0-9_]+$", Qt::CaseInsensitive);
        if (rx.indexIn(stream.get()) != -1)
        {
            state.streamId = stream.get();
        }
    }

    if (!state.streamId.isEmpty())
    {
        state.chatUrl = QUrl(QString("https://www.twitch.tv/popout/%1/chat").arg(state.streamId));
        state.streamUrl = QUrl(QString("https://www.twitch.tv/%1").arg(state.streamId));

        if (enabled.get())
        {
            socket.setProxy(network.proxy());
            socket.open(QUrl("wss://irc-ws.chat.twitch.tv:443"));
        }
    }
}

void Twitch::onIRCMessage(const QString &rawData)
{
    if (!enabled.get())
    {
        return;
    }

    if (timerCheckPong.isActive())
    {
        timerCheckPong.stop();
    }

    QList<std::shared_ptr<Message>> messages;
    QList<std::shared_ptr<Author>> authors;

    const QVector<QStringRef> rawMessages = rawData.splitRef("\r\n");
    for (const QStringRef& raw : rawMessages)
    {
        std::set<Message::Flag> messageFlags;

        QString rawMessage = raw.trimmed().toString();
        if (rawMessage.isEmpty())
        {
            continue;
        }

        //qDebug() << Q_FUNC_INFO << "received:" << rawMessage.toUtf8() << "\n";

        if (rawMessage.startsWith("PING", Qt::CaseSensitivity::CaseInsensitive))
        {
            sendIRCMessage(QString("PONG :") + TwitchIRCHost);
        }

        if (!state.connected && rawMessage.startsWith(':') && rawMessage.count(':') == 1 && rawMessage.contains("JOIN #", Qt::CaseSensitivity::CaseInsensitive))
        {
            state.connected = true;
            emit connectedChanged(true);
            emit stateChanged();
        }

        if (rawMessage.startsWith(":" + TwitchIRCHost))
        {
            // service messages should not be displayed
            continue;
        }

        // user message
        //@badge-info=;badges=broadcaster/1;client-nonce=24ce478a2773ed000a5553c13c1a3e05;color=#8A2BE2;display-name=Axe1_k;emotes=;flags=;id=3549a9a1-a4b2-4c25-876d-b514a89506b0;mod=0;room-id=215601682;subscriber=0;tmi-sent-ts=1627320332721;turbo=0;user-id=215601682;user-type= :axe1_k!axe1_k@axe1_k.tmi.twitch.tv PRIVMSG #axe1_k :2332

        if (!rawMessage.startsWith("@"))
        {
            continue;
        }

        QString messageType;
        static const QRegExp rxMessageType(".tmi.twitch.tv\\s+([a-zA-Z0-9]+)\\s+#");
        const int posSnippet1 = rxMessageType.indexIn(rawMessage);
        if (posSnippet1 != -1)
        {
            messageType = rxMessageType.cap(1).trimmed().toUpper();
        }
        else
        {
            continue;
        }

        static const QSet<QString> MessagesTypes = { "PRIVMSG", "NOTICE", "USERNOTICE" };
        if (!MessagesTypes.contains(messageType))
        {
            continue;
        }

        const QString snippet1 = rxMessageType.cap(0);
        const QString snippet2 = rawMessage.mid(posSnippet1 + snippet1.length()); // [owner-channel-id] :[message-text]

        QString rawMessageText = snippet2.mid(snippet2.indexOf(':') + 1);

        if (rawMessageText.startsWith(QString("\u0001ACTION")) && rawMessageText.endsWith("\u0001"))
        {
            rawMessageText = rawMessageText.mid(7);
            rawMessageText = rawMessageText.left(rawMessageText.length() - 1);
            rawMessageText = rawMessageText.trimmed();
            messageFlags.insert(Message::Flag::TwitchAction);
        }

        const QString snippet3 = rawMessage.left(posSnippet1); // [@tags] :[user-id]![user-id]@[channel-id]

        QList<std::shared_ptr<Message::Content>> contents;
        QString login;
        QString displayName;
        QColor nicknameColor;
        QStringList badges;
        QVector<MessageEmoteInfo> emotesInfo;
        QHash<Message::ColorRole, QColor> forcedColors;

        static const QRegExp rxLogin("\\:([a-zA-Z0-9_]+)\\!");
        if (rxLogin.indexIn(snippet3) != -1)
        {
            login = rxLogin.cap(1);
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "not found login, message =" << rawMessage;
        }

        const QString tagsSnippet = snippet3.left(snippet3.lastIndexOf(":")).mid(1).trimmed();
        const QVector<QStringRef> rawTags = tagsSnippet.splitRef(";", Qt::SplitBehaviorFlags::SkipEmptyParts);
        for (const QStringRef& tag : rawTags)
        {
            // https://dev.twitch.tv/docs/irc/tags

            if (!tag.contains("="))
            {
                qWarning() << Q_FUNC_INFO << ": tag" << tag << "not contains '=', raw message:" << rawMessage;
                continue;
            }

            const QStringRef tagName = tag.left(tag.indexOf('='));
            const QString tagValue = tag.mid(tag.indexOf('=') + 1).toString().replace("\\s", " ");

            if (tagName == "color")
            {
                nicknameColor = QColor(tagValue);
            }
            /*else if (tagName == "login")
            {
                login = tagValue;
            }*/
            else if (tagName == "display-name")
            {
                displayName = tagValue;
            }
            else if (tagName == "msg-id")
            {
                if (!tagValue.trimmed().isEmpty())
                {
                    forcedColors.insert(Message::ColorRole::BodyBackgroundColorRole, QColor(117, 94, 188));
                }
            }
            else if (tagName == "system-msg")
            {
                Message::TextStyle style;
                style.bold = true;

                contents.append(std::make_shared<Message::Text>(tagValue + "\n", style));
            }
            else if (tagName == "emotes")
            {
                if (tagValue.isEmpty())
                {
                    continue;
                }

                // http://static-cdn.jtvnw.net/emoticons/v1/:<emote ID>/:<size>
                // https://static-cdn.jtvnw.net/emoticons/v2/:<emote ID>/default/light/:<size>
                // "166266:20-28/64138:113-121" "244:8-16" "1902:36-40" "558563:5-11"
                const QVector<QStringRef> emotesPartInfo = tagValue.splitRef('/', Qt::SplitBehaviorFlags::SkipEmptyParts);
                for (const QStringRef& emotePartInfo : emotesPartInfo)
                {
                    const QStringRef emoteId = emotePartInfo.left(emotePartInfo.indexOf(':'));
                    if (emoteId.isEmpty())
                    {
                        continue;
                    }

                    MessageEmoteInfo emoteInfo;
                    emoteInfo.id = emoteId.toString();

                    const QStringRef emoteIndexesInfoPart = emotePartInfo.mid(emotePartInfo.indexOf(':') + 1);
                    const QVector<QStringRef> emoteIndexesPairsPart = emoteIndexesInfoPart.split(",", Qt::SplitBehaviorFlags::SkipEmptyParts);
                    for (const QStringRef& emoteIndexesPair : emoteIndexesPairsPart)
                    {
                        const QVector<QStringRef> emoteIndexes = emoteIndexesPair.split('-', Qt::SplitBehaviorFlags::SkipEmptyParts);
                        if (emoteIndexes.size() != 2)
                        {
                            qDebug() << Q_FUNC_INFO << "emoteIndexes.size() != 2, emotesPartInfo =" << emotesPartInfo;
                            continue;
                        }

                        bool ok = false;
                        const int firstIndex = emoteIndexes[0].toInt(&ok);
                        if (!ok)
                        {
                            qDebug() << Q_FUNC_INFO << "!ok for firstIndex, emoteIndexes[0] =" << emoteIndexes[0] << ", emotesPartInfo =" << emotesPartInfo;
                            continue;
                        }

                        ok = false;
                        const int lastIndex = emoteIndexes[1].toInt(&ok);
                        if (!ok)
                        {
                            qDebug() << Q_FUNC_INFO << "!ok for firstIndex, emoteIndexes[1] =" << emoteIndexes[1] << ", emotesPartInfo =" << emotesPartInfo;
                            continue;
                        }

                        emoteInfo.indexes.append(QPair<int, int>(firstIndex, lastIndex));
                    }

                    if (emoteInfo.isValid())
                    {
                        emotesInfo.append(emoteInfo);
                    }
                    else
                    {
                        qDebug() << Q_FUNC_INFO << "emoteInfo not valid, emotesPartInfo =" << emotesPartInfo;
                    }
                }
            }
            else if (tagName == "badges")
            {
                const QVector<QStringRef> badgesInfo = tagValue.splitRef(',', Qt::SplitBehaviorFlags::SkipEmptyParts);
                for (const QStringRef& badgeInfo : badgesInfo)
                {
                    const QString badgeInfoStr = badgeInfo.toString();
                    if (badgesUrls.contains(badgeInfoStr))
                    {
                        badges.append(badgesUrls[badgeInfoStr]);
                    }
                    else
                    {
                        qWarning() << Q_FUNC_INFO << "unknown badge" << badgeInfoStr;
                        badges.append("qrc:/resources/images/unknown-badge.png");
                    }
                }
            }
        }

        if (login.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "login is empty. Login will be taken from the display name" << displayName << ", message =" << rawMessage;
            login = displayName.toLower();
        }

        if (login != displayName.toLower())
        {
            //qWarning() << Q_FUNC_INFO << "login and display name not matching, login =" << login << ", display name =" << displayName << ", message =" << rawMessage;
        }

        std::set<Author::Flag> authorFlags;

        if (badges.contains("partner/1"))
        {
            authorFlags.insert(Author::Flag::Verified);
        }

        if (badges.contains("broadcaster/1"))
        {
            authorFlags.insert(Author::Flag::ChatOwner);
        }

        if (badges.contains("moderator/1"))
        {
            authorFlags.insert(Author::Flag::Moderator);
        }

        std::shared_ptr<Author> author = std::make_shared<Author>(
            getServiceType(),
            displayName,
            login,
            QUrl(),
            QUrl(QString("https://www.twitch.tv/%1").arg(login)),
            badges,
            QStringList(),
            authorFlags,
            nicknameColor);

        if (emotesInfo.isEmpty())
        {
            if (!rawMessageText.isEmpty())
            {
                contents.append(std::make_shared<Message::Text>(rawMessageText));
            }
        }
        else
        {
            QString textChunk;

            for (int i = 0; i < rawMessageText.length(); ++i)
            {
                bool needIgnoreChar = false;

                for (const MessageEmoteInfo& emoteInfo : emotesInfo)
                {
                    for (const QPair<int, int> indexPair : emoteInfo.indexes)
                    {
                        if (i >= indexPair.first && i <= indexPair.second)
                        {
                            needIgnoreChar = true;
                        }

                        if (i == indexPair.first)
                        {
                            if (!textChunk.isEmpty())
                            {
                                contents.append(std::make_shared<Message::Text>(textChunk));
                                textChunk.clear();
                            }

                            static const QString sizeUrlPart = "1.0";
                            const QString emoteUrl = QString("https://static-cdn.jtvnw.net/emoticons/v2/%1/default/light/%2").arg(emoteInfo.id, sizeUrlPart);

                            contents.append(std::make_shared<Message::Image>(emoteUrl));
                        }
                    }
                }

                if (!needIgnoreChar)
                {

                    textChunk += rawMessageText[i];
                }
            }

            if (!textChunk.isEmpty())
            {
                contents.append(std::make_shared<Message::Text>(textChunk));
                textChunk.clear();
            }
        }

        std::shared_ptr<Message> message = std::make_shared<Message>(
            contents,
            author,
            QDateTime::currentDateTime(),
            QDateTime::currentDateTime(),
            QString(),
            messageFlags,
            forcedColors);

        messages.append(message);
        authors.append(author);

        if (!info.usersInfoUpdated.contains(login))
        {
            requestUserInfo(login);
        }

        //qDebug() << Q_FUNC_INFO << authorName << ":" << messageText;
    }

    if (!messages.isEmpty())
    {
        emit readyRead(messages, authors);
    }
}

void Twitch::requestGlobalBadges()
{
    QNetworkRequest request(QString("https://api.twitch.tv/helix/chat/badges/global"));
    request.setRawHeader("Client-ID", ClientID.toUtf8());
    request.setRawHeader("Authorization", QByteArray("Bearer ") + auth.getAccessToken().toUtf8());
    QObject::connect(network.get(request), &QNetworkReply::finished, this, &Twitch::onReplyBadges);
}

void Twitch::requestChannelBadges(const ChannelInfo& channelInfo)
{
    if (channelInfo.id.isEmpty())
    {
        return;
    }

    QNetworkRequest request("https://api.twitch.tv/helix/chat/badges?broadcaster_id=" + channelInfo.id);
    request.setRawHeader("Client-ID", ClientID.toUtf8());
    request.setRawHeader("Authorization", QByteArray("Bearer ") + auth.getAccessToken().toUtf8());
    QObject::connect(network.get(request), &QNetworkReply::finished, this, &Twitch::onReplyBadges);
}

void Twitch::onReplyBadges()
{
    QNetworkReply* reply = dynamic_cast<QNetworkReply*>(sender());
    QByteArray data;
    if (!checkReply(reply, Q_FUNC_INFO, data))
    {
        return;
    }

    parseBadgesJson(data);
}

void Twitch::parseBadgesJson(const QByteArray &data)
{
    if (data.isEmpty())
    {
        qWarning() << Q_FUNC_INFO << "data is empty";
        return;
    }

    const QJsonArray badgesJson = QJsonDocument::fromJson(data).object().value("data").toArray();
    for (const QJsonValue& v : qAsConst(badgesJson))
    {
        const QJsonObject set = v.toObject();
        const QString setId = set.value("set_id").toString();
        const QJsonArray versions = set.value("versions").toArray();
        for (const QJsonValue& v : qAsConst(versions))
        {
            const QJsonObject version = v.toObject();
            const QString versionId = version.value("id").toString();
            const QString url = version.value("image_url_2x").toString();
            const QString badgeName = setId + "/" + versionId;
            badgesUrls.insert(badgeName, url);
        }
    }
}

void Twitch::onAuthStateChanged()
{
    if (!authStateInfo)
    {
        qCritical() << Q_FUNC_INFO << "!authStateInfo";
    }

    authorizedChannel = ChannelInfo();

    switch (auth.getState())
    {
    case OAuth2::State::NotLoggedIn:
        authStateInfo->setItemProperty("text", "<img src=\"qrc:/resources/images/error-alt-svgrepo-com.svg\" width=\"20\" height=\"20\"> " + tr("Login for full functionality"));
        loginButton->setItemProperty("text", tr("Login"));
        break;

    case OAuth2::State::LoginInProgress:
        authStateInfo->setItemProperty("text", tr("Login in progress..."));
        loginButton->setItemProperty("text", tr("Login"));
        break;

    case OAuth2::State::LoggedIn:
    {
        const QJsonObject authInfo = auth.getAuthorizationInfo();

        authorizedChannel.id = authInfo.value("user_id").toString();
        authorizedChannel.login = authInfo.value("login").toString();

        emit authorized(authorizedChannel);

        authStateInfo->setItemProperty("text", "<img src=\"qrc:/resources/images/tick.svg\" width=\"20\" height=\"20\"> " + tr("Logged in as %1").arg("<b>" + authorizedChannel.login + "</b>"));
        loginButton->setItemProperty("text", tr("Logout"));
        requestGlobalBadges();
        requestChannelBadges(authorizedChannel);
        reconnect();
        break;
    }
    }

    emit stateChanged();
}

bool Twitch::checkReply(QNetworkReply *reply, const char *tag, QByteArray &resultData)
{
    resultData.clear();

    if (!reply)
    {
        qWarning() << tag << tag << ": !reply";
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

        if (statusCode == 401)
        {
            auth.refresh();
            return false;
        }
        else if (statusCode != 200)
        {
            qWarning() << tag << ": status code:" << statusCode;
        }
    }

    if (resultData.isEmpty() && statusCode != 200)
    {
        qWarning() << tag << ": data is empty";
        return false;
    }

    const QJsonObject root = QJsonDocument::fromJson(resultData).object();
    if (root.contains("error"))
    {
        qWarning() << tag << "Error:" << resultData << ", request url =" << requestUrl.toString();

        return false;
    }

    return true;
}

void Twitch::requestUserInfo(const QString& login)
{
    if (!auth.isLoggedIn())
    {
        return;
    }

    QNetworkRequest request(QString("https://api.twitch.tv/helix/users?login=%1").arg(login));
    request.setRawHeader("Client-ID", ClientID.toUtf8());
    request.setRawHeader("Authorization", QByteArray("Bearer ") + auth.getAccessToken().toUtf8());
    QObject::connect(network.get(request), &QNetworkReply::finished, this, &Twitch::onReplyUserInfo);
}

void Twitch::onReplyUserInfo()
{
    QNetworkReply* reply = dynamic_cast<QNetworkReply*>(sender());
    QByteArray data;
    if (!checkReply(reply, Q_FUNC_INFO, data))
    {
        return;
    }

    const QJsonArray dataArr = QJsonDocument::fromJson(data).object().value("data").toArray();
    for (const QJsonValue& v : qAsConst(dataArr))
    {
        const QJsonObject vObj = v.toObject();

        const QString broadcasterId = vObj.value("id").toString();
        const QString channelLogin = vObj.value("login").toString();
        const QUrl profileImageUrl = QUrl(vObj.value("profile_image_url").toString());

        if (channelLogin.isEmpty())
        {
            continue;
        }

        info.usersInfoUpdated.insert(channelLogin);

        if (!profileImageUrl.isEmpty())
        {
            emit authorDataUpdated(channelLogin, { {Author::Role::AvatarUrl, profileImageUrl} });
        }

        if (channelLogin == state.streamId)
        {
            ChannelInfo channel;

            channel.id = broadcasterId;
            channel.login = channelLogin;

            info.connectedChannel = channel;

            requestChannelBadges(channel);

            emit connectedChannel(channel);
        }
    }
}

void Twitch::requestStreamInfo(const QString &login)
{
    if (!auth.isLoggedIn())
    {
        return;
    }

    QNetworkRequest request(QString("https://api.twitch.tv/helix/streams?user_login=%1").arg(login));
    request.setRawHeader("Client-ID", ClientID.toUtf8());
    request.setRawHeader("Authorization", QByteArray("Bearer ") + auth.getAccessToken().toUtf8());

    QNetworkReply* reply = network.get(request);
    if (!reply)
    {
        qDebug() << Q_FUNC_INFO << ": !reply";
        return;
    }

    QObject::connect(reply, &QNetworkReply::finished, this, &Twitch::onReplyStreamInfo);
}

void Twitch::onReplyStreamInfo()
{
    QNetworkReply* reply = dynamic_cast<QNetworkReply*>(sender());
    QByteArray data;
    if (!checkReply(reply, Q_FUNC_INFO, data))
    {
        return;
    }

    bool found = false;

    const QJsonArray dataArr = QJsonDocument::fromJson(data).object().value("data").toArray();
    for (const QJsonValue& v : qAsConst(dataArr))
    {
        const QJsonObject vObj = v.toObject();

        const QString channelLogin = vObj.value("user_login").toString();
        const int viewers = vObj.value("viewer_count").toInt();

        if (channelLogin.isEmpty())
        {
            continue;
        }

        if (channelLogin == state.streamId)
        {
            state.viewersCount = viewers;
            found = true;
            emit stateChanged();
        }
    }

    if (!found)
    {
        state.viewersCount = -1;
        emit stateChanged();
    }
}
