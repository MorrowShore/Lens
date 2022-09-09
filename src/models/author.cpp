#include "author.h"
#include <QCoreApplication>
#include <QMetaEnum>
#include <QDebug>

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

bool Author::setValue(const Role role, const QVariant &value)
{
    if (UpdateableRoles.find(role) == UpdateableRoles.end())
    {
        qCritical() << Q_FUNC_INFO << ": role" << role << "not updatable, author name =" << name;
        return false;
    }

    const QVariant::Type type = value.type();

    bool validType = true;
    switch (role)
    {
    case Author::Role::PageUrl:
        if (type == QVariant::Type::Url)
        {
            const QUrl url = value.toUrl();
            if (!url.isEmpty())
            {
                pageUrl = url;
            }
            return true;
        }
        else
        {
            validType = false;
        }
        break;

    case Author::Role::Name:
        if (type == QVariant::Type::String)
        {
            name = value.toString();
            return true;
        }
        else
        {
            validType = false;
        }
        break;

    case Author::Role::NicknameColor:
        if (type == QVariant::Type::Color)
        {
            customNicknameColor = value.value<QColor>();
            return true;
        }
        else
        {
            validType = false;
        }
        break;

    case Role::AvatarUrl:
        if (type == QVariant::Type::Url)
        {
            const QUrl url = value.toUrl();
            if (!url.isEmpty())
            {
                avatarUrl = url;
            }
            return true;
        }
        else
        {
            validType = false;
        }
        break;

    case Author::Role::LeftBadgesUrls:
        if (type == QVariant::Type::StringList)
        {
            leftBadgesUrls = value.toStringList();
            return true;
        }
        else
        {
            validType = false;
        }
        break;

    case Author::Role::RightBadgesUrls:
        if (type == QVariant::Type::StringList)
        {
            rightBadgesUrls = value.toStringList();
            return true;
        }
        else
        {
            validType = false;
        }
        break;

    default:
        break;
    }

    if (validType)
    {
        qCritical() << Q_FUNC_INFO << ": role" << role << "not implemented yet" << ", author name = " << name;
    }
    else
    {
        qCritical() << Q_FUNC_INFO << ": invalid type" << type << "for role" << role << ", author name = " << name;
    }

    return false;
}

QVariant Author::getValue(const Role role) const
{
    switch (role)
    {
    case Author::Role::ServiceType: return (int)serviceType;
    case Author::Role::Id: return authorId;
    case Author::Role::PageUrl: return pageUrl;
    case Author::Role::Name: return name;
    case Author::Role::NicknameColor: return customNicknameColor;
    case Author::Role::AvatarUrl: return avatarUrl;
    case Author::Role::LeftBadgesUrls: return leftBadgesUrls;
    case Author::Role::RightBadgesUrls: return rightBadgesUrls;
    case Author::Role::IsVerified: return flags.find(Flag::Verified) != flags.end();
    case Author::Role::IsChatOwner: return flags.find(Flag::ChatOwner) != flags.end();
    case Author::Role::Sponsor: return flags.find(Flag::Sponsor) != flags.end();
    case Author::Role::Moderator: return flags.find(Flag::Moderator) != flags.end();
    }

    qCritical() << Q_FUNC_INFO << ": role" << role << "not implemented yet" << ", author name = " << name;
    return QVariant();
}