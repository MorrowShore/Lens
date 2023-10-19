#pragma once

#include "chat_services/ChatServiceType.h"
#include <QObject>
#include <QDateTime>
#include <QColor>
#include <QSet>
#include <QDateTime>
#include <QUrl>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <set>

class Author;

class Message{
    Q_GADGET
public:
    enum class Role {
        Id = Qt::UserRole + 1,
        Html,
        PublishedAt,
        ReceivedAt,
        IsBotCommand,
        MarkedAsDeleted,
        CustomAuthorAvatarUrl,
        CustomAuthorName,
        Destination,

        IsDonateSimple,
        IsDonateWithText,
        IsDonateWithImage,

        IsServiceMessage,

        BodyBackgroundForcedColor,

        IsYouTubeChatMembership,

        IsTwitchAction,
    };
    Q_ENUM(Role)

    enum class Flag {
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
    Q_ENUM(Flag)

    enum ColorRole {
        BodyBackgroundColorRole
    };

    static QString colorRoleToString(const ColorRole role)
    {
        switch(role)
        {
        case Message::ColorRole::BodyBackgroundColorRole: return "bodyBackground";
        }

        qWarning() << "unknown color role" << (int)role;

        return "<unknown>";
    }

    struct TextStyle
    {
        TextStyle()
            : bold(false)
            , italic(false)
        {}

        TextStyle(bool bold_, bool italic_)
            : bold(bold_)
            , italic(italic_)
        {}

        bool bold;
        bool italic;
    };

    class Content
    {
    public:
        enum class Type { Text, Image , Hyperlink };

        Type getType() const
        {
            return type;
        }

        static QString typeToStringId(const Type type)
        {
            switch (type)
            {
            case Message::Content::Type::Text: return "text";
            case Message::Content::Type::Image: return "image";
            case Message::Content::Type::Hyperlink: return "hyperlink";
            }

            return "unknown";
        }

        virtual ~Content(){}


        virtual QJsonObject toJson() const = 0;

    protected:
        Content(const Type type_)
            : type(type_)
        {

        }

        Type type;
    };

    class Text : public Content
    {
    public:
        Text(const QString& text_, const TextStyle& style_ = TextStyle())
            : Content(Type::Text)
            , text(text_)
            , style(style_)
        {
        }

        QJsonObject toJson() const override
        {
            QJsonObject root;
            root.insert("text", text);
            return root;
        }

        const QString& getText() const
        {
            return text;
        }

        const TextStyle& getStyle() const
        {
            return style;
        }

        TextStyle& getStyle()
        {
            return style;
        }

    private:
        const QString text;
        TextStyle style;
    };

    class Hyperlink : public Content
    {
    public:
        Hyperlink(const QString& text_, const QUrl& url_, const bool needSpaces_ = true, const TextStyle& style_ = TextStyle())
            : Content(Type::Hyperlink)
            , text(text_)
            , url(url_)
            , needSpaces(needSpaces_)
            , style(style_)
        {
        }

        QJsonObject toJson() const override
        {
            QJsonObject root;
            root.insert("url", url.toString());
            root.insert("text", text);
            return root;
        }

        const QString& getText() const
        {
            return text;
        }

        const QUrl& getUrl() const
        {
            return url;
        }

        bool isNeedSpaces() const
        {
            return needSpaces;
        }

        const TextStyle& getStyle() const
        {
            return style;
        }

        TextStyle& getStyle()
        {
            return style;
        }

    private:
        const QString text;
        const QUrl url;
        const bool needSpaces;
        TextStyle style;
    };

    class Image : public Content
    {
    public:
        Image(const QUrl& url_, const int height_ = 0, const bool needSpaces_ = true)
            : Content(Type::Image)
            , url(url_)
            , height(height_)
            , needSpaces(needSpaces_)
        {
        }

        QJsonObject toJson() const override
        {
            QJsonObject root;
            root.insert("url", url.toString());
            return root;
        }

        const QUrl& getUrl() const
        {
            return url;
        }

        int getHeight() const
        {
            return height;
        }

        bool isNeedSpaces() const
        {
            return needSpaces;
        }

    private:
        QUrl url;
        const int height;
        const bool needSpaces;
    };

    struct ReplyDestinationInfo
    {
        QString authorId;
        QString authorName;

        QString messageId;
        QString messageText;
    };

    class Builder
    {
    public:
        Builder(const std::weak_ptr<Author>& author, const QString& id = QString())
            : result(std::make_shared<Message>(
                QList<std::shared_ptr<Content>>(),
                author,
                QDateTime(),
                QDateTime(),
                id))
        {}

