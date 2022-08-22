#ifndef CHATMESSAGE_H
#define CHATMESSAGE_H

#include <QObject>
#include <QDateTime>
#include <QColor>
#include <QSet>
#include <QDateTime>
#include <QUrl>
#include <QMap>
#include <set>

class ChatMessage{
    Q_GADGET
public:
    enum class Flags {
        MarkedAsDeleted,
        DeleterItem,
        BotCommand,
        IgnoreBotCommand,

        DonateSimple,
        DonateWithText,
        DonateWithImage,

        PlatformGeneric,

        YouTubeChatMembership,
        TwitchAction,
    };
    Q_ENUM(Flags)

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
                const std::set<Flags>& flags = {},
                const QHash<ForcedColorRoles, QColor>& forcedColors = {});

    static ChatMessage createYouTubeDeleter(const QString& text,
                                            const QString& id);

    inline const QString& getId() const
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
    inline const QUrl& getCustomAuthorAvatarUrl() const
    {
        return customAuthorAvatarUrl;
    }
    inline void setCustomAuthorAvatarUrl(const QUrl& customAuthorAvatarUrl_)
    {
        customAuthorAvatarUrl = customAuthorAvatarUrl_;
    }
    inline const QString& getCustomAuthorName() const
    {
        return customAuthorName;
    }
    inline void setCustomAuthorName(const QString& name)
    {
        customAuthorName = name;
    }
    inline uint64_t getIdNum() const
    {
        return idNum;
    }
    inline void setIdNum(uint64_t idNum_)
    {
        idNum = idNum_;
    }
    inline void setText(const QString& text_)
    {
        text = text_;
        updateHtml();
    }
    inline bool isHasFlag(const Flags flag) const
    {
        return flags.find(flag) != flags.end();
    }

    void setFlag(const Flags flag, bool enable);

    void printMessageInfo(const QString& prefix, const int& row = -1) const;

    QString getForcedColorRoleToQMLString(const ForcedColorRoles& role) const;

    static void trimText(QString& text);
    static QString flagToString(const Flags flag);

private:
    void updateHtml();

    QString text;
    QString html;
    QString messageId;
    QDateTime publishedAt;
    QDateTime receivedAt;
    QString authorId;
    std::set<Flags> flags;
    QHash<ForcedColorRoles, QColor> forcedColors;
    QMap<QUrl, QList<int>> images; // <image url, [image poses]>
    QUrl customAuthorAvatarUrl;
    QString customAuthorName;

    uint64_t idNum = 0;
};

#endif // CHATMESSAGE_H
