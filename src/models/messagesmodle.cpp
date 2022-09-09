#include "messagesmodle.h"
#include "author.h"
#include "message.h"
#include <QCoreApplication>
#include <QTranslator>
#include <QUuid>
#include <QDebug>

const QHash<int, QByteArray> MessagesModel::_roleNames = QHash<int, QByteArray>
{
    {(int)Message::Role::Id ,                           "messageId"},
    {(int)Message::Role::Html ,                         "messageHtml"},
    {(int)Message::Role::PublishedAt ,                  "messagePublishedAt"},
    {(int)Message::Role::ReceivedAt ,                   "messageReceivedAt"},
    {(int)Message::Role::IsBotCommand ,                 "messageIsBotCommand"},
    {(int)Message::Role::MarkedAsDeleted ,              "messageMarkedAsDeleted"},
    {(int)Message::Role::CustomAuthorAvatarUrl,         "messageCustomAuthorAvatarUrl"},
    {(int)Message::Role::CustomAuthorName,              "messageCustomAuthorName"},

    {(int)Message::Role::IsDonateSimple,                "messageIsDonateSimple"},
    {(int)Message::Role::IsDonateWithText,              "messageIsDonateWithText"},
    {(int)Message::Role::IsDonateWithImage,             "messageIsDonateWithImage"},

    {(int)Message::Role::IsServiceMessage,              "messageIsServiceMessage"},
    {(int)Message::Role::IsYouTubeChatMembership,       "messageIsYouTubeChatMembership"},
    {(int)Message::Role::IsTwitchAction,                "messageIsTwitchAction"},

    {(int)Message::Role::BodyBackgroundForcedColor,     "messageBodyBackgroundForcedColor"},

    {(int)Author::Role::ServiceType ,       "authorServiceType"},
    {(int)Author::Role::Id ,                "authorId"},
    {(int)Author::Role::PageUrl ,           "authorPageUrl"},
    {(int)Author::Role::Name ,              "authorName"},
    {(int)Author::Role::NicknameColor ,     "authorNicknameColor"},
    {(int)Author::Role::AvatarUrl ,         "authorAvatarUrl"},
    {(int)Author::Role::LeftBadgesUrls ,    "authorLeftBadgesUrls"},
    {(int)Author::Role::RightBadgesUrls,    "authorRightBadgesUrls"},

    {(int)Author::Role::IsVerified ,        "authorIsVerified"},
    {(int)Author::Role::IsChatOwner ,       "authorIsChatOwner"},
    {(int)Author::Role::Sponsor ,           "authorIsSponsor"},
    {(int)Author::Role::Moderator ,         "authorIsModerator"}
};


void MessagesModel::append(Message&& message)
{
    //ToDo: добавить сортировку сообщений по времени

    if (message.getId().isEmpty())
    {
        message.printMessageInfo("Ignore message with empty id");
        return;
    }


    if (message.getId().isEmpty())
    {
        //message.printMessageInfo(QString("%1: Ignore message with empty id:").arg(Q_FUNC_INFO));
        return;
    }

    if (!message.isHasFlag(Message::Flag::DeleterItem))
    {
        if (!_dataById.contains(message.getId()))
        {
            //Normal message

            beginInsertRows(QModelIndex(), _data.count(), _data.count());

            message.setIdNum(_lastIdNum);
            _lastIdNum++;

            QVariant* messageData = new QVariant();

            messageData->setValue(message);

            _idByData.insert(messageData, message.getId());
            _dataById.insert(message.getId(), messageData);
            _dataByIdNum.insert(message.getIdNum(), messageData);
            _idNumByData.insert(messageData, message.getIdNum());

            Author* author = getAuthor(message.getAuthorId());

            std::set<uint64_t>& messagesIds = author->getMessagesIds();
            messagesIds.insert(message.getIdNum());

            _data.append(messageData);

            //printMessageInfo("New message:", rawMessage);

            endInsertRows();
        }
        else
        {
            qCritical() << Q_FUNC_INFO << "ignore message because this id" << message.getId() << "already exists";

            const QVariant* data = _dataById.value(message.getId());
            if (data)
            {
                const Message& oldMessage = qvariant_cast<Message>(*data);

                message.printMessageInfo("Raw new message:");
                oldMessage.printMessageInfo("Old message:");
            }
            else
            {
                message.printMessageInfo("Raw new message:");
                qDebug("Old message: nullptr");
            }
        }
    }
    else
    {
        //Deleter

        //ToDo: Если пришёл делетер, а сообщение ещё нет, то когда это сообщение придёт не будет удалено

        QVariant* data = _dataById[message.getId()];
        if (_dataById.contains(message.getId()) && data)
        {
            const QModelIndex& index = createIndexByPtr(data);
            if (index.isValid())
            {
                if (!setData(index, true, (int)Message::Role::MarkedAsDeleted))
                {
                    qCritical() << Q_FUNC_INFO << ": failed to set data with role" << Message::Role::MarkedAsDeleted;

                    message.printMessageInfo("Raw message:");
                }

                if (!setData(index, message.getHtml(), (int)Message::Role::Html))
                {
                    qCritical() << Q_FUNC_INFO << ": failed to set data with role" << Message::Role::Html;

                    message.printMessageInfo("Raw message:");
                }

                //qDebug(QString("Message \"%1\" marked as deleted").arg(rawMessage.id()).toUtf8());
            }
            else
            {
                qCritical() << Q_FUNC_INFO << ": index not valid";

                message.printMessageInfo("Raw message:");
            }
        }
    }

    //qDebug() << "Count:" << _data.count();

    //Remove old messages
    if (_data.count() >= _maxSize)
    {
        removeRows(0, _data.count() - _maxSize);
    }
}