        Builder& setContents(const QList<std::shared_ptr<Content>>& contents)
        {
            result->contents = contents;
            return *this;
        }

        Builder& addText(const QString& text, const TextStyle& style = TextStyle())
        {
            result->contents.append(std::make_shared<Text>(text, style));
            return *this;
        }

        Builder& addHyperlink(const QString& text, const QUrl& url, const bool needSpaces = true, const TextStyle& style = TextStyle())
        {
            result->contents.append(std::make_shared<Hyperlink>(text, url, needSpaces, style));
            return *this;
        }

        Builder& addImage(const QUrl& url, const int height = 0, const bool needSpaces = true)
        {
            result->contents.append(std::make_shared<Image>(url, height, needSpaces));
            return *this;
        }

        Builder& setPublishedTime(const QDateTime& dateTime)
        {
            result->publishedAt = dateTime;
            return *this;
        }

        Builder& setReceivedTime(const QDateTime& dateTime)
        {
            result->receivedAt = dateTime;
            return *this;
        }

        Builder& setFlag(const Flag flag)
        {
            result->flags.insert(flag);
            return *this;
        }

        Builder& setForcedColor(const ColorRole& role, const QColor& color)
        {
            result->forcedColors.insert(role, color);
            return *this;
        }

        Builder& setDestination(const QStringList& destination)
        {
            result->destination = destination;
            return *this;
        }

        std::shared_ptr<Message> build()
        {
            if (result->publishedAt.isNull())
            {
                result->publishedAt = QDateTime::currentDateTime();
            }

            if (result->receivedAt.isNull())
            {
                result->receivedAt = QDateTime::currentDateTime();
            }

            result->updateHtml();

            return result;
        }

        static QPair<std::shared_ptr<Author>, std::shared_ptr<Message>> createDeleter(const ChatServiceType serviceType, const QString& id);

        static QPair<std::shared_ptr<Author>, std::shared_ptr<Message>> createSoftware(const QString& text);

    private:
        std::shared_ptr<Message> result;
    };

    Message(const QList<std::shared_ptr<Content>>& contents,
            const std::weak_ptr<Author>& author,
            const QDateTime& publishedAt = QDateTime::currentDateTime(),
            const QDateTime& receivedAt = QDateTime::currentDateTime(),
            const QString& messageId = QString(),
            const std::set<Flag>& flags = {},
            const QHash<ColorRole, QColor>& forcedColors = {},
            const QStringList& destination = QStringList(),
            const ReplyDestinationInfo& replyDestinationInfo = ReplyDestinationInfo());

    Message(const Message& other) = delete;
    Message(Message&& other) = delete;
    Message& operator=(const Message& other) = delete;
    Message& operator=(Message&& other) = delete;

    inline const QString& getId() const
    {
        return id;
    }
    inline const QString& toHtml() const
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
    inline std::weak_ptr<Author> getAuthor() const
    {
        return author;
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
    inline uint64_t getRow() const
    {
        return row;
    }
    inline void setRow(const uint64_t row_)
    {
        row = row_;
    }
    inline bool isHasFlag(const Flag flag) const
    {
        return flags.find(flag) != flags.end();
    }
    inline const QStringList& getDestination() const
    {
        return destination;
    }
    inline void setDestination(const QStringList& destination_)
    {
        destination = destination_;
    }

    void setPlainText(const QString& text);

    void setFlag(const Flag flag, bool enable);

    void printMessageInfo(const QString& prefix, const int& row = -1) const;

    QString getForcedColorRoleToQMLString(const ColorRole& role) const;

    const QList<std::shared_ptr<Content>>& getContents() const
    {
        return contents;
    }

    void setContents(const QList<std::shared_ptr<Content>>& contents_)
    {
        contents = contents_;
        updateHtml();
    }

    QJsonObject toJson(const Author& author) const;

    static QString flagToString(const Flag flag);

private:
    void updateHtml();
    static void trimText(QString& text);

    QList<std::shared_ptr<Content>> contents;
    QString html;
    QString id;
    QDateTime publishedAt;
    QDateTime receivedAt;
    std::weak_ptr<Author> author;
    const QString authorId;
    std::set<Flag> flags;
    QHash<ColorRole, QColor> forcedColors;
    QUrl customAuthorAvatarUrl;
    QString customAuthorName;
    QStringList destination;
    const ReplyDestinationInfo replyDestinationInfo;

    uint64_t row = 0;
};
