#include "message.h"
#include "author.h"
#include "chat_services/chatservice.h"
#include <QUuid>
#include <QMetaEnum>
#include <QDebug>

Message::Message(const QList<std::shared_ptr<Message::Content>>& contents_,
                 const std::weak_ptr<Author>& author_,
                 const QDateTime &publishedAt_,
                 const QDateTime &receivedAt_,
                 const QString &messageId_,
                 const std::set<Flag> &flags_,
                 const QHash<ColorRole, QColor> &forcedColors_,
                 const QStringList& destination_,
                 const ReplyDestinationInfo& replyDestinationInfo_)
    : contents(contents_)
    , id(messageId_)
    , publishedAt(publishedAt_)
    , receivedAt(receivedAt_)
    , author(author_)
    , authorId(!author_.expired() ? author_.lock()->getId() : QString())
    , flags(flags_)
    , forcedColors(forcedColors_)
    , destination(destination_)
    , replyDestinationInfo(replyDestinationInfo_)
{
    if (id.isEmpty())
    {
        id = authorId + "/" + QUuid::createUuid().toString(QUuid::Id128);
    }

    if (!author.expired())
    {
        id = ChatService::getServiceTypeId(author.lock()->getServiceType()) + "/" + id;
    }

    updateHtml();
}

void Message::setPlainText(const QString &text)
{
    contents = { std::make_shared<Text>(text) };
    updateHtml();
}

void Message::setFlag(const Flag flag, bool enable)
{
    if (enable)
    {
        flags.insert(flag);
    }
    else
    {
        flags.erase(flag);
    }
}

QString boolToString(const bool& value)
{
    if (value)
    {
        return "true";
    }
    else
    {
        return "false";
    }
}

void Message::printMessageInfo(const QString &prefix, const int &row) const
{
    QString resultString = prefix;
    if (!resultString.isEmpty())
        resultString += "\n";

    resultString += "===========================";

    resultString += "\nAuthor Id: \"" + getAuthorId() + "\"";
    resultString += "\nMessage Text: \"" + toHtml() + "\"";
    resultString += "\nMessage Id: \"" + getId() + "\"";
    resultString += QString("\nMessage Position: %1").arg(getRow());

    if (row != -1)
    {
        resultString += QString("\nMessage Row: %1").arg(row);
    }
    else
    {
        resultString += "\nMessage Row: failed to retrieve";
    }

    resultString += "\nMessage Flags: ";

    for (const Flag& flag : flags)
    {
        resultString += flagToString(flag) + ", ";
    }

    resultString += "\n";

    resultString += "\n===========================";
    qDebug() << resultString.toUtf8();
}

QString Message::getForcedColorRoleToQMLString(const ColorRole &role) const
{
    if (forcedColors.contains(role) && forcedColors[role].isValid())
    {
        return forcedColors[role].name(QColor::HexArgb);
    }

    return QString();
}

QJsonObject Message::toJson(const Author& author) const
{
    QJsonObject root;

    root.insert("id", id);
    root.insert("author", author.toJson());

    root.insert("publishedAt", getPublishedAt().toString(Qt::DateFormat::ISODateWithMs));
    root.insert("receivedAt", getReceivedAt().toString(Qt::DateFormat::ISODateWithMs));

    root.insert("customAuthorAvatarUrl", getCustomAuthorAvatarUrl().toString());
    root.insert("customAuthorName", getCustomAuthorName());

    QJsonArray destinationJson;
    for (const QString& part : qAsConst(destination))
    {
        destinationJson.append(part);
    }
    root.insert("destination", destinationJson);

    QJsonObject forcedColorsJson;
    const QList<ColorRole> colorRoles = forcedColors.keys();
    for (const ColorRole role : qAsConst(colorRoles))
    {
        const QString roleStr = colorRoleToString(role);
        const QColor color = forcedColors.value(role);
        forcedColorsJson.insert(roleStr, color.name());
    }
    root.insert("forcedColors", forcedColorsJson);

    QJsonArray jsonContents;
    for (const std::shared_ptr<Content>& content : qAsConst(contents))
    {
        if (!content)
        {
            qWarning() << Q_FUNC_INFO << "content is null";
        }

        QJsonObject jsonContent;
        jsonContent.insert("type", Content::typeToStringId(content->getType()));
        jsonContent.insert("data", content->toJson());
        jsonContents.append(jsonContent);
    }

    root.insert("contents", jsonContents);
    return root;
}

