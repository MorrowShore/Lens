#include "twitch.h"
#include "apikeys.h"
#include "models/messagesmodel.h"
#include "models/author.h"
#include "models/message.h"
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

static const QByteArray AcceptLanguageNetworkHeaderName = ""; // "en-US;q=0.5,en;q=0.3";

static const QString RedirectUri = "https://twitchapps.com/tmi/";//"https://localhost";
static const QString TwitchIRCHost = "tmi.twitch.tv";

static const QString FolderLogs = "logs_twitch";

static const int ReconncectPeriod = 3 * 1000;
static const int PingPeriod = 60 * 1000;
static const int PongTimeout = 5 * 1000;
static const int UpdateStreamInfoPeriod = 10 * 1000;

static bool checkReply(QNetworkReply *reply, const char *tag, QByteArray& resultData)
{
    resultData.clear();

    if (!reply)
    {
        qWarning() << tag << ": !reply";
        return false;
    }

    resultData = reply->readAll();
    if (resultData.isEmpty())
    {
        qWarning() << tag << ": data is empty";
        return false;
    }

    const QJsonObject root = QJsonDocument::fromJson(resultData).object();
    if (root.contains("error"))
    {
        qWarning() << tag << "Error:" << resultData << ", request =" << reply->request().url().toString();
        return false;
    }

    return true;
}

}

Twitch::Twitch(QSettings& settings_, const QString& settingsGroupPath, QNetworkAccessManager& network_, QObject *parent)
  : ChatService(settings_, settingsGroupPath, AxelChat::ServiceType::Twitch, parent)
  , settings(settings_)
  , network(network_)
  , oauthToken(Setting<QString>(settings, settingsGroupPath + "/oauth_token"))
{
    getParameter(stream)->setPlaceholder(tr("Link or channel name..."));

    parameters.append(Parameter(new Setting<QString>(settings, QString()), tr("To display avatars and complete work, specify the OAuth-token:"), Parameter::Type::Label));
    parameters.append(Parameter(&oauthToken, tr("OAuth token"), Parameter::Type::String, { Parameter::Flag::PasswordEcho }));
    parameters.append(Parameter(new Setting<QString>(settings, QString()), tr("Get token"), Parameter::Type::Button, {}, [this](const QVariant&)
    {
        QDesktopServices::openUrl(requesGetAOuthTokenUrl());
    }));

    QObject::connect(&socket, &QWebSocket::stateChanged, this, [](QAbstractSocket::SocketState state){
        Q_UNUSED(state)
        //qDebug() << "Twitch: WebSocket state changed:" << state;
    });

    QObject::connect(&socket, &QWebSocket::textMessageReceived, this, &Twitch::onIRCMessage);

    QObject::connect(&socket, &QWebSocket::connected, this, [this]() {
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
        sendIRCMessage(QString("JOIN #") + state.streamId);
        sendIRCMessage(QString("PING :") + TwitchIRCHost);

        requestUserInfo(state.streamId);
        requestStreamInfo(state.streamId);
    });

    QObject::connect(&socket, &QWebSocket::disconnected, this, [this]()
    {
        if (state.connected)
        {
            state.connected = false;
            emit connectedChanged(false, lastConnectedChannelName);
            emit stateChanged();
        }

        state.viewersCount = -1;
    });

    QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError error_){
        qDebug() << "Twitch: WebSocket error:" << error_ << ":" << socket.errorString();
    });

    QObject::connect(&timerReconnect, &QTimer::timeout, this, [this]()
    {
        if (!state.connected && !oauthToken.get().isEmpty())
        {
            reconnect();
        }
    });
    timerReconnect.start(ReconncectPeriod);

    reconnect();

    QObject::connect(&timerCheckPong, &QTimer::timeout, this, [this]{
        if (state.connected)
        {
            qWarning() << Q_FUNC_INFO << "Pong timeout! Reconnection...";

            const Author& author = Author::getSoftwareAuthor();

            QList<Message::Content*> contents;

            Message::Text* text = new Message::Text(tr("Ping timeout! Reconnection..."));
            contents.append(text);

            Message message(contents, author);

            QList<Message> messages;
            messages.append(message);

            QList<Author> authors;
            authors.append(author);

            emit readyRead(messages, authors);

            state.connected = false;

            emit connectedChanged(false, lastConnectedChannelName);
            emit stateChanged();
            reconnect();
        }
    });

    QObject::connect(&timerPing, &QTimer::timeout, this, [this]{
        if (state.connected)
        {
            sendIRCMessage(QString("PING :") + TwitchIRCHost);
            timerCheckPong.start(PongTimeout);
        }
    });
    timerPing.start(PingPeriod);

    QObject::connect(&timerUpdaetStreamInfo, &QTimer::timeout, this, [this]{
        if (state.connected)
        {
            requestStreamInfo(state.streamId);
        }
    });
    timerUpdaetStreamInfo.start(UpdateStreamInfoPeriod);

    requestForGlobalBadges();
}

