#ifndef CHATAUTHOR_H
#define CHATAUTHOR_H

#include "chat_services/chatservice.hpp"
#include <QString>
#include <QUrl>
#include <QColor>
#include <set>

class ChatAuthor
{
    Q_GADGET
public:
    enum class Flag {
        ChatOwner,
        Moderator,
        Sponsor,
        Verified,
    };
    Q_ENUM(Flag)

    ChatAuthor() { };
    ChatAuthor(ChatService::ServiceType serviceType,
               const QString& name,
               const QString& authorId,
               const QUrl& avatarUrl = QUrl(),
               const QUrl& pageUrl = QUrl(),
               const QStringList& leftBadgesUrls = {},
               const QStringList& rightBadgesUrls = {},
               const std::set<Flag>& flags = {},
               const QColor& customNicknameColor = QColor());

    static const ChatAuthor& getSoftwareAuthor();

    inline const QString& getId() const
    {
        return authorId;
    }
    inline const QString& getName() const
    {
        return name;
    }
    inline const QUrl& getAvatarUrl() const
    {
        return avatarUrl;
    }
    inline void setAvatarUrl(const QUrl& url)
    {
        avatarUrl = url;
    }
    inline const QUrl& getPageUrl() const
    {
        return pageUrl;
    }
    inline const QColor& getCustomNicknameColor() const
    {
        return customNicknameColor;
    }
    inline ChatService::ServiceType getServiceType() const
    {
        return serviceType;
    }
    inline const QStringList& getLeftBadgesUrls() const
    {
        return leftBadgesUrls;
    }
    inline const QStringList& getRightBadgesUrls() const
    {
        return rightBadgesUrls;
    }
    inline bool isHasFlag(const Flag flag) const
    {
        return flags.find(flag) != flags.end();
    }

    static QString flagToString(const Flag flag);

    inline const std::set<uint64_t>& getMessagesIds() const
    {
        return messagesIds;
    }

    inline std::set<uint64_t>& getMessagesIds()
    {
        return messagesIds;
    }

private:
    ChatService::ServiceType serviceType = ChatService::ServiceType::Unknown;
    QString name;
    QString authorId;
    QUrl avatarUrl;
    QUrl pageUrl;
    QStringList leftBadgesUrls;
    QStringList rightBadgesUrls;
    std::set<Flag> flags;
    QColor customNicknameColor;
    std::set<uint64_t> messagesIds;
};

#endif // CHATAUTHOR_H