void Message::trimText(QString &text)
{
    static const QSet<QChar> trimChars = {
        ' ',
        '\n',
        '\r',
    };

    int left = 0;
    int right = 0;

    for (int i = 0; i < text.length(); ++i)
    {
        const QChar& c = text[i];
        if (trimChars.contains(c))
        {
            left++;
        }
        else
        {
            break;
        }
    }

    for (int i = text.length() - 1; i >= 0; --i)
    {
        const QChar& c = text[i];
        if (trimChars.contains(c))
        {
            right++;
        }
        else
        {
            break;
        }
    }

    text = text.mid(left, text.length() - left - right);
}

QString Message::flagToString(const Flag flag)
{
    QMetaEnum metaEnum = QMetaEnum::fromType<Flag>();
    return metaEnum.valueToKey((int)flag);
}

void Message::updateHtml()
{
    html.clear();

    for (const std::shared_ptr<Content>& content : qAsConst(contents))
    {
        if (!content)
        {
            qWarning() << Q_FUNC_INFO << "content is null";
            continue;
        }

        switch (content->getType())
        {
        case Content::Type::Text:
        {
            const Text* textContent = static_cast<const Text*>(content.get());
            if (textContent)
            {
                const TextStyle& style = textContent->getStyle();

                QString text = textContent->getText().toHtmlEscaped();
                text = text.replace('\n', "<br>");

                if (style.bold)
                {
                    text = "<b>" + text + "</b>";
                }

                if (style.italic)
                {
                    text = "<i>" + text + "</i>";
                }

                html += text;
            }
        }
            break;

        case Message::Content::Type::Image:
        {
            const Image* image = static_cast<const Image*>(content.get());
            if (image)
            {
                if (image->isNeedSpaces())
                {
                    html += " ";
                }

                const QString url = image->getUrl().toString();
                if (image->getHeight() == 0)
                {
                    html += QString("<img align=\"top\" src=\"%1\">").arg(url);
                }
                else
                {
                    html += QString("<img align=\"top\" height=\"%1\" width=\"%1\" src=\"%2\">").arg(image->getHeight()).arg(url);
                }

                if (image->isNeedSpaces())
                {
                    html += " ";
                }
            }
        }
            break;

        case Message::Content::Type::Hyperlink:
        {
            const Hyperlink* hyperlink = static_cast<const Hyperlink*>(content.get());
            if (hyperlink)
            {
                const TextStyle& style = hyperlink->getStyle();

                QString chunk;

                if (hyperlink->isNeedSpaces())
                {
                    chunk += " ";
                }

                chunk += "<a href=\"" + hyperlink->getUrl().toString() + "\">" + hyperlink->getText() + "</a>";

                if (hyperlink->isNeedSpaces())
                {
                    chunk += " ";
                }

                if (style.bold)
                {
                    chunk = "<b>" + chunk + "</b>";
                }

                if (style.italic)
                {
                    chunk = "<i>" + chunk + "</i>";
                }

                html += chunk;
            }
        }
            break;
        }
    }

    trimText(html);
}

QPair<std::shared_ptr<Author>, std::shared_ptr<Message>> Message::Builder::createDeleter(const AxelChat::ServiceType serviceType, const QString &id)
{
    auto author = Author::Builder(serviceType, QString(), QString()).build();

    auto message = Builder(author, id).setFlag(Message::Flag::DeleterItem).build();

    return { author, message };
}

QPair<std::shared_ptr<Author>, std::shared_ptr<Message>> Message::Builder::createSoftware(const QString &text)
{
    static const auto author = Author::Builder(
                                   AxelChat::ServiceType::Software,
                                   "____SOFTWARE____",
                                   QCoreApplication::applicationName()).build();

    const auto message = Message::Builder(author)
        .addText(text)
        .build();

    return { author, message };
}
