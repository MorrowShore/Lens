#include "chatmessage.h"
#include <QUuid>

ChatMessage::ChatMessage(const QString &text_,
                         const QString &authorId_,
                         const QDateTime &publishedAt_,
                         const QDateTime &receivedAt_,
                         const QString &messageId_,
                         const QMap<QUrl, QList<int>>& images_,
                         const QSet<Flags> &flags_,
                         const QHash<ForcedColorRoles, QColor> &forcedColors_)
    : _text(text_)
    , _messageId(messageId_)
    , _publishedAt(publishedAt_)
    , _receivedAt(receivedAt_)
    , _authorId(authorId_)
    , _flags(flags_)
    , _forcedColors(forcedColors_)
    , images(images_)
{
    if (_messageId.isEmpty())
    {
        _messageId = _authorId + "/" + QUuid::createUuid().toString(QUuid::Id128);
    }
}

ChatMessage ChatMessage::createYouTubeDeleter(const QString& text,const QString& id)
{
    ChatMessage message = ChatMessage();

    message._text          = text;
    message._messageId            = id;
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

    resultString += "\nAuthor Id: \"" + getAuthorId() + "\"";
    resultString += "\nMessage Text: \"" + getText() + "\"";
    resultString += "\nMessage Id: \"" + getMessageId() + "\"";
    resultString += QString("\nMessage Id Num: %1").arg(getIdNum());

    if (row != -1)
    {
        resultString += QString("\nMessage Row: %1").arg(row);
    }
    else
    {
        resultString += "\nMessage Row: failed to retrieve";
    }

    resultString += "\nMessage Is Bot Command: " + boolToString(isBotCommand());
    resultString += "\nMessage Is Marked as Deleted: " + boolToString(isMarkedAsDeleted());
    resultString += "\nMessage Is Deleter: " + boolToString(isDeleterItem());
    //ToDo: message._type message._receivedAt message._publishedAt
    //ToDo: other author data

    resultString += "\n===========================";
    qDebug(resultString.toUtf8());
}

QString ChatMessage::getForcedColorRoleToQMLString(const ForcedColorRoles &role) const
{
    if (_forcedColors.contains(role) && _forcedColors[role].isValid())
    {
        return _forcedColors[role].name(QColor::HexArgb);
    }

    return QString();
}

void ChatMessage::trimText(QString &text)
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
