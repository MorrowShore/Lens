#include "vkvideo.h"
#include "secrets.h"
#include "utils.h"
#include "crypto/obfuscator.h"
#include "models/message.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>

namespace
{

static const int RequestChatInterval = 3000;
static const int RequestVideoInterval = 10000;

static const QString ApiVersion = "5.131";
static const int AvatarMinHeight = 128;
static const int StickerImageHeight = 128;

}

VkVideo::VkVideo(QSettings &settings, const QString &settingsGroupPathParent, QNetworkAccessManager &network_, cweqt::Manager&, QObject *parent)
    : ChatService(settings, settingsGroupPathParent, AxelChat::ServiceType::VkVideo, false, parent)
    , network(network_)
    , authStateInfo(ui.addLabel("Loading..."))
    , auth(settings, getSettingsGroupPath() + "/auth", network)
{
    ui.findBySetting(stream)->setItemProperty("placeholderText", tr("Broadcast link..."));
    
    OAuth2::Config config;
    config.flowType = OAuth2::FlowType::AuthorizationCode;
    config.clientId = OBFUSCATE(VK_APP_ID);
    config.clientSecret = OBFUSCATE(VK_SECURE_KEY);
    config.authorizationPageUrl = QString("https://oauth.vk.com/authorize?v=%1&display=page").arg(ApiVersion);
    config.redirectUrl = "http://localhost:" + QString("%1").arg(TcpServer::Port) + "/chat_service/" + getServiceTypeId(getServiceType()) + "/auth_code";
    config.scope = "video+offline";
    config.requestTokenUrl = "https://oauth.vk.com/access_token";
    auth.setConfig(config);
    QObject::connect(&auth, &OAuth2::stateChanged, this, &VkVideo::updateUI);
    
    loginButton = ui.addButton(tr("Login"), [this]()
    {
        if (auth.isLoggedIn())
        {
            auth.logout();
            reconnect();
        }
        else
        {
            auth.login();
        }
    });

    QObject::connect(&timerRequestChat, &QTimer::timeout, this, &VkVideo::requestChat);
    timerRequestChat.start(RequestChatInterval);

    QObject::connect(&timerRequestVideo, &QTimer::timeout, this, &VkVideo::requestVideo);
    timerRequestVideo.start(RequestVideoInterval);

    updateUI();
    reconnect();
}

ChatService::ConnectionState VkVideo::getConnectionState() const
{
    if (isConnected())
    {
        return ChatService::ConnectionState::Connected;
    }
    else if (isCanConnect() && enabled.get())
    {
        return ChatService::ConnectionState::Connecting;
    }
    
    return ChatService::ConnectionState::NotConnected;
}

QString VkVideo::getStateDescription() const
{
    if (!auth.isLoggedIn())
    {
        return tr("Not logged in");
    }

    switch (getConnectionState())
    {
    case ConnectionState::NotConnected:
        if (stream.get().isEmpty())
        {
            return tr("Broadcast not specified");
        }

        if (info.ownerId.isEmpty() || info.videoId.isEmpty())
        {
            return tr("The broadcast is not correct");
        }

        return tr("Not connected");
        
    case ConnectionState::Connecting:
        return tr("Connecting...");
        
    case ConnectionState::Connected:
        return tr("Successfully connected!");

    }

    return "<unknown_state>";
}

TcpReply VkVideo::processTcpRequest(const TcpRequest &request)
{
    const QString path = request.getUrl().path().toLower();

    if (path == "/auth_code")
    {
        return auth.processRedirect(request);
    }

    return TcpReply::createTextHtmlError("Unknown path");
}

void VkVideo::reconnectImpl()
{
    info = Info();

    if (!extractOwnerVideoId(stream.get().trimmed(), info.ownerId, info.videoId))
    {
        return;
    }

    state.streamUrl = QUrl(QString("https://vk.com/video/lives?z=video%1_%2").arg(info.ownerId, info.videoId));

    requestVideo();
    requestChat();
    updateUI();
}

