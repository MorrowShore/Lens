#include "messagesmodel.h"
#include "author.h"
#include "message.h"
#include <QCoreApplication>
#include <QTranslator>
#include <QUuid>
#include <QDebug>

namespace
{
const int MaxSize  = 10; // 1000

const QHash<int, QByteArray> RoleNames = QHash<int, QByteArray>
{
    {(int)Message::Role::Id ,                           "messageId"},
    {(int)Message::Role::Html ,                         "messageHtml"},
    {(int)Message::Role::PublishedAt ,                  "messagePublishedAt"},
    {(int)Message::Role::ReceivedAt ,                   "messageReceivedAt"},
    {(int)Message::Role::IsBotCommand ,                 "messageIsBotCommand"},
    {(int)Message::Role::MarkedAsDeleted ,              "messageMarkedAsDeleted"},
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

    {(int)Author::Role::ServiceType ,                       "authorServiceType"},
    {(int)Author::Role::Id ,                                "authorId"},
    {(int)Author::Role::PageUrl ,                           "authorPageUrl"},
    {(int)Author::Role::Name ,                              "authorName"},
    {(int)Author::Role::HasCustomNicknameColor,             "authorHasCustomNicknameColor"},
    {(int)Author::Role::CustomNicknameColor,                "authorCustomNicknameColor"},
    {(int)Author::Role::HasCustomNicknameBackgroundColor,   "authorHasCustomNicknameBackgroundColor"},
    {(int)Author::Role::CustomNicknameBackgroundColor,      "authorCustomNicknameBackgroundColor"},
    {(int)Author::Role::AvatarUrl ,                         "authorAvatarUrl"},
    {(int)Author::Role::LeftBadgesUrls ,                    "authorLeftBadgesUrls"},
    {(int)Author::Role::RightBadgesUrls,                    "authorRightBadgesUrls"},
};

}

QHash<int, QByteArray> MessagesModel::roleNames() const
{
    return RoleNames;
}

void MessagesModel::append(const std::shared_ptr<Message>& message)
{
    //ToDo: добавить сортировку сообщений по времени

    if (!message)
    {
        qWarning() << Q_FUNC_INFO << "message is null";
        return;
    }

    if (message->getId().isEmpty())
    {
        qWarning() << Q_FUNC_INFO << "message id is empty";
        return;
    }

    if (message->isHasFlag(Message::Flag::DeleterItem))
    {
        //Deleter

        std::shared_ptr<Message> prevMessage = _dataById.value(message->getId());
        if (!prevMessage)
        {
            return;
        }

        prevMessage->setFlag(Message::Flag::MarkedAsDeleted, true);

        prevMessage->setPlainText(tr("Message deleted"));

        const QModelIndex index = createIndexByData(message);
        emit dataChanged(index, index);
    }
    else
    {
        if (_dataById.contains(message->getId()))
        {
            qCritical() << Q_FUNC_INFO << "ignore message because this id" << message->getId() << "already exists";
            return;
        }

        //Normal message

        beginInsertRows(QModelIndex(), _data.count(), _data.count());

        message->setRow(lastRow);
        lastRow++;

        _dataById.insert(message->getId(), message);
        dataByRow.insert(message->getRow(), message);
        rowById[message->getId()] = message->getRow();

        Author* author = getAuthor(message->getAuthorId());

        std::set<uint64_t>& messagesIds = author->getMessagesIds();
        messagesIds.insert(message->getRow());

        _data.append(message);

        //printMessageInfo("New message:", rawMessage);

        endInsertRows();
    }

    //qDebug() << "Count:" << _data.count();

    //Remove old messages
    if (_data.count() >= MaxSize)
    {
        removeRows(0, _data.count() - MaxSize);
    }
}

bool MessagesModel::removeRows(int row, int count, const QModelIndex &parent)
{
    Q_UNUSED(parent)

    beginRemoveRows(QModelIndex(), row, row + count - 1);

    for (int row = 0; row < count; ++row)
    {
        const std::shared_ptr<Message>& message = _data[row];

        const QString& id = message->getId();

        _dataById.remove(id);
        dataByRow.remove(row);
        rowById.erase(id);
        _data.removeAt(row);

        removedRows++;
    }

    endRemoveRows();

    return true;
}

void MessagesModel::clear()
{
    removeRows(0, rowCount(QModelIndex()));
}

QModelIndex MessagesModel::createIndexById(const QString& id) const
{
    if (auto it = rowById.find(id); it != rowById.end())
    {
        return createIndex(it->second - removedRows, 0);
    }

    return QModelIndex();
}

QModelIndex MessagesModel::createIndexByData(const std::shared_ptr<Message>& message) const
{
    if (!message)
    {
        return QModelIndex();
    }

    return createIndexById(message->getId());
}

void MessagesModel::setAuthorValues(const AxelChat::ServiceType serviceType, const QString& authorId, const QMap<Author::Role, QVariant>& values)
{
    Author* author = _authorsById.value(authorId, nullptr);
    if (!author)
    {
        insertAuthor(Author(serviceType, "<blank>", authorId));
        author = _authorsById.value(authorId, nullptr);
    }

    if (!author)
    {
        qCritical() << Q_FUNC_INFO << ": author id" << authorId << "not found";
        return;
    }

    QVector<int> rolesInt;
    const QList<Author::Role> roles = values.keys();
    for (const Author::Role role : roles)
    {
        author->setValue(role, values[role]);
        rolesInt.append((int)role);
    }

    const std::set<uint64_t>& messagesIds = author->getMessagesIds();
    for (const uint64_t id : messagesIds)
    {
        std::shared_ptr<Message> message = dataByRow.value(id);
        const QModelIndex index = createIndexByData(message);
        emit dataChanged(index, index, rolesInt);
    }
}

QList<std::shared_ptr<Message>> MessagesModel::getLastMessages(int count) const
{
    QList<std::shared_ptr<Message>> result;

    for (int64_t i = _data.count() - 1; i >= 0; --i)
    {
        const std::shared_ptr<Message>& message = _data[i];
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

void MessagesModel::insertAuthor(const Author& author)
{
    const QString& id = author.getId();
    Author* prevAuthor = _authorsById.value(id, nullptr);
    if (prevAuthor)
    {
        std::set<uint64_t>& prevMssagesIds = prevAuthor->getMessagesIds();
        for (const uint64_t newMessageId : author.getMessagesIds())
        {
            prevMssagesIds.insert(newMessageId);
        }

        QMap<Author::Role, QVariant> values;

        for (const Author::Role role : Author::UpdateableRoles)
        {
            values.insert(role, author.getValue(role));
        }

        setAuthorValues(author.getServiceType(), id, values);
    }
    else
    {
        Author* newAuthor = new Author();
        *newAuthor = author;
        _authorsById.insert(id, newAuthor);
    }
}

bool MessagesModel::contains(const QString &id)
{
    return _dataById.contains(id);
}

int MessagesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return _data.count();
}

QVariant MessagesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (index.row() >= _data.size())
    {
        return QVariant();
    }

    const std::shared_ptr<Message>& message = _data.value(index.row());
    return dataByRole(*message, role);
}

QVariant MessagesModel::dataByRole(const Message &message, int role) const
{
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

    const Author* author = getAuthor(message.getAuthorId());
    if (!author)
    {
        return QVariant();
    }

    return author->getValue((Author::Role)role);
}
