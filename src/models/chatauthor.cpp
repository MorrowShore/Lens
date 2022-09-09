#include "chatauthor.h"

ChatAuthor::ChatAuthor(ChatService::ServiceType serviceType_,
                       const QString &name_,
                       const QString &authorId_,
                       const QUrl &avatarUrl_,
                       const QUrl &pageUrl_,
                       const QStringList &leftBadgesUrls_,
                       const QStringList &rightBadgesUrls_,
                       const std::set<Flag> &flags_,
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

const ChatAuthor &ChatAuthor::getSoftwareAuthor()
{
    static const QString authorId = "____SOFTWARE____";
    static const ChatAuthor author(ChatService::ServiceType::Software,
                            QCoreApplication::applicationName(),
                            authorId);

    return author;
}

QString ChatAuthor::flagToString(const Flag flag)
{
    QMetaEnum metaEnum = QMetaEnum::fromType<Flag>();
    return metaEnum.valueToKey((int)flag);
}
