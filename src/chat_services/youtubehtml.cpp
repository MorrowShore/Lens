#include "youtubehtml.h"
#include "utils.h"
#include "models/messagesmodel.h"
#include "models/author.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QFile>
#include <QDir>
#include <QNetworkRequest>
#include <QNetworkReply>

namespace
{

static const int RequestChatInterval = 2000;
static const int RequestStreamInterval = 20000;

static const QString FolderLogs = "logs_youtube";

static const QByteArray AcceptLanguageNetworkHeaderName = "";

static const int MaxBadChatReplies = 10;
static const int MaxBadLivePageReplies = 3;

QByteArray extractDigitsOnly(const QByteArray& data)
{
    QByteArray result;
    for (const char& c : data)
    {
        if (c >= 48 && c <= 57)
        {
            result += c;
        }
    }

    return result;
}

}

YouTubeHtml::YouTubeHtml(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network_, cweqt::Manager& web_, QObject *parent)
    : ChatService(settings, settingsGroupPathParent, AxelChat::ServiceType::YouTube, true, parent)
    , network(network_)
    , browser(web_)
{
    getUIElementBridgeBySetting(stream)->setItemProperty("placeholderText", tr("Link or broadcast ID..."));

    /*addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createButton(tr("Open window"), [this]()
    {
        browser.openWindow();
    })));*/
    
    QObject::connect(&timerRequestChat, &QTimer::timeout, this, &YouTubeHtml::onTimeoutRequestChat);
    timerRequestChat.start(RequestChatInterval);
    
    QObject::connect(&timerRequestStreamPage,&QTimer::timeout, this, &YouTubeHtml::onTimeoutRequestStreamPage);
    timerRequestStreamPage.start(RequestStreamInterval);

    reconnect();
}

QString YouTubeHtml::extractBroadcastId(const QString &link) const
{
    const QString simpleUrl = AxelChat::simplifyUrl(link);

    const QUrlQuery& urlQuery = QUrlQuery(QUrl(link).query());

    const QString& vParameter = urlQuery.queryItemValue("v");

    QString broadcastId;
    QRegExp rx;

    //youtu.be/rSjMyeISW7w
    if (broadcastId.isEmpty())
    {
        rx = QRegExp("^youtu.be/([^/]*)$", Qt::CaseInsensitive);
        if (rx.indexIn(simpleUrl) != -1)
        {
            broadcastId = rx.cap(1);
        }
    }

    //studio.youtube.com/video/rSjMyeISW7w/livestreaming
    if (broadcastId.isEmpty())
    {
        rx = QRegExp("^(studio\\.)?youtube.com/video/([^/]*)/livestreaming$", Qt::CaseInsensitive);
        if (rx.indexIn(simpleUrl) != -1)
        {
            broadcastId = rx.cap(2);
        }
    }

    //youtube.com/video/rSjMyeISW7w
    //studio.youtube.com/video/rSjMyeISW7w
    if (broadcastId.isEmpty())
    {
        rx = QRegExp("^(studio\\.)?youtube.com/video/([^/]*)$", Qt::CaseInsensitive);
        if (rx.indexIn(simpleUrl) != -1)
        {
            broadcastId = rx.cap(2);
        }
    }

    //youtube.com/watch/rSjMyeISW7w
    //studio.youtube.com/watch/rSjMyeISW7w
    if (broadcastId.isEmpty())
    {
        rx = QRegExp("^(studio\\.)?youtube.com/watch/([^/]*)$", Qt::CaseInsensitive);
        if (rx.indexIn(simpleUrl) != -1)
        {
            broadcastId = rx.cap(2);
        }
    }

    //youtube.com/live_chat?is_popout=1&v=rSjMyeISW7w
    //studio.youtube.com/live_chat?v=rSjMyeISW7w&is_popout=1
    if (broadcastId.isEmpty())
    {
        rx = QRegExp("^(studio\\.)?youtube.com/live_chat$", Qt::CaseInsensitive);
        if (rx.indexIn(simpleUrl) != -1 && !vParameter.isEmpty())
        {
            broadcastId = vParameter;
        }
    }

    //https://www.youtube.com/watch?v=rSjMyeISW7w&feature=youtu.be
    //https://www.youtube.com/watch?v=rSjMyeISW7w
    if (broadcastId.isEmpty())
    {
        rx = QRegExp("^(studio\\.)?youtube.com/watch$", Qt::CaseInsensitive);
        if (rx.indexIn(simpleUrl) != -1 && !vParameter.isEmpty())
        {
            broadcastId = vParameter;
        }
    }

    //Broadcast id only
    //rSjMyeISW7w
    if (broadcastId.isEmpty())
    {
        rx = QRegExp("^[a-zA-Z0-9_\\-]+$", Qt::CaseInsensitive);
        if (rx.indexIn(link) != -1)
        {
            broadcastId = link;
        }
    }

    return broadcastId;
}

