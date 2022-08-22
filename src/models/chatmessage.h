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
        return messageId;
    }
    inline const QString& getText() const
    {
        return text;
    }
    inline const QString& getHtml() const
    {
        return html;
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
        return publishedAt;
    }
    inline const QDateTime& getReceivedAt() const
    {
        return receivedAt;
    }
    inline const QString& getAuthorId() const
    {
        return authorId;
    }
    inline uint64_t getIdNum() const
    {
        return idNum;
    }
    inline bool isMarkedAsDeleted() const
    {
        return _isMarkedAsDeleted;
    }

    inline void setIsBotCommand(bool isBotCommand_)
    {
        _isBotCommand = isBotCommand_;
    }
    inline void setMarkedAsDeleted(bool markedAsDeleted_)
    {
        _isMarkedAsDeleted = markedAsDeleted_;
    }
    inline void setIdNum(uint64_t idNum_)
    {
        idNum = idNum_;
    }
    inline void setText(const QString& text_)
    {
        text = text_;
    }

    inline const QSet<Flags>& getFlags() const
    {
        return flags;
    }

    void printMessageInfo(const QString& prefix, const int& row = -1) const;

    QString getForcedColorRoleToQMLString(const ForcedColorRoles& role) const;

    static void trimText(QString& text);

private:
    QString text;
    QString html;
    QString messageId;
    QDateTime publishedAt;
    QDateTime receivedAt;
    QString authorId;
    QSet<Flags> flags;
    QHash<ForcedColorRoles, QColor> forcedColors;
    QMap<QUrl, QList<int>> images; // <image url, [image poses]>

    uint64_t idNum = 0;

    bool _isMarkedAsDeleted = false;
    bool _isBotCommand = false;
    bool _isDeleterItem = false;
};

#endif // CHATMESSAGE_H
