#include "chatmessagesmodle.hpp"
#include "chatauthor.h"
#include "chatmessage.h"
#include <QCoreApplication>
#include <QTranslator>
#include <QUuid>

const QHash<int, QByteArray> ChatMessagesModel::_roleNames = QHash<int, QByteArray>{
    {MessageId ,              "messageId"},
    {MessageText ,            "messageText"},
    {MessagePublishedAt ,     "messagePublishedAt"},
    {MessageReceivedAt ,      "messageReceivedAt"},
    {MessageIsBotCommand ,    "messageIsBotCommand"},
    {MessageMarkedAsDeleted , "messageMarkedAsDeleted"},

    {MessageIsDonateSimple,   "messageIsDonateSimple"},
    {MessageIsDonateWithText, "messageIsDonateWithText"},
    {MessageIsDonateWithImage,"messageIsDonateWithImage"},

    {MessageIsPlatformGeneric,          "messageIsPlatformGeneric"},
    {MessageIsYouTubeChatMembership,    "messageIsYouTubeChatMembership"},
    {MessageIsTwitchAction,             "messageIsTwitchAction"},

    {MessageBodyBackgroundForcedColor, "messageBodyBackgroundForcedColor"},

    {AuthorServiceType ,      "authorServiceType"},
    {AuthorId ,               "authorId"},
    {AuthorPageUrl ,          "authorPageUrl"},
    {AuthorName ,             "authorName"},
    {AuthorNicknameColor ,    "authorNicknameColor"},
    {AuthorAvatarUrl ,        "authorAvatarUrl"},
    {AuthorCustomBadgeUrl ,   "authorCustomBadgeUrl"},
    {AuthorTwitchBadgesUrls,  "authorTwitchBadgesUrls"},
    {AuthorIsVerified ,       "authorIsVerified"},
    {AuthorIsChatOwner ,      "authorIsChatOwner"},
    {AuthorChatSponsor ,      "authorChatSponsor"},
    {AuthorChatModerator ,    "authorChatModerator"}
};


ChatMessagesModel::ChatMessagesModel(QObject *parent) : QAbstractListModel(parent)
{

}

