#include "youtubeutils.h"
#include "utils.h"
#include <QUrlQuery>

namespace
{

static const AxelChat::ServiceType ServiceType = AxelChat::ServiceType::YouTube;
static const int EmojiPixelSize = 24;
static const int StickerSize = 80;
static const int BadgePixelSize = 64;

}

const QString YouTubeUtils::FolderLogs = "logs_youtube";

QString YouTubeUtils::extractBroadcastId(const QString &link)
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

void YouTubeUtils::parseActionsArray(const QJsonArray &array, const QByteArray &data, QList<std::shared_ptr<Message>> &messages, QList<std::shared_ptr<Author>> &authors)
{
    //AxelChat::saveDebugDataToFile(FolderLogs, "debug.json", data);

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
                            rightBadges.append(createResizedAvatarUrl(QUrl(thumbnails.first().toObject().value("url").toString()), BadgePixelSize).toString());
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
                const QString stickerUrl = createResizedAvatarUrl(stickerThumbnails.first().toObject().value("url").toString(), StickerSize).toString();

                if (!contents.isEmpty() && contents.last()->getType() == Message::Content::Type::Text)
                {
                    contents.append(std::make_shared<Message::Text>("\n"));
                }

                contents.append(std::make_shared<Message::Image>(stickerUrl, StickerSize));
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
                static const std::shared_ptr<Author> author = std::make_shared<Author>(ServiceType, "", "");

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
                    ServiceType,
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
}

void YouTubeUtils::printData(const QString &tag, const QByteArray& data)
{
    qDebug() << "==============================================================================================================================";
    qDebug() << tag.toUtf8();
    qDebug() << "==================================DATA========================================================================================";
    qDebug() << data;
    qDebug() << "==============================================================================================================================";
}

QUrl YouTubeUtils::createResizedAvatarUrl(const QUrl &sourceAvatarUrl, int imageHeight)
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

void YouTubeUtils::tryAppedToText(QList<std::shared_ptr<Message::Content>>& contents, const QJsonObject& jsonObject, const QString& varName, bool bold)
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

void YouTubeUtils::parseText(const QJsonObject &message, QList<std::shared_ptr<Message::Content>>& contents)
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
                        contents.append(std::make_shared<Message::Image>(empjiUrl, EmojiPixelSize));
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