void VkVideo::requestChat()
{
    if (!isCanConnect())
    {
        return;
    }

    QString rawUrl = QString("https://api.vk.com/method/video.getComments?extended=1&access_token=%1&v=%2&owner_id=%3&video_id=%4")
                                    .arg(auth.getAccessToken(), ApiVersion, info.ownerId, info.videoId);

    if (info.startOffset == -1)
    {
        rawUrl += "&count=1&sort=desc";
    }
    else
    {
        if (info.lastMessageId == -1)
        {
            rawUrl += QString("&count=30&offset=%1").arg(info.startOffset);
        }
        else
        {
            rawUrl += QString("&count=30&offset=1&start_comment_id=%1").arg(info.lastMessageId);
        }
    }

    const QUrl url(rawUrl);

    QNetworkRequest request(url);
    QNetworkReply* reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        if (!isCanConnect())
        {
            reply->deleteLater();
            return;
        }

        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(data).object();

        info.hasChat = root.contains("response");

        if (info.hasChat)
        {
            if (!isConnected())
            {
                requestVideo();
            }
        }

        const QJsonObject response = root.value("response").toObject();

        if (info.startOffset == -1)
        {
            info.startOffset = response.value("count").toVariant().toLongLong();

            info.startOffset -= 10;

            if (info.startOffset < 0)
            {
                info.startOffset = 0;
            }
            return;
        }

        QList<int64_t> idsNeedUpdate;

        const QJsonArray groups = response.value("groups").toArray();
        for (const QJsonValue& v : qAsConst(groups))
        {
            const QJsonObject group = v.toObject();
            int64_t id = group.value("id").toVariant().toLongLong();
            if (id >= 0)
            {
                id *= -1;
            }

            const QString name = group.value("name").toString();
            const QString avatar = group.value("photo_200").toString();

            User user;
            user.id = QString("%1").arg(id);
            user.name = name;
            user.avatar = avatar;
            user.isGroup = true;
            user.groupType = group.value("type").toString();
            user.verified = group.value("verified").toInt() == 1;

            users[id] = user;
        }

        const QJsonArray profiles = response.value("profiles").toArray();
        for (const QJsonValue& v : qAsConst(profiles))
        {
            const QJsonObject profile = v.toObject();
            const int64_t id = profile.value("id").toVariant().toLongLong();

            QString name = profile.value("first_name").toString().trimmed();

            const QString lastName = profile.value("last_name").toString().trimmed();
            if (!lastName.isEmpty())
            {
                if (!name.isEmpty())
                {
                    name += " ";
                }

                name += lastName;
            }

            if (auto it = users.find(id); it == users.end())
            {
                User user;
                user.id = QString("%1").arg(id);
                user.name = name;
                user.isGroup = false;

                users[id] = user;

                idsNeedUpdate.append(id);
            }
            else
            {
                it->second.name = name;
            }
        }

        const QJsonArray items = response.value("items").toArray();

        QList<std::shared_ptr<Message>> messages;
        QList<std::shared_ptr<Author>> authors;

        for (const QJsonValue& v : qAsConst(items))
        {
            const auto pair = parseMessage(v.toObject());

            if (pair.first && pair.second)
            {
                messages.append(pair.first);
                authors.append(pair.second);
            }
        }

        if (!messages.isEmpty())
        {
            emit readyRead(messages, authors);
        }

        if (!idsNeedUpdate.isEmpty())
        {
            requsetUsers(idsNeedUpdate);
        }
    });
}

void VkVideo::requestVideo()
{
    if (!isCanConnect())
    {
        return;
    }

    const QUrl url(QString("https://api.vk.com/method/video.get?count=1&access_token=%1&v=%2&videos=%3_%4")
                                .arg(auth.getAccessToken(), ApiVersion, info.ownerId, info.videoId));

    QNetworkRequest request(url);
    QNetworkReply* reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        bool connected = false;

        if (!isCanConnect())
        {
            reply->deleteLater();
            return;
        }

        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            if (isConnected())
            {
                setConnected(false);
            }
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(data).object();
        const QJsonArray items = root.value("response").toObject().value("items").toArray();

        if (items.count() == 1)
        {
            const QJsonObject video = items.first().toObject();
            if (video.value("live").toInt() == 1)
            {
                if (!info.hasChat)
                {
                    qWarning() << "no chat";
                }
            }
            else
            {
                qWarning() << "video is not live";
            }

            connected = true;
            state.viewers = video.value("spectators").toInt(-1);
            emit stateChanged();
        }
        else
        {
            qCritical() << "items not equal 1";
        }

        if (connected != isConnected())
        {
            setConnected(connected);
        }
    });
}

