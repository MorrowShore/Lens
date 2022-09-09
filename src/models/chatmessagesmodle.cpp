#include "chatmessagesmodle.hpp"
#include "author.h"
#include "chatmessage.h"
#include <QCoreApplication>
#include <QTranslator>
#include <QUuid>

const QHash<int, QByteArray> ChatMessagesModel::_roleNames = QHash<int, QByteArray>{
    {(int)ChatMessage::Role::MessageId ,                     "messageId"},
    {(int)ChatMessage::Role::MessageHtml ,                   "messageHtml"},
    {(int)ChatMessage::Role::MessagePublishedAt ,            "messagePublishedAt"},
    {(int)ChatMessage::Role::MessageReceivedAt ,             "messageReceivedAt"},
    {(int)ChatMessage::Role::MessageIsBotCommand ,           "messageIsBotCommand"},
    {(int)ChatMessage::Role::MessageMarkedAsDeleted ,        "messageMarkedAsDeleted"},
    {(int)ChatMessage::Role::MessageCustomAuthorAvatarUrl,   "messageCustomAuthorAvatarUrl"},
    {(int)ChatMessage::Role::MessageCustomAuthorName,        "messageCustomAuthorName"},

    {(int)ChatMessage::Role::MessageIsDonateSimple,      "messageIsDonateSimple"},
    {(int)ChatMessage::Role::MessageIsDonateWithText,    "messageIsDonateWithText"},
    {(int)ChatMessage::Role::MessageIsDonateWithImage,   "messageIsDonateWithImage"},

    {(int)ChatMessage::Role::MessageIsServiceMessage,           "messageIsServiceMessage"},
    {(int)ChatMessage::Role::MessageIsYouTubeChatMembership,    "messageIsYouTubeChatMembership"},
    {(int)ChatMessage::Role::MessageIsTwitchAction,             "messageIsTwitchAction"},

    {(int)ChatMessage::Role::MessageBodyBackgroundForcedColor, "messageBodyBackgroundForcedColor"},

    {(int)Author::Role::ServiceType ,      "authorServiceType"},
    {(int)Author::Role::Id ,               "authorId"},
    {(int)Author::Role::AuthorPageUrl ,          "authorPageUrl"},
    {(int)Author::Role::Name ,             "authorName"},
    {(int)Author::Role::NicknameColor ,    "authorNicknameColor"},
    {(int)Author::Role::AvatarUrl ,        "authorAvatarUrl"},
    {(int)Author::Role::LeftBadgesUrls ,   "authorLeftBadgesUrls"},
    {(int)Author::Role::RightBadgesUrls,   "authorRightBadgesUrls"},

    {(int)Author::Role::IsVerified ,       "authorIsVerified"},
    {(int)Author::Role::IsChatOwner ,      "authorIsChatOwner"},
    {(int)Author::Role::Sponsor ,        "authorIsSponsor"},
    {(int)Author::Role::Moderator ,      "authorIsModerator"}
};