ChatService::ConnectionStateType Twitch::getConnectionStateType() const
{
    if (state.connected)
    {
        return ChatService::ConnectionStateType::Connected;
    }
    else if (!oauthToken.get().isEmpty() && !state.streamId.isEmpty())
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

        if (oauthToken.get().isEmpty())
        {
            return tr("OAuth token not specified");
        }

        return tr("Not connected");

    case ConnectionStateType::Connecting:
        return tr("Connecting...");

    case ConnectionStateType::Connected:
        return tr("Successfully connected!");

    }

    return "<unknown_state>";
}

QUrl Twitch::requesGetAOuthTokenUrl() const
{
    return QUrl("https://id.twitch.tv/oauth2/authorize?client_id=" + QString(TWITCH_CLIENT_ID)
                        + "&redirect_uri=" + RedirectUri
                        + "&response_type=token"
                        + "&scope=openid+chat:read");
}

void Twitch::onParameterChangedImpl(Parameter& parameter)
{
    Setting<QString>& setting = *parameter.getSetting();

    if (&setting == &oauthToken)
    {
        QString token = parameter.getSetting()->get();
        if (token.startsWith("oauth:", Qt::CaseSensitivity::CaseInsensitive))
        {
            token = token.mid(6);
            setting.set(token);
        }

        reconnect();
    }

    emit stateChanged();
}

void Twitch::sendIRCMessage(const QString &message)
{
    //qDebug() << "Twitch: send:" << message.toUtf8() << "\n";
    socket.sendTextMessage(message);
}

void Twitch::reconnect()
{
    socket.close();

    state = State();

    stream.set(stream.get().toLower().trimmed());

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
        state.controlPanelUrl = QUrl(QString("https://dashboard.twitch.tv/u/%1/stream-manager").arg(state.streamId));

        socket.setProxy(network.proxy());
        socket.open(QUrl("wss://irc-ws.chat.twitch.tv:443")); // ToDo: use SSL? wss://irc-ws.chat.twitch.tv:443
    }

    emit stateChanged();
}

void Twitch::onIRCMessage(const QString &rawData)
{
    if (timerCheckPong.isActive())
    {
        timerCheckPong.stop();
    }

    QList<Message> messages;
    QList<Author> authors;

    const QVector<QStringRef> rawMessages = rawData.splitRef("\r\n");
    for (const QStringRef& raw : rawMessages)
    {
        std::set<Message::Flag> messageFlags;

        QString rawMessage = raw.trimmed().toString();
        if (rawMessage.isEmpty())
        {
            continue;
        }

        //qDebug() << "Twitch: received:" << rawMessage.toUtf8() << "\n";

        if (rawMessage.startsWith("PING", Qt::CaseSensitivity::CaseInsensitive))
        {
            sendIRCMessage(QString("PONG :") + TwitchIRCHost);
        }

        if (!state.connected && rawMessage.startsWith(':') && rawMessage.count(':') == 1 && rawMessage.contains("JOIN #", Qt::CaseSensitivity::CaseInsensitive))
        {
            state.connected = true;
            lastConnectedChannelName = state.streamId;
            emit connectedChanged(true, state.streamId);
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

        const QString snippet3 = rawMessage.left(posSnippet1); // [@tags] :[channel-id]![channel-id]@[channel-id]

        QList<Message::Content*> contents;
        QString login;
        QString displayName;
        QColor nicknameColor;
        QStringList badges;
        QVector<MessageEmoteInfo> emotesInfo;
        QHash<Message::ColorRole, QColor> forcedColors;

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
            else if (tagName == "login")
            {
                login = tagValue;
            }
            else if (tagName == "display-name")
            {
                displayName = tagValue;
            }
            else if (tagName == "msg-id")
            {
                if (!tagValue.trimmed().isEmpty())
                {
                    forcedColors.insert(Message::ColorRole::BodyBackground, QColor(117, 94, 188));
                }
            }
            else if (tagName == "system-msg")
            {
                Message::Text::Style style;
                style.bold = true;

                contents.append(new Message::Text(tagValue + "\n", style));
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
            login = displayName.toLower();
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

        const Author author(getServiceType(),
                                displayName,
                                login,
                                QUrl(),
                                QUrl(QString("https://www.twitch.tv/%1").arg(login)),
                                badges,
                                {},
                                authorFlags,
                                nicknameColor);

        if (emotesInfo.isEmpty())
        {
            if (!rawMessageText.isEmpty())
            {
                contents.append(new Message::Text(rawMessageText));
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
                                contents.append(new Message::Text(textChunk));
                                textChunk.clear();
                            }

                            static const QString sizeUrlPart = "1.0";
                            const QString emoteUrl = QString("https://static-cdn.jtvnw.net/emoticons/v2/%1/default/light/%2").arg(emoteInfo.id, sizeUrlPart);

                            contents.append(new Message::Image(emoteUrl));
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
                contents.append(new Message::Text(textChunk));
                textChunk.clear();
            }
        }

        const Message message = Message(contents,
                                        author,
                                        QDateTime::currentDateTime(),
                                        QDateTime::currentDateTime(),
                                        QString(),
                                        messageFlags,
                                        forcedColors);
        messages.append(message);
        authors.append(author);

        if (!usersInfoUpdated.contains(login))
        {
            requestUserInfo(login);
        }

        //qDebug() << "Twitch:" << authorName << ":" << messageText;
    }

    if (!messages.isEmpty())
    {
        emit readyRead(messages, authors);
    }
}

void Twitch::requestForGlobalBadges()
{
    QNetworkRequest request(QString("https://badges.twitch.tv/v1/badges/global/display"));
    request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, AxelChat::UserAgentNetworkHeaderName);
    request.setRawHeader("Accept-Language", AcceptLanguageNetworkHeaderName);
    QNetworkReply* reply = network.get(request);
    if (!reply)
    {
        qDebug() << Q_FUNC_INFO << ": !reply";
        return;
    }

    QObject::connect(reply, &QNetworkReply::finished, this, &Twitch::onReplyBadges);
}

