#ifndef CHATMESSAGE_H
#define CHATMESSAGE_H

#include <QObject>
#include <QDateTime>
#include <QColor>
#include <QSet>
#include <QDateTime>
#include <QUrl>
#include <QMap>

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
                const QString& authorId,
                const QDateTime& publishedAt = QDateTime::currentDateTime(),
                const QDateTime& receivedAt = QDateTime::currentDateTime(),
                const QString& messageId = QString(),
                const QMap<QUrl, QList<int>>& images = {},
                const QSet<Flags>& flags = {},
                const QHash<ForcedColorRoles, QColor>& forcedColors = {});

    static ChatMessage createYouTubeDeleter(const QString& text,
                                            const QString& id);

    inline const QString& getMessageId() const
    {
        return _messageId;
    }
    inline const QString& getText() const
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
    inline const QDateTime& getPublishedAt() const
    {
        return _publishedAt;
    }
    inline const QDateTime& getReceivedAt() const
    {
        return _receivedAt;
    }
    inline const QString& getAuthorId() const
    {
        return _authorId;
    }
    inline void setIsBotCommand(bool isBotCommand) const
    {
        _isBotCommand = isBotCommand;
    }
    inline uint64_t getIdNum() const
    {
        return _idNum;
    }
    inline bool isMarkedAsDeleted() const
    {
        return _markedAsDeleted;
    }

    void printMessageInfo(const QString& prefix, const int& row = -1) const;

    QString getForcedColorRoleToQMLString(const ForcedColorRoles& role) const;

    static void trimText(QString& text);

private:
    QString _text;
    QString _messageId;
    QDateTime _publishedAt;
    QDateTime _receivedAt;
    QString _authorId;
    QSet<Flags> _flags;
    QHash<ForcedColorRoles, QColor> _forcedColors;
    QMap<QUrl, QList<int>> images; // <image url, [image poses]>

    bool _markedAsDeleted = false;

    uint64_t _idNum = 0;

    mutable bool _isBotCommand = false;
    bool _isDeleterItem = false;
};

#endif // CHATMESSAGE_H
