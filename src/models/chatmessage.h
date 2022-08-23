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

        ServiceMessage,

        YouTubeChatMembership,
        TwitchAction,
    };
    Q_ENUM(Flags)

    enum ForcedColorRoles {
        BodyBackgroundForcedColorRole
    };

    class Content
    {
    public:
        enum class Type { Unknown, Text, Image , Hyperlink };

        Type getContentType() const
        {
            return type;
        }

    protected:
        Content(const Type type_)
            : type(type_)
        {

        }

        Type type = Type::Unknown;
    };

    class Text : public Content
    {
    public:
        struct Style
        {
            Style()
                : bold(false)
            {}

            bool bold;
        };

        Text(const QString& text_, const Style& style_ = Style())
            : Content(Type::Text)
            , text(text_)
            , style(style_)
        {
        }

        const QString& getText() const
        {
            return text;
        }

        const Style& getStyle() const
        {
            return style;
        }

        Style& getStyle()
        {
            return style;
        }

    private:
        const QString text;
        Style style;
    };

    class Hyperlink : public Content
    {
    public:
        Hyperlink(const QString& text_, const QUrl& url_)
            : Content(Type::Hyperlink)
            , text(text_)
            , url(url_)
        {
        }

        const QString& getText() const
        {
            return text;
        }

        const QUrl& getUrl() const
        {
            return url;
        }

    private:
        const QString text;
        const QUrl url;
    };

    class Image : public Content
    {
    public:
        Image(const QUrl& url_, const int height_ = 0)
            : Content(Type::Image)
            , url(url_)
            , height(height_)
        {
        }

        const QUrl& getUrl() const
        {
            return url;
        }

        int getHeight() const
        {
            return height;
        }

    private:
        QUrl url;
        const int height;
    };

    ChatMessage() { }
    ChatMessage(const QList<Content*>& contents,
                const QString& authorId,
                const QDateTime& publishedAt = QDateTime::currentDateTime(),
                const QDateTime& receivedAt = QDateTime::currentDateTime(),
                const QString& messageId = QString(),
                const QMap<QUrl, QList<int>>& images = {},
                const std::set<Flags>& flags = {},
                const QHash<ForcedColorRoles, QColor>& forcedColors = {});

    inline const QString& getId() const
    {
        return messageId;
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
    inline bool isHasFlag(const Flags flag) const
    {
        return flags.find(flag) != flags.end();
    }
    void setPlainText(const QString& text);

    void setFlag(const Flags flag, bool enable);

    void printMessageInfo(const QString& prefix, const int& row = -1) const;

    QString getForcedColorRoleToQMLString(const ForcedColorRoles& role) const;

    const QList<Content*>& getContents() const
    {
        return contents;
    }

    static QString flagToString(const Flags flag);

private:
    void updateHtml();
    static void trimText(QString& text);

    QList<Content*> contents;
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