void YouTubeHtml::printData(const QString &tag, const QByteArray& data)
{
    qDebug() << "==============================================================================================================================";
    qDebug() << tag.toUtf8();
    qDebug() << "==================================DATA========================================================================================";
    qDebug() << data;
    qDebug() << "==============================================================================================================================";
}

QUrl YouTubeHtml::createResizedAvatarUrl(const QUrl &sourceAvatarUrl, int imageHeight)
{
    if (sourceAvatarUrl.isEmpty())
    {
        return sourceAvatarUrl;
    }

    //qDebug("Source URL: " + sourceAvatarUrl.toString().toUtf8());

    //example: https://yt3.ggpht.com/ytc/AAUvwngFVeI2l6JADC9wxZbdGK1fu382MwOtp6bYWA=s3200-c-k-c0x00ffffff-no-rj

    QString source = sourceAvatarUrl.toString().trimmed();
    source.replace('\\', '/');
    if (source.back() == '/')
    {
        source = source.left(source.length() - 1);
    }

    if (source.startsWith("//"))
    {
        source = QString("https:") + source;
    }

    const QVector<QStringRef>& parts = source.splitRef('/', Qt::KeepEmptyParts);
    if (parts.count() < 2)
    {
        qDebug() << Q_FUNC_INFO << ": Failed to convert: parts.count() < 2, url:" << sourceAvatarUrl;
        return sourceAvatarUrl;
    }

    //qDebug("Before URL: " + sourceAvatarUrl.toString().toUtf8());

    QString targetPart = parts[parts.count() - 1].toString();
    QRegExp rx(".*s(\\d+).*", Qt::CaseInsensitive);
    rx.setMinimal(false);
    if (rx.lastIndexIn(targetPart) != -1)
    {
        //qDebug("Regexp matched: '" + rx.cap(1).toUtf8() + "'");
        //qDebug(QString("Regexp matched position: %1").arg(rx.pos()).toUtf8());

        targetPart.remove(rx.pos() + 1, rx.cap(1).length());
        targetPart.insert(rx.pos() + 1, QString("%1").arg(imageHeight));

        QString newUrlStr;
        for (int i = 0; i < parts.count(); ++i)
        {
            if (i != parts.count() - 1)
            {
                newUrlStr += parts[i].toString();
            }
            else
            {
                newUrlStr += targetPart;
            }

            if (i != parts.count() - 1)
            {
                newUrlStr += "/";
            }
        }

        //qDebug("Result URL: " + newUrlStr.toUtf8());
        return newUrlStr;
    }

    qDebug() << Q_FUNC_INFO << ": Failed to convert";
    return sourceAvatarUrl;
}

void YouTubeHtml::reconnectImpl()
{
    const bool preConnected = state.connected;
    const QString preBroadcastId = state.streamId;

    state = State();

    badChatReplies = 0;
    badLivePageReplies = 0;

    if (preConnected && !preBroadcastId.isEmpty())
    {
        emit connectedChanged(false);
    }

    state.streamId = extractBroadcastId(stream.get().trimmed());

    if (!state.streamId.isEmpty())
    {
        state.chatUrl = QUrl(QString("https://www.youtube.com/live_chat?v=%1").arg(state.streamId));

        state.streamUrl = QUrl(QString("https://www.youtube.com/watch?v=%1").arg(state.streamId));

        state.controlPanelUrl = QUrl(QString("https://studio.youtube.com/video/%1/livestreaming").arg(state.streamId));
    }

    if (!enabled.get())
    {
        return;
    }

    onTimeoutRequestChat();
    onTimeoutRequestStreamPage();
}