bool MessagesModel::removeRows(int position, int rows, const QModelIndex &parent)
{
    Q_UNUSED(parent)

    beginRemoveRows(QModelIndex(), position, position + rows - 1);

    for (int row = 0; row < rows; ++row)
    {
        QVariant* messageData = _data[position];

        const Message& message = qvariant_cast<Message>(*messageData);

        const QString& id = message.getId();
        const uint64_t& idNum = message.getIdNum();

        _dataById.remove(id);
        _idByData.remove(messageData);
        _dataByIdNum.remove(idNum);
        _idNumByData.remove(messageData);
        _data.removeAt(position);

        delete messageData;
        _removedRows++;
    }

    endRemoveRows();

    return true;
}

void MessagesModel::clear()
{
    removeRows(0, rowCount(QModelIndex()));
}

uint64_t MessagesModel::lastIdNum() const
{
    return _lastIdNum;
}

QModelIndex MessagesModel::createIndexByPtr(QVariant *data) const
{
    if (!data)
    {
        return QModelIndex();
    }

    if (_idNumByData.contains(data) && data != nullptr)
    {
        return createIndex(_idNumByData.value(data) - _removedRows, 0);
    }
    else
    {
        return QModelIndex();
    }
}

int MessagesModel::getRow(QVariant *data)
{
    if (data)
    {
        const QModelIndex& index = createIndexByPtr(data);
        if (index.isValid())
        {
            return index.row();
        }
        else
        {
            return -1;
        }
    }
    else
    {
        return -1;
    }
}

void MessagesModel::setAuthorValues(const QString& authorId, const QMap<Author::Role, QVariant>& values)
{
    Author* author = _authorsById.value(authorId, nullptr);
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
        const QModelIndex index = createIndexByPtr(_dataByIdNum.value(id));
        emit dataChanged(index, index, rolesInt);
    }
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

        setAuthorValues(id, values);
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

    const QVariant* data = _data.value(index.row());
    const Message& message = qvariant_cast<Message>(*data);
    return dataByRole(message, role);
}

bool MessagesModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.row() >= _data.size())
    {
        return false;
    }

    Message message = qvariant_cast<Message>(*_data[index.row()]);

    switch ((Message::Role)role)
    {
    case Message::Role::Html:
        if (value.canConvert(QMetaType::QString))
        {
            message.setPlainText(value.toString());
        }
        else
        {
            return false;
        }
        break;
    case Message::Role::MarkedAsDeleted:
        if (value.canConvert(QMetaType::Bool))
        {
            message.setFlag(Message::Flag::MarkedAsDeleted, value.toBool());
        }
        else
        {
            return false;
        }
        break;

    default:
        qCritical() << Q_FUNC_INFO << ": role not updatable";
        return false;
    }

    _data[index.row()]->setValue(message);

    emit dataChanged(index, index, {role});
    return true;
}

QVariant MessagesModel::dataByRole(const Message &message, int role) const
{
    switch ((Message::Role)role)
    {
    case Message::Role::Id:
        return message.getId();
    case Message::Role::Html:
        return message.getHtml();
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
        return message.getForcedColorRoleToQMLString(Message::ForcedColorRole::BodyBackgroundForcedColorRole);
    }

    const Author* author = getAuthor(message.getAuthorId());
    if (!author)
    {
        return QVariant();
    }

    return author->getValue((Author::Role)role);
}

QVariant MessagesModel::dataByNumId(const uint64_t &idNum, int role)
{
    if (_dataByIdNum.contains(idNum))
    {
        const QVariant* data = _dataByIdNum.value(idNum);
        const Message message = qvariant_cast<Message>(*data);
        return dataByRole(message, role);
    }

    return QVariant();
}