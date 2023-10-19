#include "messagesmodel.h"
#include "author.h"
#include "message.h"
#include <QCoreApplication>
#include <QTranslator>
#include <QUuid>
#include <QDebug>

namespace
{
const int MaxSize  = 1000;

const QHash<int, QByteArray> RoleNames = QHash<int, QByteArray>
{
    {(int)Message::Role::Id,                            "messageId"},
    {(int)Message::Role::Html,                          "messageHtml"},
    {(int)Message::Role::PublishedAt,                   "messagePublishedAt"},
    {(int)Message::Role::ReceivedAt,                    "messageReceivedAt"},
    {(int)Message::Role::IsBotCommand,                  "messageIsBotCommand"},
    {(int)Message::Role::MarkedAsDeleted,               "messageMarkedAsDeleted"},
    {(int)Message::Role::CustomAuthorAvatarUrl,         "messageCustomAuthorAvatarUrl"},
    {(int)Message::Role::CustomAuthorName,              "messageCustomAuthorName"},
    {(int)Message::Role::Destination,                   "messageDestination"},

    {(int)Message::Role::IsDonateSimple,                "messageIsDonateSimple"},
    {(int)Message::Role::IsDonateWithText,              "messageIsDonateWithText"},
    {(int)Message::Role::IsDonateWithImage,             "messageIsDonateWithImage"},

    {(int)Message::Role::IsServiceMessage,              "messageIsServiceMessage"},
    {(int)Message::Role::IsYouTubeChatMembership,       "messageIsYouTubeChatMembership"},
    {(int)Message::Role::IsTwitchAction,                "messageIsTwitchAction"},

    {(int)Message::Role::BodyBackgroundForcedColor,     "messageBodyBackgroundForcedColor"},

    {(int)Author::Role::ServiceType,                        "authorServiceType"},
    {(int)Author::Role::Id,                                 "authorId"},
    {(int)Author::Role::PageUrl,                            "authorPageUrl"},
    {(int)Author::Role::Name,                               "authorName"},
    {(int)Author::Role::HasCustomNicknameColor,             "authorHasCustomNicknameColor"},
    {(int)Author::Role::CustomNicknameColor,                "authorCustomNicknameColor"},
    {(int)Author::Role::HasCustomNicknameBackgroundColor,   "authorHasCustomNicknameBackgroundColor"},
    {(int)Author::Role::CustomNicknameBackgroundColor,      "authorCustomNicknameBackgroundColor"},
    {(int)Author::Role::AvatarUrl,                          "authorAvatarUrl"},
    {(int)Author::Role::LeftBadgesUrls,                     "authorLeftBadgesUrls"},
    {(int)Author::Role::RightBadgesUrls,                    "authorRightBadgesUrls"},
    {(int)Author::Role::LeftTags,                           "authorLeftTags"},
    {(int)Author::Role::RightTags,                          "authorRightTags"},
};

}

QHash<int, QByteArray> MessagesModel::roleNames() const
{
    return RoleNames;
}

void MessagesModel::addMessage(const std::shared_ptr<Message>& message)
{
    //ToDo: добавить сортировку сообщений по времени

    if (!message)
    {
        qWarning() << "message is null";
        return;
    }

    if (message->getId().isEmpty())
    {
        qWarning() << "message id is empty";
        return;
    }

    if (message->isHasFlag(Message::Flag::DeleterItem))
    {
        //Deleter

        std::shared_ptr<Message> prevMessage = messagesById.value(message->getId());
        if (!prevMessage)
        {
            return;
        }

        prevMessage->setFlag(Message::Flag::MarkedAsDeleted, true);

        prevMessage->setPlainText(tr("Message deleted"));

        const QModelIndex index = createIndexByExistingMessageId(message->getId());
        emit dataChanged(index, index);
    }
    else
    {
        if (messagesById.contains(message->getId()))
        {
            qCritical() << "ignore message because this id" << message->getId() << "already exists";
            return;
        }

        //Normal message

        beginInsertRows(QModelIndex(), messages.count(), messages.count());

        message->setRow(lastRow);
        lastRow++;

        messagesById.insert(message->getId(), message);

        if (std::shared_ptr<Author> author = getAuthor(message->getAuthorId()); author)
        {
            std::set<QString>& messagesIds = author->getMessagesIds();
            messagesIds.insert(message->getId());
        }
        else
        {
            qWarning() << "author is null";
        }

        messages.append(message);

        //printMessageInfo("New message:", rawMessage);

        endInsertRows();
    }

    //qDebug() << "Count:" << _data.count();

    //Remove old messages
    if (messages.count() >= MaxSize)
    {
        removeRows(0, messages.count() - MaxSize);
    }
}

bool MessagesModel::removeRows(int firstRow, int count, const QModelIndex &parent)
{
    Q_UNUSED(parent)

    if (count > messages.count())
    {
        count = messages.count();
    }

    beginRemoveRows(QModelIndex(), firstRow, firstRow + count - 1);

    for (int i = 0; i < count; ++i)
    {
        const std::shared_ptr<Message>& message = messages[firstRow];

        const QString& id = message->getId();

        messagesById.remove(id);
        messages.removeAt(firstRow);

        removedRows++;
    }

    endRemoveRows();

    return true;
}

