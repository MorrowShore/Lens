#include "message.h"
#include "author.h"
#include "chat_services/chatservice.h"
#include <QUuid>
#include <QMetaEnum>
#include <QDebug>

Message::Message(const QList<Message::Content*>& contents_,
                         const Author& author,
                         const QDateTime &publishedAt_,
                         const QDateTime &receivedAt_,
                         const QString &messageId_,
                         const std::set<Flag> &flags_,
                         const QHash<ColorRole, QColor> &forcedColors_)
    : contents(contents_)
    , messageId(messageId_)
    , publishedAt(publishedAt_)
    , receivedAt(receivedAt_)
    , authorId(author.getId())
    , flags(flags_)
    , forcedColors(forcedColors_)
{
    if (messageId.isEmpty())
    {
        messageId = authorId + "/" + QUuid::createUuid().toString(QUuid::Id128);
    }

    messageId = ChatService::getServiceTypeId(author.getServiceType()) + "/" + messageId;

    updateHtml();
}

void Message::setPlainText(const QString &text)
{
    contents = QList<Content*>({new Text(text)});
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
    resultString += "\nMessage Text: \"" + getHtml() + "\"";
    resultString += "\nMessage Id: \"" + getId() + "\"";
    resultString += QString("\nMessage Id Num: %1").arg(getIdNum());

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
    qDebug(resultString.toUtf8());
}

QString Message::getForcedColorRoleToQMLString(const ColorRole &role) const
{
    if (forcedColors.contains(role) && forcedColors[role].isValid())
    {
        return forcedColors[role].name(QColor::HexArgb);
    }

    return QString();
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

    for (const Content* content : qAsConst(contents))
    {
        switch (content->getContentType())
        {
        case Content::Type::Unknown:
            qWarning() << "Unknown content type";
            break;

        case Content::Type::Text:
        {
            const Text* textContent = static_cast<const Text*>(content);
            if (textContent)
            {
                const Text::Style& style = textContent->getStyle();

                QString text = textContent->getText();

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
            const Image* image = static_cast<const Image*>(content);
            if (image)
            {
                const QString url = image->getUrl().toString();
                if (image->getHeight() == 0)
                {
                    html += QString(" <img align=\"top\" src=\"%1\"> ").arg(url);
                }
                else
                {
                    html += QString(" <img align=\"top\" height=\"%1\" width=\"%1\" src=\"%2\"> ").arg(image->getHeight()).arg(url);
                }
            }
        }
            break;

        case Message::Content::Type::Hyperlink:
        {
            const Hyperlink* hyperlink = static_cast<const Hyperlink*>(content);
            if (hyperlink)
            {
                html += " <a href=\"" + hyperlink->getUrl().toString() + "\">" + hyperlink->getText() + "</a> ";
            }
        }
            break;
        }
    }

    trimText(html);

    html = html.replace('\n', "<br>");
}