void ChatMessagesModel::append(ChatMessage&& message)
{
    //ToDo: добавить сортировку сообщений по времени

    if (message.id().isEmpty())
    {
        message.printMessageInfo("Ignore message with empty id");
        return;
    }


    if (message.id().isEmpty())
    {
        //message.printMessageInfo(QString("%1: Ignore message with empty id:").arg(Q_FUNC_INFO));
        return;
    }

    if (!message.isDeleterItem())
    {
        if (!_dataById.contains(message.id()))
        {
            //Normal message

            beginInsertRows(QModelIndex(), _data.count(), _data.count());

            message._idNum = _lastIdNum;
            _lastIdNum++;

            QVariant* messageData = new QVariant();

            messageData->setValue(message);

            _idByData.insert(messageData, message.id());
            _dataById.insert(message.id(), messageData);
            _dataByIdNum.insert(message._idNum, messageData);
            _idNumByData.insert(messageData, message._idNum);

            const ChatAuthor* author = getAuthor(message.authorId());
            if (author && !author->avatarUrl().isValid())
            {
                const QString& authorId = author->authorId();
                if (!_needUpdateAvatarMessages.contains(authorId))
                {
                    _needUpdateAvatarMessages.insert(authorId, QSet<uint64_t>());

                }

                _needUpdateAvatarMessages[authorId].insert(message._idNum);
            }

            _data.append(messageData);

            //printMessageInfo("New message:", rawMessage);

            endInsertRows();
        }
        else
        {
            qDebug(QString("%1: ignore message because this id already exists")
                   .arg(Q_FUNC_INFO).toUtf8());

            const QVariant* data = _dataById.value(message.id());
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

        QVariant* data = _dataById[message.id()];
        if (_dataById.contains(message.id()) && data)
        {
            const QModelIndex& index = createIndexByPtr(data);
            if (index.isValid())
            {
                if (!setData(index, true, ChatMessageRoles::MessageMarkedAsDeleted))
                {
                    qDebug(QString("%1: failed to set data with role ChatMessageRoles::MessageMarkedAsDeleted")
                           .arg(Q_FUNC_INFO).toUtf8());

                    message.printMessageInfo("Raw message:");
                }

                if (!setData(index, message.text(), ChatMessageRoles::MessageText))
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

        const QString& id = message.id();
        const uint64_t& idNum = message.idNum();

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

void ChatMessagesModel::applyAvatar(const QString &channelId, const QUrl &url)
{
    for (const uint64_t& oldIdNum : qAsConst(_needUpdateAvatarMessages[channelId]))
    {
        if (_dataByIdNum.contains(oldIdNum))
        {
            QVariant* data = _dataByIdNum[oldIdNum];
            if (data)
            {
                const QModelIndex& index = createIndexByPtr(data);
                setData(index, url, ChatMessagesModel::AuthorAvatarUrl);
            }
        }
    }

    _needUpdateAvatarMessages.remove(channelId);
}

std::vector<uint64_t> ChatMessagesModel::searchByMessageText(const QString& sample, Qt::CaseSensitivity caseSensitivity)
{
    std::vector<uint64_t> result;
    result.reserve(std::min(_dataByIdNum.size(), 1000));

    for (auto it = _dataByIdNum.begin(); it != _dataByIdNum.end(); ++it)
    {
        QVariant*& data = it.value();
        if (!data)
        {
            continue;
        }

        if (((ChatMessage*&)(data))->_text.contains(sample, caseSensitivity))
        {
            result.push_back(it.key());
        }
    }

    return result;
}

const ChatAuthor &ChatMessagesModel::softwareAuthor()
{
    const QString authorId = "____" + QCoreApplication::applicationName() + "____";
    if (!_authorsById.contains(authorId))
    {
        ChatAuthor* author = new ChatAuthor();
        author->_serviceType = AbstractChatService::ServiceType::Software;
        author->_authorId = authorId;
        author->_name = QCoreApplication::applicationName();
        _authorsById.insert(authorId, author);
    }

    return *_authorsById.value(authorId);
}

const ChatAuthor &ChatMessagesModel::testAuthor()
{
    const QString authorId = "____TEST_MESSAGE____";
    if (!_authorsById.contains(authorId))
    {
        ChatAuthor* author = new ChatAuthor();
        author->_serviceType = AbstractChatService::ServiceType::Test;
        author->_authorId = authorId;
        author->_name = QTranslator::tr("Test Message");
        _authorsById.insert(authorId, author);
    }

    return *_authorsById.value(authorId);
}

void ChatMessagesModel::addAuthor(ChatAuthor* author)
{
    if (!author)
    {
        return;
    }

    _authorsById.insert(author->authorId(), author);
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
    case MessageText:
        if (value.canConvert(QMetaType::QString))
        {
            message._text = value.toString();
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
            message._markedAsDeleted = value.toBool();
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
            ChatAuthor* author = getAuthor(message._authorId);
            if (author)
            {
                author->_avatarUrl = value.toUrl();
            }
        }
        else
        {
            return false;
        }
        break;
    case AuthorCustomBadgeUrl:
        return false;
    case AuthorIsVerified:
        return false;
    case AuthorIsChatOwner:
        return false;
    case AuthorChatSponsor:
        return false;
    case AuthorChatModerator:
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
    const ChatAuthor* author = getAuthor(message._authorId);

    switch (role) {
    case MessageId:
        return message.id();
    case MessageText:
        return message.text();
    case MessagePublishedAt:
        return message.publishedAt();
    case MessageReceivedAt:
        return message.receivedAt();
    case MessageIsBotCommand:
        return message.isBotCommand();
    case MessageMarkedAsDeleted:
        return message.markedAsDeleted();

    case MessageIsDonateSimple:
        return message._flags.contains(ChatMessage::Flags::DonateSimple);
    case MessageIsDonateWithText:
        return message._flags.contains(ChatMessage::Flags::DonateWithText);
    case MessageIsDonateWithImage:
        return message._flags.contains(ChatMessage::Flags::DonateWithImage);

    case MessageIsPlatformGeneric:
        return message._flags.contains(ChatMessage::Flags::PlatformGeneric);

    case MessageIsYouTubeChatMembership:
        return message._flags.contains(ChatMessage::Flags::YouTubeChatMembership);

    case MessageIsTwitchAction:
        return message._flags.contains(ChatMessage::Flags::TwitchAction);

    case MessageBodyBackgroundForcedColor:
        return message.forcedColorRoleToQMLString(ChatMessage::ForcedColorRoles::BodyBackgroundForcedColorRole);

    case AuthorServiceType:
        return (int)(author ? author->getServiceType() : AbstractChatService::ServiceType::Unknown);
    case AuthorId:
        return author ? author->authorId() : QString();
    case AuthorPageUrl:
        return author ? author->pageUrl() : QUrl();
    case AuthorName:
        return author ? author->name() : QString();
    case AuthorNicknameColor:
        return author ? author->nicknameColor() : QColor();
    case AuthorAvatarUrl:
        return author ? author->avatarUrl() : QUrl();
    case AuthorCustomBadgeUrl:
        return author ? author->customBadgeUrl() : QUrl();
    case AuthorTwitchBadgesUrls:
        return author ? author->twitchBadgesUrls() : QStringList();
    case AuthorIsVerified:
        return author ? author->isVerified() : false;
    case AuthorIsChatOwner:
        return author ? author->isChatOwner() : false;
    case AuthorChatSponsor:
        return author ? author->isChatSponsor() : false;
    case AuthorChatModerator:
        return author ? author->isChatModerator() : false;
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