ChatService::ConnectionStateType YouTubeHtml::getConnectionStateType() const
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

QString YouTubeHtml::getStateDescription() const
{
    switch (getConnectionStateType())
    {
    case ConnectionStateType::NotConnected:
        if (stream.get().isEmpty())
        {
            return tr("Broadcast not specified");
        }

        if (state.streamId.isEmpty())
        {
            return tr("The broadcast is not correct");
        }

        return tr("Not connected");

    case ConnectionStateType::Connecting:
        return tr("Connecting...");

    case ConnectionStateType::Connected:
        return tr("Successfully connected!");

    }

    return "<unknown_state>";
}

void YouTubeHtml::onTimeoutRequestChat()
{
    if (!enabled.get() || state.chatUrl.isEmpty())
    {
        return;
    }

    QNetworkRequest request(state.chatUrl);
    request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, AxelChat::UserAgentNetworkHeaderName);
    request.setRawHeader("Accept-Language", AcceptLanguageNetworkHeaderName);
    QObject::connect(network.get(request), &QNetworkReply::finished, this, &YouTubeHtml::onReplyChatPage);
}

void YouTubeHtml::onReplyChatPage()
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply*>(sender());
    if (!reply)
    {
        qDebug() << Q_FUNC_INFO << "!reply";
        return;
    }

    const QByteArray rawData = reply->readAll();
    reply->deleteLater();

    if (rawData.isEmpty())
    {
        processBadChatReply();
        qDebug() << Q_FUNC_INFO << ":rawData is empty";
        return;
    }

    //AxelChat::saveDebugDataToFile(FolderLogs, "raw_last_youtube.html", rawData);

    const QString startData = QString::fromUtf8(rawData.left(100));

    if (startData.contains("<title>Oops</title>", Qt::CaseSensitivity::CaseInsensitive))
    {
        //ToDo: show message "bad url"
        processBadChatReply();
        return;
    }

    const int start = rawData.indexOf("\"actions\":[");
    if (start == -1)
    {
        qDebug() << Q_FUNC_INFO << ": not found actions";
        AxelChat::saveDebugDataToFile(FolderLogs, "not_found_actions_from_html_youtube.html", rawData);
        processBadChatReply();
        return;
    }

    QByteArray data = rawData.mid(start + 10);
    const int pos = data.lastIndexOf(",\"actionPanel\"");
    if (pos != -1)
    {
        data = data.remove(pos, data.length());
    }

    //AxelChat::saveDebugDataToFile(FolderLogs, "last_youtube.json", data);

    const QJsonDocument jsonDocument = QJsonDocument::fromJson(data);
    if (jsonDocument.isArray())
    {
        const QJsonArray actionsArray = jsonDocument.array();
        //qDebug() << "array size = " << actionsArray.size();
        parseActionsArray(actionsArray, data);
        badChatReplies = 0;
    }
    else
    {
        printData(Q_FUNC_INFO + QString(": document is not array"), data);

        AxelChat::saveDebugDataToFile(FolderLogs, "failed_to_parse_from_html_youtube.html", rawData);
        AxelChat::saveDebugDataToFile(FolderLogs, "failed_to_parse_from_html_youtube.json", data);
        processBadChatReply();
    }
}

void YouTubeHtml::onTimeoutRequestStreamPage()
{
    if (!enabled.get() || state.streamUrl.isEmpty())
    {
        return;
    }

    QNetworkRequest request(state.streamUrl);
    request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, AxelChat::UserAgentNetworkHeaderName);
    request.setRawHeader("Accept-Language", AcceptLanguageNetworkHeaderName);

    QNetworkReply* reply = network.get(request);
    if (!reply)
    {
        qDebug() << Q_FUNC_INFO << ": !reply";
        return;
    }
    
    QObject::connect(reply, &QNetworkReply::finished, this, &YouTubeHtml::onReplyStreamPage);
}

void YouTubeHtml::onReplyStreamPage()
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply*>(sender());
    if (!reply)
    {
        qDebug() << "!reply";
        return;
    }

    const QByteArray rawData = reply->readAll();
    reply->deleteLater();

    if (rawData.isEmpty())
    {
        qDebug() << Q_FUNC_INFO << ":rawData is empty";
        processBadLivePageReply();
        return;
    }

    if (parseViews(rawData))
    {
        badLivePageReplies = 0;
    }
    else
    {
        processBadLivePageReply();
    }
}