void Twitch::requestForChannelBadges(const QString &broadcasterId)
{
    QNetworkRequest request(QString("https://badges.twitch.tv/v1/badges/channels/%1/display").arg(broadcasterId));
    request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, AxelChat::UserAgentNetworkHeaderName);
    request.setRawHeader("Accept-Language", AcceptLanguageNetworkHeaderName);
    QNetworkReply* reply = network.get(request);
    if (!reply)
    {
        qDebug() << Q_FUNC_INFO << ": !reply";
        return;
    }

    QObject::connect(reply, &QNetworkReply::finished, this, &Twitch::onReplyBadges);
}

void Twitch::onReplyBadges()
{
    QNetworkReply* reply = dynamic_cast<QNetworkReply*>(sender());
    QByteArray data;
    if (!checkReply(reply, Q_FUNC_INFO, data))
    {
        return;
    }

    reply->deleteLater();

    parseBadgesJson(data);
}

void Twitch::parseBadgesJson(const QByteArray &data)
{
    if (data.isEmpty())
    {
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    const QJsonObject badgeSetsObj = doc.object().value("badge_sets").toObject();

    const QStringList badgesNames = badgeSetsObj.keys();
    for (const QString& badgeName : badgesNames)
    {
        const QJsonObject versionsObj = badgeSetsObj.value(badgeName).toObject().value("versions").toObject();
        const QStringList versions = versionsObj.keys();
        for (const QString& version : versions)
        {
            const QString url = versionsObj.value(version).toObject().value("image_url_1x").toString();
            if (!url.isEmpty())
            {
                badgesUrls.insert(badgeName + "/" + version, url);
            }
        }
    }
}

void Twitch::requestUserInfo(const QString& login)
{
    QNetworkRequest request(QString("https://api.twitch.tv/helix/users?login=%1").arg(login));
    request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, AxelChat::UserAgentNetworkHeaderName);
    request.setRawHeader("Accept-Language", AcceptLanguageNetworkHeaderName);
    request.setRawHeader("Client-ID", TWITCH_CLIENT_ID);
    request.setRawHeader("Authorization", QByteArray("Bearer ") + oauthToken.get().toUtf8());
    QNetworkReply* reply = network.get(request);
    if (!reply)
    {
        qDebug() << Q_FUNC_INFO << ": !reply";
        return;
    }

    QObject::connect(reply, &QNetworkReply::finished, this, &Twitch::onReplyUserInfo);
}

void Twitch::onReplyUserInfo()
{
    QNetworkReply* reply = dynamic_cast<QNetworkReply*>(sender());
    QByteArray data;
    if (!checkReply(reply, Q_FUNC_INFO, data))
    {
        return;
    }

    reply->deleteLater();

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

        usersInfoUpdated.insert(channelLogin);

        if (!profileImageUrl.isEmpty())
        {
            emit authorDataUpdated(channelLogin, { {Author::Role::AvatarUrl, profileImageUrl} });
        }

        if (channelLogin == state.streamId)
        {
            requestForChannelBadges(broadcasterId);
        }
    }
}

void Twitch::requestStreamInfo(const QString &login)
{
    QNetworkRequest request(QString("https://api.twitch.tv/helix/streams?user_login=%1").arg(login));
    request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, AxelChat::UserAgentNetworkHeaderName);
    request.setRawHeader("Accept-Language", AcceptLanguageNetworkHeaderName);
    request.setRawHeader("Client-ID", TWITCH_CLIENT_ID);
    request.setRawHeader("Authorization", QByteArray("Bearer ") + oauthToken.get().toUtf8());
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

    reply->deleteLater();

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

