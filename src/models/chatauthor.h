#ifndef CHATAUTHOR_H
#define CHATAUTHOR_H

#include "abstractchatservice.hpp"
#include <QString>
#include <QUrl>
#include <QColor>
#include <set>

class ChatAuthor
{
    Q_GADGET
public:
    enum class Flags {
        ChatOwner,
        Moderator,
        Sponsor,
        Verified,
    };

    ChatAuthor() { };
    ChatAuthor(AbstractChatService::ServiceType serviceType,
               const QString& name,
               const QString& authorId,
               const QUrl& avatarUrl = QUrl(),
               const QUrl& pageUrl = QUrl(),
               const QStringList& leftBadgesUrls = {},
               const QStringList& rightBadgesUrls = {},
               const std::set<Flags>& flags = {},
               const QColor& customNicknameColor = QColor());

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
    inline int getMessagesCount() const
    {
        return messagesCount;
    }
    inline void setMessagesCount(int count)
    {
        messagesCount = count;
    }
    inline const QColor& getCustomNicknameColor() const
    {
        return customNicknameColor;
    }
    inline AbstractChatService::ServiceType getServiceType() const
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
    inline bool isHasFlag(const Flags flag) const
    {
        return flags.find(flag) != flags.end();
    }

private:
    AbstractChatService::ServiceType serviceType = AbstractChatService::ServiceType::Unknown;
    QString name;
    QString authorId;
    QUrl avatarUrl;
    QUrl pageUrl;
    QStringList leftBadgesUrls;
    QStringList rightBadgesUrls;
    std::set<Flags> flags;
    QColor customNicknameColor;
    int messagesCount = 0;
};

#endif // CHATAUTHOR_H