void YouTubeHtml::parseActionsArray(const QJsonArray& array, const QByteArray& data)
{
    //AxelChat::saveDebugDataToFile(FolderLogs, "debug.json", data);

    if (!state.connected && !state.streamId.isEmpty() && enabled.get())
    {
        state.connected = true;

        emit connectedChanged(true);
        emit stateChanged();
    }

    QList<std::shared_ptr<Message>> messages;
    QList<std::shared_ptr<Author>> authors;

    for (const QJsonValue& actionJson : array)
    {
        //const QByteArray rawData = QJsonDocument(actionJson.toObject()).toJson(QJsonDocument::JsonFormat::Compact);
        //if (rawData.contains("")) { qDebug(rawData + "\n"); }

        bool valid = false;
        bool isDeleter = false;

        QList<std::shared_ptr<Message::Content>> contents;

        QString messageId;
        const QDateTime& receivedAt = QDateTime::currentDateTime();
        QDateTime publishedAt;

        QString authorName;
        QString authorChannelId;
        QStringList rightBadges;
        QColor authorNicknameColor;
        QColor authorNicknameBackgroundColor;
        QUrl authorAvatarUrl;
        std::set<Message::Flag> messageFlags;
        std::set<Author::Flag> authorFlags;

        QHash<Message::ColorRole, QColor> forcedColors;

        const QJsonObject actionObject = actionJson.toObject();

        if (actionObject.contains("addChatItemAction"))
        {
            //Message

            const QJsonObject item = actionObject.value("addChatItemAction").toObject().value("item").toObject();

            QJsonObject itemRenderer;

            if (item.contains("liveChatTextMessageRenderer"))
            {
                itemRenderer = item.value("liveChatTextMessageRenderer").toObject();

                valid = true;
            }
            else if (item.contains("liveChatPaidMessageRenderer"))
            {
                itemRenderer = item.value("liveChatPaidMessageRenderer").toObject();

                messageFlags.insert(Message::Flag::DonateWithText);

                valid = true;
            }
            else if (item.contains("liveChatPaidStickerRenderer"))
            {
                //ToDo:
                itemRenderer = item.value("liveChatPaidStickerRenderer").toObject();

                messageFlags.insert(Message::Flag::DonateWithImage);

                valid = true;
            }
            else if (item.contains("liveChatMembershipItemRenderer"))
            {
                itemRenderer = item.value("liveChatMembershipItemRenderer").toObject();

                messageFlags.insert(Message::Flag::YouTubeChatMembership);

                valid = true;
            }
            else if (item.contains("liveChatSponsorshipsGiftPurchaseAnnouncementRenderer"))
            {
                itemRenderer = item.value("liveChatSponsorshipsGiftPurchaseAnnouncementRenderer").toObject();
                if (itemRenderer.contains("id"))
                {
                    messageId = itemRenderer.value("id").toString(messageId);
                }

                if (itemRenderer.contains("timestampUsec"))
                {
                    publishedAt = QDateTime::fromMSecsSinceEpoch(
                                itemRenderer.value("timestampUsec").toString().toLongLong() / 1000,
                                Qt::TimeSpec::UTC).toLocalTime();
                }

                itemRenderer = itemRenderer.value("header").toObject().value("liveChatSponsorshipsHeaderRenderer").toObject();

                messageFlags.insert(Message::Flag::YouTubeChatMembership);

                valid = true;
            }
            else if (item.contains("liveChatSponsorshipsGiftRedemptionAnnouncementRenderer"))
            {
                itemRenderer = item.value("liveChatSponsorshipsGiftRedemptionAnnouncementRenderer").toObject();

                messageFlags.insert(Message::Flag::YouTubeChatMembership);

                valid = true;
            }
            else if (item.contains("liveChatTickerSponsorItemRenderer"))
            {
                itemRenderer = item.value("liveChatTickerSponsorItemRenderer").toObject();

                messageFlags.insert(Message::Flag::YouTubeChatMembership);

                valid = true;
            }
            else if (item.contains("liveChatViewerEngagementMessageRenderer"))
            {
                QJsonObject itemRenderer_ = item.value("liveChatViewerEngagementMessageRenderer").toObject();
                const QString iconType = itemRenderer_.value("icon").toObject().value("iconType").toString().trimmed().toUpper();
                if (iconType == "POLL")
                {
                    itemRenderer = std::move(itemRenderer_);
                    messageFlags.insert(Message::Flag::ServiceMessage);
                    valid = true;
                }
                else if (iconType == "YOUTUBE_ROUND")
                {
                    // ignore
                    valid = false;
                }
                else if (iconType.isEmpty())
                {
                    printData(Q_FUNC_INFO + QString(": iconType is empty of object \"%1\"").arg(iconType, "liveChatViewerEngagementMessageRenderer"), data);
                    valid = false;
                }
                else
                {
                    printData(Q_FUNC_INFO + QString(": Unknown iconType \"%1\" of object \"%2\"").arg(iconType, "liveChatViewerEngagementMessageRenderer"), data);
                    valid = false;
                }
            }
            else if (item.contains("liveChatPlaceholderItemRenderer") ||
                     item.contains("liveChatModeChangeMessageRenderer"))
            {
                // ignore
                valid = false;
            }
            else
            {
                AxelChat::saveDebugDataToFile(FolderLogs, "unknown_item_structure.json", QJsonDocument(item).toJson());
                valid = false;
            }

            if (itemRenderer.contains("id"))
            {
                messageId = itemRenderer.value("id").toString(messageId);
            }

            if (itemRenderer.contains("timestampUsec"))
            {
                publishedAt = QDateTime::fromMSecsSinceEpoch(
                            itemRenderer.value("timestampUsec").toString().toLongLong() / 1000,
                            Qt::TimeSpec::UTC).toLocalTime();
            }

            QList<std::shared_ptr<Message::Content>> authorNameContent;
            tryAppedToText(authorNameContent, itemRenderer, "authorName", false);
            if (!authorNameContent.isEmpty())
            {
                if (const std::shared_ptr<Message::Content> content = authorNameContent.first(); content->getType() == Message::Content::Type::Text)
                {
                    authorName = static_cast<const Message::Text*>(content.get())->getText();
                }
            }

            tryAppedToText(contents, itemRenderer, "purchaseAmountText", true);

            tryAppedToText(contents, itemRenderer, "headerPrimaryText",  true);
            tryAppedToText(contents, itemRenderer, "header",             true);
            tryAppedToText(contents, itemRenderer, "headerSubtext",      false);

            tryAppedToText(contents, itemRenderer, "primaryText",        true);
            tryAppedToText(contents, itemRenderer, "text",               true);
            tryAppedToText(contents, itemRenderer, "subtext",            false);

            tryAppedToText(contents, itemRenderer, "message",            false);

            authorChannelId = itemRenderer
                    .value("authorExternalChannelId").toString();

            const QJsonArray authorPhotoThumbnails = itemRenderer
                    .value("authorPhoto").toObject()
                    .value("thumbnails").toArray();

            if (!authorPhotoThumbnails.isEmpty())
            {
                authorAvatarUrl = createResizedAvatarUrl(authorPhotoThumbnails.first().toObject().value("url").toString(), 300);
            }

            if (itemRenderer.contains("authorBadges"))
            {
                const QJsonArray authorBadges = itemRenderer.value("authorBadges").toArray();

                for (const QJsonValue& badge : authorBadges)
                {
                    const QJsonObject liveChatAuthorBadgeRenderer = badge.toObject().value("liveChatAuthorBadgeRenderer").toObject();

                    if (liveChatAuthorBadgeRenderer.contains("icon"))
                    {
                        const QString& iconType = liveChatAuthorBadgeRenderer.value("icon").toObject()
                                .value("iconType").toString();

                        bool foundIconType = false;

                        if (iconType.toLower() == "owner")
                        {
                            authorFlags.insert(Author::Flag::ChatOwner);
                            authorNicknameColor = QColor(255, 217, 15);
                            authorNicknameBackgroundColor = QColor(255, 214, 0);
                            foundIconType = true;
                        }

                        if (iconType.toLower() == "moderator")
                        {
                            authorFlags.insert(Author::Flag::Moderator);
                            authorNicknameColor = QColor(95, 132, 241);
                            rightBadges.append("qrc:/resources/images/youtube-moderator-icon.svg");
                            foundIconType = true;
                        }

                        if (iconType.toLower() == "verified")
                        {
                            authorFlags.insert(Author::Flag::Verified);
                            authorNicknameColor = QColor(244, 143, 177);
                            rightBadges.append("qrc:/resources/images/verified-icon.svg");
                            foundIconType = true;
                        }

                        if (!foundIconType && !iconType.isEmpty())
                        {
                            qDebug() << "Unknown iconType" << iconType;
                        }
                    }
                    else if (liveChatAuthorBadgeRenderer.contains("customThumbnail"))
                    {
                        authorFlags.insert(Author::Flag::Sponsor);
                        authorNicknameColor = QColor(16, 117, 22);

                        const QJsonArray thumbnails = liveChatAuthorBadgeRenderer.value("customThumbnail").toObject()
                                .value("thumbnails").toArray();

                        if (!thumbnails.isEmpty())
                        {
                            rightBadges.append(createResizedAvatarUrl(QUrl(thumbnails.first().toObject().value("url").toString()), badgePixelSize).toString());
                        }
                    }
                    else
                    {
                        printData(Q_FUNC_INFO + QString(": Unknown json structure of object \"%1\"").arg("liveChatAuthorBadgeRenderer"), data);
                    }
                }
            }

            if (const QJsonValue v = itemRenderer.value("authorUsernameColorDark"); v.isDouble() && !authorNicknameColor.isValid())
            {
                authorNicknameColor = v.toInt();
            }

            const QJsonArray stickerThumbnails = itemRenderer
                    .value("sticker").toObject()
                    .value("thumbnails").toArray();

            if (!stickerThumbnails.isEmpty())
            {
                const QString stickerUrl = createResizedAvatarUrl(stickerThumbnails.first().toObject().value("url").toString(), stickerSize).toString();

                if (!contents.isEmpty() && contents.last()->getType() == Message::Content::Type::Text)
                {
                    contents.append(std::make_shared<Message::Text>("\n"));
                }

                contents.append(std::make_shared<Message::Image>(stickerUrl, stickerSize));
            }

            //Other colors: headerBackgroundColor headerTextColor bodyBackgroundColor bodyTextColor authorNameTextColor timestampColor backgroundColor moneyChipBackgroundColor moneyChipTextColor

            if (itemRenderer.contains("bodyBackgroundColor"))
            {
                // usually for liveChatPaidMessageRenderer
                forcedColors.insert(Message::ColorRole::BodyBackgroundColorRole, itemRenderer.value("bodyBackgroundColor").toVariant().toLongLong());
            }

            if (itemRenderer.contains("backgroundColor"))
            {
                // usually for liveChatPaidStickerRenderer
                forcedColors.insert(Message::ColorRole::BodyBackgroundColorRole, itemRenderer.value("backgroundColor").toVariant().toLongLong());
            }

            if (itemRenderer.contains("headerBackgroundColor"))
            {
                // usually for liveChatPaidStickerRenderer
                forcedColors.insert(Message::ColorRole::BodyBackgroundColorRole, itemRenderer.value("headerBackgroundColor").toVariant().toLongLong());
            }
        }
        else if (actionObject.contains("removeChatItemAction"))
        {
            //Message deleted

            const QJsonObject removeChatItemAction = actionObject.value("removeChatItemAction").toObject();

            messageId = removeChatItemAction.value("targetItemId").toString();

            isDeleter = true;
            valid = true;
        }
        else if (actionObject.contains("markChatItemsByAuthorAsDeletedAction"))
        {
            //ToDo: Это событие удаляет все сообщения этого пользователя
            //Deleted message by author

            const QJsonObject markChatItemsByAuthorAsDeletedAction = actionObject.value("markChatItemsByAuthorAsDeletedAction").toObject();

            tryAppedToText(contents, markChatItemsByAuthorAsDeletedAction, "deletedStateMessage", false);

            messageId = markChatItemsByAuthorAsDeletedAction.value("targetItemId").toString();

            isDeleter = true;
            valid = true;
        }
        else if (actionObject.contains("replaceChatItemAction"))
        {
            //ToDo
            //qDebug() << Q_FUNC_INFO << QString(": object \"replaceChatItemAction\" not supported yet");
        }
        else if (actionObject.contains("addLiveChatTickerItemAction"))
        {
            //ToDo
            //qDebug() << Q_FUNC_INFO << QString(": object \"addLiveChatTickerItemAction\" not supported yet");
        }
        else if (actionObject.contains("addToPlaylistCommand") ||
                 actionObject.contains("clickTrackingParams") ||
                 actionObject.contains("addBannerToLiveChatCommand"))
        {
            // ignore
        }
        else
        {
            QJsonDocument doc(actionObject);
            //printData(Q_FUNC_INFO + QString(": unknown json structure of array \"%1\"").arg("actions"), doc.toJson());
            AxelChat::saveDebugDataToFile(FolderLogs, "unknown_action_structure.json", doc.toJson());
        }

        if (valid)
        {
            if (isDeleter)
            {
                static const std::shared_ptr<Author> author = std::make_shared<Author>(getServiceType(), "", "");

                messages.append(std::make_shared<Message>(
                    contents,
                    author,
                    QDateTime::currentDateTime(),
                    QDateTime::currentDateTime(),
                    messageId,
                    std::set<Message::Flag>{Message::Flag::DeleterItem}));

                authors.append(author);
            }
            else
            {
                std::shared_ptr<Author> author = std::make_shared<Author>(
                    getServiceType(),
                    authorName,
                    authorChannelId,
                    authorAvatarUrl,
                    QUrl(QString("https://www.youtube.com/channel/%1").arg(authorChannelId)),
                    QStringList(),
                    rightBadges,
                    authorFlags,
                    authorNicknameColor,
                    authorNicknameBackgroundColor);

                std::shared_ptr<Message> message = std::make_shared<Message>(
                            contents,
                            author,
                            publishedAt,
                            receivedAt,
                            messageId,
                            messageFlags,
                            forcedColors);

                messages.append(message);
                authors.append(author);
            }
        }
    }

    emit readyRead(messages, authors);
    emit stateChanged();
}

