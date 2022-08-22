#include "chatauthor.h"

ChatAuthor::ChatAuthor(ChatService::ServiceType serviceType_,
                       const QString &name_,
                       const QString &authorId_,
                       const QUrl &avatarUrl_,
                       const QUrl &pageUrl_,
                       const QStringList &leftBadgesUrls_,
                       const QStringList &rightBadgesUrls_,
                       const std::set<Flags> &flags_,
                       const QColor &customNicknameColor_)
    : serviceType(serviceType_)
    , name(name_)
    , authorId(authorId_)
    , avatarUrl(avatarUrl_)
    , pageUrl(pageUrl_)
    , leftBadgesUrls(leftBadgesUrls_)
    , rightBadgesUrls(rightBadgesUrls_)
    , flags(flags_)
    , customNicknameColor(customNicknameColor_)
{

}

QString ChatAuthor::flagToString(const Flags flag)
{
    QMetaEnum metaEnum = QMetaEnum::fromType<Flags>();
    return metaEnum.valueToKey((int)flag);
}

/*ChatAuthor ChatAuthor::createFromYouTube(
        const QString &name,
        const QString &channelId,
        const QUrl &avatarUrl,
        const QUrl badgeUrl,
        const bool isVerified,
        const bool isChatOwner,
        const bool isChatSponsor,
        const bool isChatModerator)
{
    ChatAuthor author;

    author._name = name;
    author._authorId = channelId;
    author._pageUrl = QUrl(QString("https://www.youtube.com/channel/%1").arg(channelId));
    author._avatarUrl = avatarUrl;
    author._customBadgeUrl = badgeUrl;
    author._isVerified = isVerified;
    author._isChatOwner = isChatOwner;
    author._isChatSponsor = isChatSponsor;
    author._isChatModerator = isChatModerator;
    author._serviceType = AbstractChatService::ServiceType::YouTube;

    return author;
}

ChatAuthor ChatAuthor::createFromTwitch(const QString &name, const QString &channelId, const QUrl& avatarUrl, const QColor& nicknameColor, const QMap<QString, QString>& badges)
{
    ChatAuthor author;

    author._name = name;
    author._nicknameColor = nicknameColor;
    author._twitchBadgesUrls = badges.values();
    author._authorId = channelId;
    author._pageUrl = QUrl(QString("https://www.twitch.tv/%1").arg(channelId));
    if (avatarUrl.isValid())
    {
        author._avatarUrl = avatarUrl;
    }

    author._isVerified = badges.contains("partner/1");
    author._isChatOwner = badges.contains("broadcaster/1");
    author._isChatModerator = badges.contains("moderator/1");

    author._serviceType = AbstractChatService::ServiceType::Twitch;

    return author;
}

ChatAuthor ChatAuthor::createFromGoodGame(const QString &name, const QString &userId, const int userGroup)
{
    ChatAuthor author;

    author._name = name;
    author._authorId = userId;
    author._pageUrl = QUrl(QString("https://goodgame.ru/user/%1").arg(userId));

    author._serviceType = AbstractChatService::ServiceType::GoodGame;

    return author;
}*/
