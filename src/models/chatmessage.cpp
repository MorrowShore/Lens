#include "chatmessage.h"
#include <QUuid>

ChatMessage::ChatMessage(const QString &text_,
                         const QString &id_,
                         const QDateTime &publishedAt_,
                         const QDateTime &receivedAt_,
                         const QString &authorId_,
                         const QSet<Flags> &flags_,
                         const QHash<ForcedColorRoles, QColor> &forcedColors_)
    : _text(text_)
    , _id(id_)
    , _publishedAt(publishedAt_)
    , _receivedAt(receivedAt_)
    , _authorId(authorId_)
    , _flags(flags_)
    , _forcedColors(forcedColors_)
{

}

ChatMessage ChatMessage::createFromYouTube(const QString& text,
                                       const QString& id,
                                       const QDateTime& publishedAt,
                                       const QDateTime& receivedAt,
                                       const QString& authorId,
                                       const QSet<Flags>& flags,
                                       const QHash<ForcedColorRoles, QColor>& forcedColors)
{
    ChatMessage message = ChatMessage();

    message._text        = text;
    message._id          = id;
    message._publishedAt = publishedAt;
    message._receivedAt  = receivedAt;
    message._authorId    = authorId;
    message._flags       = flags;
    message._forcedColors = forcedColors;

    trimCustom(message._text);

    return message;
}

ChatMessage ChatMessage::createFromTwitch(const QString &text, const QDateTime &receivedAt, const QString& authorId, const QSet<Flags>& flags)
{
    ChatMessage message = ChatMessage();

    message._text        = text;
    message._id          = QUuid::createUuid().toString(QUuid::Id128);//ToDo: отказать вовсе того, чтобы id был обязателен
    message._publishedAt = receivedAt;//ToDo: возможно, это нужно исправить
    message._receivedAt  = receivedAt;
    message._authorId    = authorId;
    message._flags       = flags;

    trimCustom(message._text);

    return message;
}

ChatMessage ChatMessage::createFromGoodGame(const QString &text, const QDateTime &timestamp, const QString& authorId)
{
    ChatMessage message = ChatMessage();

    message._text        = text;
    message._id          = QUuid::createUuid().toString(QUuid::Id128);//ToDo: отказать вовсе того, чтобы id был обязателен
    message._publishedAt = timestamp;//ToDo: возможно, это нужно исправить
    message._receivedAt  = timestamp;
    message._authorId    = authorId;

    trimCustom(message._text);

    return message;
}

ChatMessage ChatMessage::createDeleterFromYouTube(const QString& text,const QString& id)
{
    ChatMessage message = ChatMessage();

    message._text          = text;
    message._id            = id;
    message._isDeleterItem = true;

    return message;
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

void ChatMessage::printMessageInfo(const QString &prefix, const int &row) const
{
    QString resultString = prefix;
    if (!resultString.isEmpty())
        resultString += "\n";

    resultString += "===========================";

    resultString += "\nAuthor Id: \"" + authorId() + "\"";
    resultString += "\nMessage Text: \"" + text() + "\"";
    resultString += "\nMessage Id: \"" + id() + "\"";
    resultString += QString("\nMessage Id Num: %1").arg(idNum());

    if (row != -1)
    {
        resultString += QString("\nMessage Row: %1").arg(row);
    }
    else
    {
        resultString += "\nMessage Row: failed to retrieve";
    }

    resultString += "\nMessage Is Bot Command: " + boolToString(isBotCommand());
    resultString += "\nMessage Is Marked as Deleted: " + boolToString(markedAsDeleted());
    resultString += "\nMessage Is Deleter: " + boolToString(isDeleterItem());
    //ToDo: message._type message._receivedAt message._publishedAt
    //ToDo: other author data

    resultString += "\n===========================";
    qDebug(resultString.toUtf8());
}

QString ChatMessage::forcedColorRoleToQMLString(const ForcedColorRoles &role) const
{
    if (_forcedColors.contains(role) && _forcedColors[role].isValid())
    {
        return _forcedColors[role].name(QColor::HexArgb);
    }

    return QString();
}

void ChatMessage::trimCustom(QString &text)
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