void ChatMessagesModel::append(ChatMessage&& message)
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

    if (!message.isHasFlag(ChatMessage::Flag::DeleterItem))
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
            qDebug(QString("%1: ignore message because this id already exists")
                   .arg(Q_FUNC_INFO).toUtf8());

            const QVariant* data = _dataById.value(message.getId());
            if (data)
            {
                const ChatMessage& oldMessage = qvariant_cast<ChatMessage>(*data);

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
                if (!setData(index, true, (int)ChatMessage::Role::MessageMarkedAsDeleted))
                {
                    qDebug(QString("%1: failed to set data with role ChatMessageRoles::MessageMarkedAsDeleted")
                           .arg(Q_FUNC_INFO).toUtf8());

                    message.printMessageInfo("Raw message:");
                }

                if (!setData(index, message.getHtml(), (int)ChatMessage::Role::MessageHtml))
                {
                    qDebug(QString("%1: failed to set data with role ChatMessageRoles::MessageText")
                           .arg(Q_FUNC_INFO).toUtf8());

                    message.printMessageInfo("Raw message:");
                }

                //qDebug(QString("Message \"%1\" marked as deleted").arg(rawMessage.id()).toUtf8());
            }
            else
            {
                qDebug(QString("%1: index not valid")
                       .arg(Q_FUNC_INFO).toUtf8());

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

bool ChatMessagesModel::removeRows(int position, int rows, const QModelIndex &parent)
{
    Q_UNUSED(parent)

    beginRemoveRows(QModelIndex(), position, position + rows - 1);

    for (int row = 0; row < rows; ++row)
    {
        QVariant* messageData = _data[position];

        const ChatMessage& message = qvariant_cast<ChatMessage>(*messageData);

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

void ChatMessagesModel::clear()
{
    removeRows(0, rowCount(QModelIndex()));
}

uint64_t ChatMessagesModel::lastIdNum() const
{
    return _lastIdNum;
}

QModelIndex ChatMessagesModel::createIndexByPtr(QVariant *data) const
{
    if (_idNumByData.contains(data) && data != nullptr)
    {
        return createIndex(_idNumByData.value(data) - _removedRows, 0);
    }
    else
    {
        return QModelIndex();
    }
}

int ChatMessagesModel::getRow(QVariant *data)
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

void ChatMessagesModel::setAuthorData(const QString &authorId, const QVariant& value, const Author::Role role)
{
    Author* author = _authorsById.value(authorId, nullptr);
    if (!author)
    {
        qCritical() << Q_FUNC_INFO << ": author id" << authorId << "not found";
        return;
    }

    const std::set<uint64_t>& messagesIds = author->getMessagesIds();

    for (const uint64_t& oldIdNum : messagesIds)
    {
        if (_dataByIdNum.contains(oldIdNum))
        {
            QVariant* data = _dataByIdNum[oldIdNum];
            if (data)
            {
                const QModelIndex& index = createIndexByPtr(data);
                if (!setData(index, value, (int)role))
                {
                    qCritical() << Q_FUNC_INFO << "failed to set author data, role =" << role << ", author id =" << authorId;
                }
            }
        }
    }
}

void ChatMessagesModel::insertAuthor(const Author& author)
{
    const QString& id = author.getId();
    Author* prevAuthor = _authorsById.value(id, nullptr);
    if (prevAuthor)
    {
        std::set<uint64_t> prevMssagesIds = prevAuthor->getMessagesIds();
        for (const uint64_t newMessageId : author.getMessagesIds())
        {
            prevMssagesIds.insert(newMessageId);
        }

        *prevAuthor = author;
        prevAuthor->getMessagesIds() = prevMssagesIds;
        // TODO: update message data
    }
    else
    {
        Author* newAuthor = new Author();
        *newAuthor = author;
        _authorsById.insert(id, newAuthor);
    }
}

bool ChatMessagesModel::contains(const QString &id)
{
    return _dataById.contains(id);
}

int ChatMessagesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return _data.count();
}

QVariant ChatMessagesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (index.row() >= _data.size())
    {
        return QVariant();
    }

    const QVariant* data = _data.value(index.row());
    const ChatMessage& message = qvariant_cast<ChatMessage>(*data);
    return dataByRole(message, role);
}

bool ChatMessagesModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.row() >= _data.size())
    {
        return false;
    }

    ChatMessage message = qvariant_cast<ChatMessage>(*_data[index.row()]);

    switch ((ChatMessage::Role)role)
    {
    case ChatMessage::Role::MessageCustomAuthorAvatarUrl:
        break;
    case ChatMessage::Role::MessageCustomAuthorName:
        break;
    case ChatMessage::Role::MessageIsDonateSimple:
        break;
    case ChatMessage::Role::MessageIsDonateWithText:
        break;
    case ChatMessage::Role::MessageIsDonateWithImage:
        break;
    case ChatMessage::Role::MessageIsServiceMessage:
        break;
    case ChatMessage::Role::MessageBodyBackgroundForcedColor:
        break;
    case ChatMessage::Role::MessageIsYouTubeChatMembership:
        break;
    case ChatMessage::Role::MessageIsTwitchAction:
        break;
    case ChatMessage::Role::MessageId:
        return false;
    case ChatMessage::Role::MessageHtml:
        if (value.canConvert(QMetaType::QString))
        {
            message.setPlainText(value.toString());
        }
        else
        {
            return false;
        }
        break;
    case ChatMessage::Role::MessagePublishedAt:
        return false;
    case ChatMessage::Role::MessageReceivedAt:
        return false;
    case ChatMessage::Role::MessageIsBotCommand:
        return false;
    case ChatMessage::Role::MessageMarkedAsDeleted:
        if (value.canConvert(QMetaType::Bool))
        {
            message.setFlag(ChatMessage::Flag::MarkedAsDeleted, value.toBool());
        }
        else
        {
            return false;
        }
        break;
    }

    switch ((Author::Role)role)
    {
    case Author::Role::ServiceType:
        break;
    case Author::Role::NicknameColor:
        break;
    case Author::Role::Id:
        return false;
    case Author::Role::AuthorPageUrl:
        return false;
    case Author::Role::Name:
        return false;
    case Author::Role::AvatarUrl:
        if (value.canConvert(QMetaType::QUrl))
        {
            Author* author = getAuthor(message.getAuthorId());
            if (author)
            {
                author->setAvatarUrl(value.toUrl());
            }
        }
        else
        {
            return false;
        }
        break;
    case Author::Role::LeftBadgesUrls:
        return false;
    case Author::Role::RightBadgesUrls:
        return false;
    case Author::Role::IsVerified:
        return false;
    case Author::Role::IsChatOwner:
        return false;
    case Author::Role::Sponsor:
        return false;
    case Author::Role::Moderator:
        return false;
    }

    _data[index.row()]->setValue(message);

    emit dataChanged(index, index, {role});
    return true;
}