void VkVideo::requsetUsers(const QList<int64_t>& ids)
{
    if (ids.isEmpty())
    {
        qCritical() << "ids is empty";
        return;
    }

    QString idsString;

    for (const auto id : ids)
    {
        if (!idsString.isEmpty())
        {
            idsString += ",";
        }

        idsString += QString("%1").arg(id);
    }

    const QUrl url(QString("https://api.vk.com/method/users.get?fields=photo_max,verified&access_token=%1&v=%2&user_ids=%3")
                                .arg(auth.getAccessToken(), ApiVersion, idsString));

    QNetworkRequest request(url);
    QNetworkReply* reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        QByteArray data;
        if (!checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(data).object();

        const QJsonArray jsonUsers = root.value("response").toArray();

        for (const QJsonValue& v : qAsConst(jsonUsers))
        {
            const QJsonObject jsonUser = v.toObject();

            const int64_t id = jsonUser.value("id").toVariant().toLongLong();

            auto userIt = users.find(id);
            if (userIt == users.end())
            {
                qCritical() << "not found user id " << id;
                continue;
            }

            User& user = userIt->second;
            user.avatar = jsonUser.value("photo_max").toString();
            user.verified = jsonUser.value("verified").toInt() == 1;

            QStringList rightBadges;
            if (user.verified)
            {
                rightBadges.append("qrc:/resources/images/verified-icon.svg");
            }

            emit authorDataUpdated(
                generateAuthorId(user.id),
                    {
                        {Author::Role::AvatarUrl, QUrl(user.avatar)},
                        {Author::Role::RightBadgesUrls, rightBadges},
                    });
        }
    });
}

void VkVideo::updateUI()
{
    if (!authStateInfo)
    {
        qCritical() << "!authStateInfo";
    }

    switch (auth.getState())
    {
    case OAuth2::State::NotLoggedIn:
        authStateInfo->setItemProperty("text", "<img src=\"qrc:/resources/images/error-alt-svgrepo-com.svg\" width=\"20\" height=\"20\"> " + tr("Not logged in"));
        loginButton->setItemProperty("text", tr("Login"));
        break;

    case OAuth2::State::LoginInProgress:
        authStateInfo->setItemProperty("text", tr("Login in progress..."));
        loginButton->setItemProperty("text", tr("Login"));
        break;

    case OAuth2::State::LoggedIn:
        authStateInfo->setItemProperty("text", "<img src=\"qrc:/resources/images/tick.svg\" width=\"20\" height=\"20\"> " + tr("Logged in"));
        loginButton->setItemProperty("text", tr("Logout"));
        break;
    }

    emit stateChanged();
}

bool VkVideo::extractOwnerVideoId(const QString &videoiLink_, QString &ownerId, QString &videoId)
{
    ownerId.clear();
    videoId.clear();

    const QString videoiLink = videoiLink_.trimmed();
    const QString simplifyed = AxelChat::simplifyUrl(videoiLink);

    if (!simplifyed.startsWith("vk.com", Qt::CaseSensitivity::CaseInsensitive))
    {
        return false;
    }

    QString videoPart;

    if (videoiLink.contains("?"))
    {
        const QUrlQuery query(videoiLink.mid(videoiLink.indexOf("?") + 1));

        videoPart = query.queryItemValue("z", QUrl::ComponentFormattingOption::FullyDecoded);
        if (videoPart.isEmpty())
        {
            videoPart = query.queryItemValue("Z", QUrl::ComponentFormattingOption::FullyDecoded);
        }

        if (videoPart.contains("/"))
        {
            videoPart = videoPart.left(videoPart.indexOf("/"));
        }
    }
    else
    {
        if (!simplifyed.contains("/"))
        {
            return false;
        }

        videoPart = simplifyed.mid(simplifyed.indexOf("/") + 1);

        if (videoPart.contains("/"))
        {
            videoPart = videoPart.left(videoPart.indexOf("/"));
        }
    }

    if (videoPart.startsWith("video", Qt::CaseSensitivity::CaseInsensitive))
    {
        videoPart = videoPart.mid(5);
    }
    else
    {
        return false;
    }

    if (!videoPart.contains("_"))
    {
        return false;
    }

    ownerId = videoPart.left(videoPart.indexOf("_"));
    videoId = videoPart.mid(videoPart.indexOf("_") + 1);

    return true;
}

bool VkVideo::isCanConnect() const
{
    return enabled.get() && auth.isLoggedIn() && !info.ownerId.isEmpty() && !info.videoId.isEmpty();
}

QUrl VkVideo::parseSticker(const QJsonObject &jsonSticker)
{
    const QJsonArray images = jsonSticker.value("images").toArray();

    for (const QJsonValue& v : images)
    {
        const QJsonObject image = v.toObject();
        const int height = image.value("height").toInt();

        if (height >= AvatarMinHeight)
        {
            return QUrl(image.value("url").toString());
        }
    }

    return QUrl();
}

bool VkVideo::checkReply(QNetworkReply *reply, const char *tag, QByteArray &resultData)
{
    resultData.clear();

    if (!reply)
    {
        qCritical() << tag << ": !reply";
        return false;
    }

    resultData = reply->readAll();
    reply->deleteLater();
    if (resultData.isEmpty())
    {
        qCritical() << tag << ": data is empty";
        return false;
    }

    const QJsonObject root = QJsonDocument::fromJson(resultData).object();
    if (root.contains("error"))
    {
        qCritical() << tag << "Error:" << resultData << ", request =" << reply->request().url().toString();

        const int errorCode = root.value("error").toObject().value("error_code").toInt();
        if (errorCode == 2 || errorCode == 5 || errorCode == 27 || errorCode == 28 || errorCode == 101) // https://dev.vk.com/reference/errors
        {
            auth.logout();
            emit stateChanged();
        }

        return false;
    }

    return true;
}