bool YouTubeHtml::parseViews(const QByteArray &rawData)
{
    static const QByteArray Prefix = "{\"videoViewCountRenderer\":{\"viewCount\":{\"runs\":[";

    const int start = rawData.indexOf(Prefix);
    if (start == -1)
    {
        if (rawData.contains("videoViewCountRenderer\":{\"viewCount\":{}"))
        {
            state.viewersCount = 0;
            emit stateChanged();
            return false;
        }

        qDebug() << Q_FUNC_INFO << ": failed to parse videoViewCountRenderer";
        AxelChat::saveDebugDataToFile(FolderLogs, "failed_to_parse_videoViewCountRenderer_from_html_youtube.html", rawData);
        return false;
    }

    int lastPos = -1;
    for (int i = start + Prefix.length(); i < rawData.length(); ++i)
    {
        const QChar& c = rawData[i];
        if (c == ']')
        {
            lastPos = i;
            break;
        }
    }

    if (lastPos == -1)
    {
        qDebug() << Q_FUNC_INFO << ": not found ']'";
        return false;
    }

    const QByteArray data = rawData.mid(start + Prefix.length(), lastPos - (start + Prefix.length()));
    const QByteArray digits = extractDigitsOnly(data);
    if (digits.isEmpty())
    {
        printData(Q_FUNC_INFO + QString(": failed to find digits"), data);

        AxelChat::saveDebugDataToFile(FolderLogs, "failed_to_parse_from_html_no_digits_youtube.html", rawData);
        AxelChat::saveDebugDataToFile(FolderLogs, "failed_to_parse_from_html_no_digits_youtube.json", data);
        return false;
    }

    bool ok = false;
    const int viewers = digits.toInt(&ok);
    if (!ok)
    {
        printData(Q_FUNC_INFO + QString(": failed to convert to number"), data);

        AxelChat::saveDebugDataToFile(FolderLogs, "failed_to_parse_from_html_fail_to_convert_to_number_youtube.html", rawData);
        AxelChat::saveDebugDataToFile(FolderLogs, "failed_to_parse_from_html_fail_to_convert_to_number_youtube.json", data);
        return false;
    }

    state.viewersCount = viewers;

    emit stateChanged();

    return true;
}