QVariant ChatMessagesModel::dataByRole(const ChatMessage &message, int role) const
{
    switch ((ChatMessage::Role)role)
    {
    case ChatMessage::Role::MessageId:
        return message.getId();
    case ChatMessage::Role::MessageHtml:
        return message.getHtml();
    case ChatMessage::Role::MessagePublishedAt:
        return message.getPublishedAt();
    case ChatMessage::Role::MessageReceivedAt:
        return message.getReceivedAt();
    case ChatMessage::Role::MessageIsBotCommand:
        return message.isHasFlag(ChatMessage::Flag::BotCommand);
    case ChatMessage::Role::MessageMarkedAsDeleted:
        return message.isHasFlag(ChatMessage::Flag::MarkedAsDeleted);
    case ChatMessage::Role::MessageCustomAuthorAvatarUrl:
        return message.getCustomAuthorAvatarUrl();
    case ChatMessage::Role::MessageCustomAuthorName:
        return message.getCustomAuthorName();

    case ChatMessage::Role::MessageIsDonateSimple:
        return message.isHasFlag(ChatMessage::Flag::DonateSimple);
    case ChatMessage::Role::MessageIsDonateWithText:
        return message.isHasFlag(ChatMessage::Flag::DonateWithText);
    case ChatMessage::Role::MessageIsDonateWithImage:
        return message.isHasFlag(ChatMessage::Flag::DonateWithImage);

    case ChatMessage::Role::MessageIsServiceMessage:
        return message.isHasFlag(ChatMessage::Flag::ServiceMessage);

    case ChatMessage::Role::MessageIsYouTubeChatMembership:
        return message.isHasFlag(ChatMessage::Flag::YouTubeChatMembership);

    case ChatMessage::Role::MessageIsTwitchAction:
        return message.isHasFlag(ChatMessage::Flag::TwitchAction);

    case ChatMessage::Role::MessageBodyBackgroundForcedColor:
        return message.getForcedColorRoleToQMLString(ChatMessage::ForcedColorRole::BodyBackgroundForcedColorRole);
    }

    const Author* author = getAuthor(message.getAuthorId());
    if (!author)
    {
        return QVariant();
    }

    switch ((Author::Role)role)
    {
    case Author::Role::ServiceType:
        return (int)author->getServiceType();
    case Author::Role::Id:
        return author->getId();
    case Author::Role::AuthorPageUrl:
        return author->getPageUrl();
    case Author::Role::Name:
        return author->getName();
    case Author::Role::NicknameColor:
        return author->getCustomNicknameColor();
    case Author::Role::AvatarUrl:
        return author->getAvatarUrl();
    case Author::Role::LeftBadgesUrls:
        return author->getLeftBadgesUrls();
    case Author::Role::RightBadgesUrls:
        return author->getRightBadgesUrls();

    case Author::Role::IsVerified:
        return author->isHasFlag(Author::Flag::Verified);
    case Author::Role::IsChatOwner:
        return author->isHasFlag(Author::Flag::ChatOwner);
    case Author::Role::Sponsor:
        return author->isHasFlag(Author::Flag::Sponsor);
    case Author::Role::Moderator:
        return author->isHasFlag(Author::Flag::Moderator);
    }

    return QVariant();
}

QVariant ChatMessagesModel::dataByNumId(const uint64_t &idNum, int role)
{
    if (_dataByIdNum.contains(idNum))
    {
        const QVariant* data = _dataByIdNum.value(idNum);
        const ChatMessage message = qvariant_cast<ChatMessage>(*data);
        return dataByRole(message, role);
    }

    return QVariant();
}
