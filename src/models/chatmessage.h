#ifndef CHATMESSAGE_H
#define CHATMESSAGE_H

#include <QObject>
#include <QDateTime>
#include <QColor>
#include <QSet>
#include <QDateTime>
#include <QUrl>

class ChatMessage{
    Q_GADGET
public:
    enum Flags {
        DonateSimple,
        DonateWithText,
        DonateWithImage,

        PlatformGeneric,

        YouTubeChatMembership,
        TwitchAction,
    };

    enum ForcedColorRoles {
        BodyBackgroundForcedColorRole
    };

    friend class ChatMessagesModel;
    ChatMessage() { }
    ChatMessage(const QString& text,
                const QString& id,
                const QDateTime& publishedAt,
                const QDateTime& receivedAt,
                const QString& authorId,
                const QSet<Flags>& flags,
                const QHash<ForcedColorRoles, QColor>& forcedColors);

    static ChatMessage createFromYouTube(const QString& text,
                              const QString& id,
                              const QDateTime& publishedAt,
                              const QDateTime& receivedAt,
                              const QString& authorId,
                              const QSet<Flags>& flags,
                              const QHash<ForcedColorRoles, QColor>& forcedColors);

    static ChatMessage createFromTwitch(const QString& text,
                              const QDateTime& receivedAt,
                              const QString& authorId,
                              const QSet<Flags>& flags);

    static ChatMessage createFromGoodGame(const QString& text,
                              const QDateTime& timestamp,
                              const QString& authorId);

    static ChatMessage createDeleterFromYouTube(const QString& text,
                                                const QString& id);

    inline const QString& id() const
    {
        return _id;
    }
    inline const QString& text() const
    {
        return _text;
    }
    inline bool isBotCommand() const
    {
        return _isBotCommand;
    }
    inline bool isDeleterItem() const
    {
        return _isDeleterItem;
    }
    inline const QDateTime& publishedAt() const
    {
        return _publishedAt;
    }
    inline const QDateTime& receivedAt() const
    {
        return _receivedAt;
    }
    inline const QString& authorId() const
    {
        return _authorId;
    }
    inline void setIsBotCommand(bool isBotCommand) const
    {
        _isBotCommand = isBotCommand;
    }
    inline uint64_t idNum() const
    {
        return _idNum;
    }
    inline bool markedAsDeleted() const
    {
        return _markedAsDeleted;
    }

    void printMessageInfo(const QString& prefix, const int& row = -1) const;

    QString forcedColorRoleToQMLString(const ForcedColorRoles& role) const;

    static void trimCustom(QString& text);

private:
    QString _text;
    QString _id;
    QDateTime _publishedAt;
    QDateTime _receivedAt;
    QString _authorId;
    QSet<Flags> _flags;
    QHash<ForcedColorRoles, QColor> _forcedColors;

    bool _markedAsDeleted = false;

    uint64_t _idNum = 0;

    mutable bool _isBotCommand = false;
    bool _isDeleterItem = false;
};

#endif // CHATMESSAGE_H
