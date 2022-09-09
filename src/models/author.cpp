#include "author.h"
#include <QCoreApplication>
#include <QMetaEnum>

Author::Author(AxelChat::ServiceType serviceType_,
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

const Author &Author::getSoftwareAuthor()
{
    static const QString authorId = "____SOFTWARE____";
    static const Author author(AxelChat::ServiceType::Software,
                            QCoreApplication::applicationName(),
                            authorId);

    return author;
}

QString Author::flagToString(const Flag flag)
{
    return QMetaEnum::fromType<Flag>().valueToKey((int)flag);
}