void MessagesModel::clear()
{
    removeRows(0, rowCount(QModelIndex()));
}

QModelIndex MessagesModel::createIndexByExistingMessageId(const QString& id) const
{
    std::shared_ptr<Message> message = messagesById.value(id);
    if (!message)
    {
        return QModelIndex();
    }

    return createIndex(message->getRow() - removedRows, 0);
}

void MessagesModel::setAuthorValues(const ChatServiceType serviceType, const QString& authorId, const QMap<Author::Role, QVariant>& values)
{
    std::shared_ptr<Author> author = authors.value(authorId, nullptr);
    if (!author)
    {
        addAuthor(std::make_shared<Author>(serviceType, "<blank>", authorId));
        author = authors.value(authorId, nullptr);
    }

    QVector<int> rolesInt;
    const QList<Author::Role> roles = values.keys();
    for (const Author::Role role : roles)
    {
        author->setValue(role, values[role]);
        rolesInt.append((int)role);
    }

    const std::set<QString>& messagesIds = author->getMessagesIds();
    for (const QString& id : messagesIds)
    {
        if (std::shared_ptr<Message> message = messagesById.value(id); message)
        {
            const QModelIndex index = createIndexByExistingMessageId(message->getId());
            emit dataChanged(index, index, rolesInt);
        }
    }
}

QList<std::shared_ptr<Message>> MessagesModel::getLastMessages(int count) const
{
    QList<std::shared_ptr<Message>> result;

    for (int64_t i = messages.count() - 1; i >= 0; --i)
    {
        const std::shared_ptr<Message>& message = messages[i];
        if (message->isHasFlag(Message::Flag::DeleterItem))
        {
            continue;
        }

        result.insert(0, message);

        if (result.count() >= count)
        {
            break;
        }
    }

    return result;
}

void MessagesModel::addAuthor(const std::shared_ptr<Author>& author)
{
    if (!author)
    {
        qWarning() << "author is null";
        return;
    }

    const QString& id = author->getId();
    std::shared_ptr<Author> prevAuthor = authors.value(id, nullptr);
    if (prevAuthor)
    {
        std::set<QString>& prevMssagesIds = prevAuthor->getMessagesIds();
        for (const QString& newMessageId : author->getMessagesIds())
        {
            prevMssagesIds.insert(newMessageId);
        }

        QMap<Author::Role, QVariant> values;

        for (const Author::Role role : Author::UpdateableRoles)
        {
            values.insert(role, author->getValue(role));
        }

        setAuthorValues(author->getServiceType(), id, values);
    }
    else
    {
        authors.insert(id, author);
    }
}

bool MessagesModel::contains(const QString &id)
{
    return messagesById.contains(id);
}

int MessagesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return messages.count();
}

QVariant MessagesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (index.row() >= messages.size())
    {
        return QVariant();
    }

    return dataByRole(messages.value(index.row()), role);
}

QVariant MessagesModel::dataByRole(const std::shared_ptr<Message>& message_, int role) const
{
    if (!message_)
    {
        return QVariant();
    }

    const Message& message = *message_;

    switch ((Message::Role)role)
    {
    case Message::Role::Id:
        return message.getId();
    case Message::Role::Html:
        return message.toHtml();
    case Message::Role::PublishedAt:
        return message.getPublishedAt();
    case Message::Role::ReceivedAt:
        return message.getReceivedAt();
    case Message::Role::IsBotCommand:
        return message.isHasFlag(Message::Flag::BotCommand);
    case Message::Role::MarkedAsDeleted:
        return message.isHasFlag(Message::Flag::MarkedAsDeleted);
    case Message::Role::CustomAuthorAvatarUrl:
        return message.getCustomAuthorAvatarUrl();
    case Message::Role::CustomAuthorName:
        return message.getCustomAuthorName();
    case Message::Role::Destination:
        return message.getDestination();

    case Message::Role::IsDonateSimple:
        return message.isHasFlag(Message::Flag::DonateSimple);
    case Message::Role::IsDonateWithText:
        return message.isHasFlag(Message::Flag::DonateWithText);
    case Message::Role::IsDonateWithImage:
        return message.isHasFlag(Message::Flag::DonateWithImage);

    case Message::Role::IsServiceMessage:
        return message.isHasFlag(Message::Flag::ServiceMessage);

    case Message::Role::IsYouTubeChatMembership:
        return message.isHasFlag(Message::Flag::YouTubeChatMembership);

    case Message::Role::IsTwitchAction:
        return message.isHasFlag(Message::Flag::TwitchAction);

    case Message::Role::BodyBackgroundForcedColor:
        return message.getForcedColorRoleToQMLString(Message::ColorRole::BodyBackgroundColorRole);
    }

    const std::shared_ptr<Author> author = getAuthor(message.getAuthorId());
    if (!author)
    {
        return QVariant();
    }

    return author->getValue((Author::Role)role);
}
