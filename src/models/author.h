#pragma once

#include "chat_services/chatservicestypes.h"
#include <QString>
#include <QUrl>
#include <QColor>
#include <QJsonObject>
#include <set>

class Author
{
    Q_GADGET
public:
    enum class Role {
        ServiceType = Qt::UserRole + 1 + 1024,
        Id,
        PageUrl,
        Name,
        HasCustomNicknameColor,
        CustomNicknameColor,
        HasCustomNicknameBackgroundColor,
        CustomNicknameBackgroundColor,
        AvatarUrl,
        LeftBadgesUrls,
        RightBadgesUrls,

        IsVerified,
        IsChatOwner,
        Sponsor,
        Moderator
    };
    Q_ENUM(Role)

    enum class Flag {
        ChatOwner,
        Moderator,
        Sponsor,
        Verified,
    };
    Q_ENUM(Flag)

    Author() { };
    Author(AxelChat::ServiceType serviceType,
           const QString& name,
           const QString& authorId,
           const QUrl& avatarUrl = QUrl(),
           const QUrl& pageUrl = QUrl(),
           const QStringList& leftBadgesUrls = {},
           const QStringList& rightBadgesUrls = {},
           const std::set<Flag>& flags = {},
           const QColor& customNicknameColor = QColor(),
           const QColor& customNicknameBackgroundColor = QColor());

    static const Author& getSoftwareAuthor();

    inline const QString& getId() const
    {
        return authorId;
    }
    inline AxelChat::ServiceType getServiceType() const
    {
        return serviceType;
    }
    inline const std::set<QString>& getMessagesIds() const
    {
        return messagesIds;
    }
    inline std::set<QString>& getMessagesIds()
    {
        return messagesIds;
    }

    bool setValue(const Role role, const QVariant& value);

    QVariant getValue(const Role role) const;

    static QString getJsonRoleName(const Role role);
    QJsonObject toJson() const;

    inline static const std::set<Role> UpdateableRoles =
    {
        Role::PageUrl,
        Role::Name,
        Role::CustomNicknameColor,
        Role::CustomNicknameBackgroundColor,
        Role::AvatarUrl,
        Role::LeftBadgesUrls,
        Role::RightBadgesUrls,
    };

private:
    AxelChat::ServiceType serviceType = AxelChat::ServiceType::Unknown;
    QString name;
    QString authorId;
    QUrl avatarUrl;
    QUrl pageUrl;
    QStringList leftBadgesUrls;
    QStringList rightBadgesUrls;
    std::set<Flag> flags;
    QColor customNicknameColor;
    QColor customNicknameBackgroundColor;
    std::set<QString> messagesIds;
};

Q_DECLARE_METATYPE(Author::Role)
