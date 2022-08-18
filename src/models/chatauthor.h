#ifndef CHATAUTHOR_H
#define CHATAUTHOR_H

#include "abstractchatservice.hpp"
#include <QString>
#include <QUrl>
#include <QColor>

class ChatAuthor
{
    Q_GADGET
public:
    friend class ChatHandler;
    friend class ChatMessagesModel;

    ChatAuthor() { };

    inline const QString& authorId() const
    {
        return _authorId;
    }
    inline const QString& name() const
    {
        return _name;
    }
    inline const QUrl& avatarUrl() const
    {
        return _avatarUrl;
    }
    inline bool isVerified() const
    {
        return _isVerified;
    }
    inline bool isChatOwner() const
    {
        return _isChatOwner;
    }
    inline bool isChatSponsor() const
    {
        return _isChatSponsor;
    }
    inline bool isChatModerator() const
    {
        return _isChatModerator;
    }
    inline const QUrl& customBadgeUrl() const
    {
        return _customBadgeUrl;
    }
    inline const QStringList& twitchBadgesUrls() const
    {
        return _twitchBadgesUrls;
    }
    inline const QUrl& pageUrl() const
    {
        return _pageUrl;
    }

    inline int messagesCount() const
    {
        return _messagesSent;
    }

    inline const QColor& nicknameColor() const
    {
        return _nicknameColor;
    }

    inline AbstractChatService::ServiceType getServiceType() const
    {
        return _serviceType;
    }

    static ChatAuthor createFromYouTube(const QString& name,
                                           const QString& channelId,
                                           const QUrl& avatarUrl,
                                           const QUrl badgeUrl,
                                           const bool isVerified,
                                           const bool isChatOwner,
                                           const bool isChatSponsor,
                                           const bool isChatModerator);

    static ChatAuthor createFromTwitch(const QString& name,
                                          const QString& channelId,
                                          const QUrl& avatarUrl,
                                          const QColor& nicknameColor,
                                          const QMap<QString, QString>& badges);

    static ChatAuthor createFromGoodGame(const QString& name,
                                          const QString& userId,
                                          const int userGroup);

private:
    friend class ChatMessage;

    QString _authorId;
    QString _name;
    QColor _nicknameColor;
    QStringList _twitchBadgesUrls;
    QUrl _pageUrl;
    QUrl _avatarUrl;
    QUrl _customBadgeUrl;
    bool _isVerified      = false;
    bool _isChatOwner     = false;
    bool _isChatSponsor   = false;
    bool _isChatModerator = false;
    bool _isDonation      = false;
    int _messagesSent = 0;
    AbstractChatService::ServiceType _serviceType = AbstractChatService::ServiceType::Unknown;
};

#endif // CHATAUTHOR_H