void YouTubeHtml::processBadChatReply()
{
    badChatReplies++;

    if (badChatReplies >= MaxBadChatReplies)
    {
        if (state.connected && !state.streamId.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "too many bad chat replies! Disonnecting...";

            state = State();

            state.connected = false;

            emit connectedChanged(false);

            reconnect();
        }
    }
}

void YouTubeHtml::processBadLivePageReply()
{
    badLivePageReplies++;

    if (badLivePageReplies >= MaxBadLivePageReplies)
    {
        qWarning() << Q_FUNC_INFO << "too many bad live page replies!";
        state.viewersCount = -1;

        emit stateChanged();
    }
}

void YouTubeHtml::tryAppedToText(QList<std::shared_ptr<Message::Content>>& contents, const QJsonObject& jsonObject, const QString& varName, bool bold) const
{
    if (!jsonObject.contains(varName))
    {
        return;
    }

    QList<std::shared_ptr<Message::Content>> newContents;
    parseText(jsonObject.value(varName).toObject(), newContents);

    if (!contents.isEmpty() && !newContents.isEmpty())
    {
        if (contents.last()->getType() == Message::Content::Type::Text && contents.last()->getType() == newContents.first()->getType())
        {
            contents.append(std::make_shared<Message::Text>("\n"));
        }
    }

    for (std::shared_ptr<Message::Content> content : qAsConst(newContents))
    {
        if (content->getType() == Message::Content::Type::Text)
        {
            static_cast<Message::Text*>(content.get())->getStyle().bold = bold;
        }
    }

    contents.append(newContents);
}

