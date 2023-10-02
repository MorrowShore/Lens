#pragma once

#include "chat_services/chatservicestypes.h"
#include <QString>
#include <QUrl>
#include <QColor>
#include <QJsonObject>
#include <QJsonDocument>
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

        LeftTags,
        RightTags,

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

    struct Tag
    {
        QString text;
        QColor color = QColor(255, 0, 0);
        QColor textColor = QColor(255, 255, 255);

        QJsonObject toJson() const
        {
            return QJsonObject(
            {
                { "text", text },
                { "color", color.name() },
                { "textColor", textColor.name() },
            });
        }
    };

    friend class Builder;

    class Builder
    {
    public:
        Builder(const AxelChat::ServiceType serviceType, const QString& id, const QString& name)
            : result(std::make_shared<Author>(serviceType, name, id))
        {}

        Builder& setAvatar(const QString& url) { result->avatarUrl = url; return *this; }
        Builder& setPage(const QString& url) { result->pageUrl = url; return *this; }
        Builder& addLeftBadge(const QString& url) { result->leftBadgesUrls.append(url); return *this; }
        Builder& addRightBadge(const QString& url) { result->rightBadgesUrls.append(url); return *this; }
        Builder& setFlag(const Flag flag) { result->flags.insert(flag); return *this; }
        Builder& setCustomNicknameColor(const QColor& color) { result->customNicknameColor = color; return *this; }
        Builder& setCustomNicknameBackgroundColor(const QColor& color) { result->customNicknameBackgroundColor = color; return *this; }
        Builder& addLeftTag(const Tag& tag) { result->leftTags.append(tag); return *this; }
        Builder& addRightTag(const Tag& tag) { result->rightTags.append(tag); return *this; }

        std::shared_ptr<Author> build() const { return result; }

    private:
        std::shared_ptr<Author> result;
    };

    Author(const AxelChat::ServiceType serviceType,
           const QString& name,
           const QString& authorId,
           const QUrl& avatarUrl = QUrl(),
           const QUrl& pageUrl = QUrl(),
           const QStringList& leftBadgesUrls = {},
           const QStringList& rightBadgesUrls = {},
           const std::set<Flag>& flags = {},
           const QColor& customNicknameColor = QColor(),
           const QColor& customNicknameBackgroundColor = QColor());

    Author(const Author& other) = delete;
    Author(Author&& other) = delete;
    Author& operator=(const Author& other) = delete;
    Author& operator=(Author&& other) = delete;

    static const std::shared_ptr<Author> getSoftwareAuthor();

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
    const AxelChat::ServiceType serviceType = AxelChat::ServiceType::Unknown;
    QString name;
    QString authorId;
    QUrl avatarUrl;
    QUrl pageUrl;
    QStringList leftBadgesUrls;
    QStringList rightBadgesUrls;
    QList<Tag> leftTags;
    QList<Tag> rightTags;
    std::set<Flag> flags;
    QColor customNicknameColor;
    QColor customNicknameBackgroundColor;
    std::set<QString> messagesIds;
};

Q_DECLARE_METATYPE(Author::Role)
