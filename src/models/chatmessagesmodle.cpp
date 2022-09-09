#include "chatmessagesmodle.hpp"
#include "chatauthor.h"
#include "chatmessage.h"
#include <QCoreApplication>
#include <QTranslator>
#include <QUuid>

const QHash<int, QByteArray> ChatMessagesModel::_roleNames = QHash<int, QByteArray>{
    {MessageId ,                    "messageId"},
    {MessageHtml ,                  "messageHtml"},
    {MessagePublishedAt ,           "messagePublishedAt"},
    {MessageReceivedAt ,            "messageReceivedAt"},
    {MessageIsBotCommand ,          "messageIsBotCommand"},
    {MessageMarkedAsDeleted ,       "messageMarkedAsDeleted"},
    {MessageCustomAuthorAvatarUrl,  "messageCustomAuthorAvatarUrl"},
    {MessageCustomAuthorName,       "messageCustomAuthorName"},

    {MessageIsDonateSimple,   "messageIsDonateSimple"},
    {MessageIsDonateWithText, "messageIsDonateWithText"},
    {MessageIsDonateWithImage,"messageIsDonateWithImage"},

    {MessageIsServiceMessage,           "messageIsServiceMessage"},
    {MessageIsYouTubeChatMembership,    "messageIsYouTubeChatMembership"},
    {MessageIsTwitchAction,             "messageIsTwitchAction"},

    {MessageBodyBackgroundForcedColor, "messageBodyBackgroundForcedColor"},

    {AuthorServiceType ,      "authorServiceType"},
    {AuthorId ,               "authorId"},
    {AuthorPageUrl ,          "authorPageUrl"},
    {AuthorName ,             "authorName"},
    {AuthorNicknameColor ,    "authorNicknameColor"},
    {AuthorAvatarUrl ,        "authorAvatarUrl"},
    {AuthorLeftBadgesUrls ,   "authorLeftBadgesUrls"},
    {AuthorRightBadgesUrls,   "authorRightBadgesUrls"},

    {AuthorIsVerified ,       "authorIsVerified"},
    {AuthorIsChatOwner ,      "authorIsChatOwner"},
    {AuthorIsSponsor ,        "authorIsSponsor"},
    {AuthorIsModerator ,      "authorIsModerator"}
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

            ChatAuthor* author = getAuthor(message.getAuthorId());

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
                if (!setData(index, true, Role::MessageMarkedAsDeleted))
                {
                    qDebug(QString("%1: failed to set data with role ChatMessageRoles::MessageMarkedAsDeleted")
                           .arg(Q_FUNC_INFO).toUtf8());

                    message.printMessageInfo("Raw message:");
                }

                if (!setData(index, message.getHtml(), Role::MessageHtml))
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

void ChatMessagesModel::setAuthorData(const QString &authorId, const QVariant& value, const Role role)
{
    ChatAuthor* author = _authorsById.value(authorId, nullptr);
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
                if (!setData(index, value, role))
                {
                    qCritical() << Q_FUNC_INFO << "failed to set author data, role =" << role << ", author id =" << authorId;
                }
            }
        }
    }
}

void ChatMessagesModel::insertAuthor(const ChatAuthor& author)
{
    const QString& id = author.getId();
    ChatAuthor* prevAuthor = _authorsById.value(id, nullptr);
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
        ChatAuthor* newAuthor = new ChatAuthor();
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

    switch (role) {
    case MessageId:
        return false;
    case MessageHtml:
        if (value.canConvert(QMetaType::QString))
        {
            message.setPlainText(value.toString());
        }
        else
        {
            return false;
        }
        break;
    case AuthorServiceType:
        return false;
    case MessagePublishedAt:
        return false;
    case MessageReceivedAt:
        return false;
    case MessageIsBotCommand:
        return false;
    case MessageMarkedAsDeleted:
        if (value.canConvert(QMetaType::Bool))
        {
            message.setFlag(ChatMessage::Flag::MarkedAsDeleted, value.toBool());
        }
        else
        {
            return false;
        }
        break;

    case AuthorId:
        return false;
    case AuthorPageUrl:
        return false;
    case AuthorName:
        return false;
    case AuthorAvatarUrl:
        if (value.canConvert(QMetaType::QUrl))
        {
            ChatAuthor* author = getAuthor(message.getAuthorId());
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
    case AuthorLeftBadgesUrls:
        return false;
    case AuthorRightBadgesUrls:
        return false;
    case AuthorIsVerified:
        return false;
    case AuthorIsChatOwner:
        return false;
    case AuthorIsSponsor:
        return false;
    case AuthorIsModerator:
        return false;
    default:
        return false;
    }

    _data[index.row()]->setValue(message);

    emit dataChanged(index, index, {role});
    return true;
}

QVariant ChatMessagesModel::dataByRole(const ChatMessage &message, int role) const
{
    const ChatAuthor* author = getAuthor(message.getAuthorId());

    switch (role) {
    case MessageId:
        return message.getId();
    case MessageHtml:
        return message.getHtml();
    case MessagePublishedAt:
        return message.getPublishedAt();
    case MessageReceivedAt:
        return message.getReceivedAt();
    case MessageIsBotCommand:
        return message.isHasFlag(ChatMessage::Flag::BotCommand);
    case MessageMarkedAsDeleted:
        return message.isHasFlag(ChatMessage::Flag::MarkedAsDeleted);
    case MessageCustomAuthorAvatarUrl:
        return message.getCustomAuthorAvatarUrl();
    case MessageCustomAuthorName:
        return message.getCustomAuthorName();

    case MessageIsDonateSimple:
        return message.isHasFlag(ChatMessage::Flag::DonateSimple);
    case MessageIsDonateWithText:
        return message.isHasFlag(ChatMessage::Flag::DonateWithText);
    case MessageIsDonateWithImage:
        return message.isHasFlag(ChatMessage::Flag::DonateWithImage);

    case MessageIsServiceMessage:
        return message.isHasFlag(ChatMessage::Flag::ServiceMessage);

    case MessageIsYouTubeChatMembership:
        return message.isHasFlag(ChatMessage::Flag::YouTubeChatMembership);

    case MessageIsTwitchAction:
        return message.isHasFlag(ChatMessage::Flag::TwitchAction);

    case MessageBodyBackgroundForcedColor:
        return message.getForcedColorRoleToQMLString(ChatMessage::ForcedColorRole::BodyBackgroundForcedColorRole);

    case AuthorServiceType:
        return (int)(author ? author->getServiceType() : ChatService::ServiceType::Unknown);
    case AuthorId:
        return author ? author->getId() : QString();
    case AuthorPageUrl:
        return author ? author->getPageUrl() : QUrl();
    case AuthorName:
        return author ? author->getName() : QString();
    case AuthorNicknameColor:
        return author ? author->getCustomNicknameColor() : QColor();
    case AuthorAvatarUrl:
        return author ? author->getAvatarUrl() : QUrl();
    case AuthorLeftBadgesUrls:
        return author ? author->getLeftBadgesUrls() : QStringList();
    case AuthorRightBadgesUrls:
        return author ? author->getRightBadgesUrls() : QStringList();

    case AuthorIsVerified:
        return author ? author->isHasFlag(ChatAuthor::Flag::Verified) : false;
    case AuthorIsChatOwner:
        return author ? author->isHasFlag(ChatAuthor::Flag::ChatOwner) : false;
    case AuthorIsSponsor:
        return author ? author->isHasFlag(ChatAuthor::Flag::Sponsor) : false;
    case AuthorIsModerator:
        return author ? author->isHasFlag(ChatAuthor::Flag::Moderator) : false;
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