void YouTubeHtml::parseText(const QJsonObject &message, QList<std::shared_ptr<Message::Content>>& contents) const
{
    if (message.contains("simpleText"))
    {
        const QString text = message.value("simpleText").toString().trimmed();
        if (!text.isEmpty())
        {
            contents.append(std::make_shared<Message::Text>(text));
        }
    }

    if (message.contains("runs"))
    {
        const QJsonArray runs = message.value("runs").toArray();

        for (const QJsonValue& run : runs)
        {
            const QJsonObject& runObject = run.toObject();
            if (runObject.contains("navigationEndpoint"))
            {
                QString url = runObject.value("navigationEndpoint").toObject()
                        .value("commandMetadata").toObject()
                        .value("webCommandMetadata").toObject()
                        .value("url").toString();
                const QString& text = runObject.value("text").toString();

                if (!url.isEmpty() && !text.isEmpty())
                {
                    if (url.startsWith("/"))
                    {
                        url = "https://www.youtube.com" + url;
                    }

                    contents.append(std::make_shared<Message::Hyperlink>(text, url));
                    continue;
                }
                else
                {
                    AxelChat::saveDebugDataToFile(FolderLogs, "unknown_message_runs_unknown_navigationEndpoint_structure.json", QJsonDocument(message).toJson());
                }
            }

            if (runObject.contains("text"))
            {
                const QString text = runObject.value("text").toString();
                if (!text.isEmpty())
                {
                    contents.append(std::make_shared<Message::Text>(text));
                }
            }
            else if (runObject.contains("emoji"))
            {
                const QJsonArray thumbnails = runObject.value("emoji").toObject()
                        .value("image").toObject()
                        .value("thumbnails").toArray();

                if (!thumbnails.isEmpty())
                {
                    const QString empjiUrl = thumbnails.first().toObject().value("url").toString();
                    if (!empjiUrl.isEmpty())
                    {
                        contents.append(std::make_shared<Message::Image>(empjiUrl, emojiPixelSize));
                    }
                    else
                    {
                        AxelChat::saveDebugDataToFile(FolderLogs, "unknown_message_runs_empty_emoji_structure.json", QJsonDocument(message).toJson());
                    }
                }
                else
                {
                    AxelChat::saveDebugDataToFile(FolderLogs, "unknown_message_runs_empty_thumbnails_structure.json", QJsonDocument(message).toJson());
                }
            }
            else
            {
                AxelChat::saveDebugDataToFile(FolderLogs, "unknown_message_runs_empty_thumbnails_structure.json", QJsonDocument(message).toJson());
            }
        }
    }
}

QColor YouTubeHtml::intToColor(quint64 rawColor) const
{
    qDebug() << "rawColor" << rawColor;
    return QColor::fromRgba64(QRgba64::fromRgba64(rawColor));
}