QPair<std::shared_ptr<Message>, std::shared_ptr<Author>> VkVideo::parseMessage(const QJsonObject &json)
{
    const int64_t date = json.value("date").toVariant().toLongLong();
    const int64_t fromId = json.value("from_id").toVariant().toLongLong();
    const int64_t rawMessageId = json.value("id").toVariant().toLongLong();
    QString text = json.value("text").toString();
    const QJsonArray jsonAttachments = json.value("attachments").toArray();

    info.lastMessageId = rawMessageId;

    auto userIt = users.find(fromId);
    if (userIt == users.end())
    {
        qCritical() << "not found user id " << fromId;
        return { nullptr, nullptr };
    }

    const User& user = userIt->second;

    const QDateTime publishedAt =  QDateTime::fromSecsSinceEpoch(date);

    QList<std::shared_ptr<Message::Content>> contents;

    if (json.contains("reply_to_user"))
    {
        const int64_t replyToUserId = json.value("reply_to_user").toVariant().toLongLong();
        if (text.contains("[") && text.contains("|") && text.contains("]"))
        {
            const int pipePos = text.indexOf("|");
            const QString name = text.mid(pipePos + 1, text.indexOf("]") - pipePos - 1);

            text = text.mid(text.indexOf("]") + 1);

            Message::TextStyle style;
            style.bold = true;

            contents.append(std::make_shared<Message::Hyperlink>(name, QUrl(QString("https://vk.com/id%1").arg(replyToUserId)), false, style));
        }
        else
        {
            qCritical() << "not found [, | or ]";
        }
    }

    if (!text.isEmpty())
    {
        contents.append(std::make_shared<Message::Text>(text));
    }

    QString attachmentsString;
    for (const QJsonValue& v : qAsConst(jsonAttachments))
    {
        if (!attachmentsString.isEmpty())
        {
            attachmentsString += ", ";
        }

        const QJsonObject jsonAttachment = v.toObject();
        const QString type = jsonAttachment.value("type").toString();
        if (type == "photo")
        {
            attachmentsString += tr("image");
        }
        else if (type == "video")
        {
            attachmentsString += tr("video");
        }
        else if (type == "audio")
        {
            attachmentsString += tr("audio");
        }
        else if (type == "doc")
        {
            attachmentsString += tr("document");
        }
        else if (type == "sticker")
        {
            const QUrl url = parseSticker(jsonAttachment.value("sticker").toObject());
            if (url.isEmpty())
            {
                attachmentsString += tr("sticker");
            }
            else
            {
                contents.append(std::make_shared<Message::Image>(url, StickerImageHeight));
            }
        }
        else
        {
            qCritical() << "unknown attachment type" << type;
            attachmentsString += tr("unknown(%1)").arg(type);
        }
    }

    if (!attachmentsString.isEmpty())
    {
        if (!contents.isEmpty()) { contents.append(std::make_shared<Message::Text>("\n")); }

        Message::TextStyle style;
        style.italic = true;

        contents.append(std::make_shared<Message::Text>("[" + attachmentsString + "]", style));
    }

    QString pageUrl;

    if (user.isGroup)
    {
        QString id = user.id;
        if (id.startsWith('-'))
        {
            id = id.mid(1);
        }

        if (user.groupType == "page")
        {
            pageUrl = "https://vk.com/public";
        }
        else if (user.groupType == "event")
        {
            pageUrl = "https://vk.com/event";
        }
        else if (user.groupType == "group")
        {
            pageUrl = "https://vk.com/club";
        }
        else
        {
            pageUrl = "https://vk.com/public";
            qCritical() << "unknown group type" << user.groupType;
        }

        pageUrl += id;
    }
    else
    {
        pageUrl = QString("https://vk.com/id%1").arg(user.id);
    }

    Author::Builder authorBuilder(getServiceType(), generateAuthorId(user.id),user.name);
    authorBuilder.setAvatar(user.avatar);
    authorBuilder.setPage(pageUrl);

    if (user.verified)
    {
        authorBuilder.addRightBadge("qrc:/resources/images/verified-icon.svg");
    }

    const auto author = authorBuilder.build();

    const auto message = Message::Builder(
        author,
        generateMessageId(QString("%1_%2").arg(date).arg(rawMessageId)))
        .setPublishedTime(publishedAt)
        .setContents(contents)
        .build();

    return { message, author };
}
