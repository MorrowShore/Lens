#pragma once

#include "chat_services/chatservice.hpp"
#include <QAbstractListModel>

class ChatMessagesModel : public QAbstractListModel
{
    Q_OBJECT
public:
    ChatMessagesModel(QObject *parent = 0) : QAbstractListModel(parent) {}

    enum ChatMessageRoles {
        MessageId = Qt::UserRole + 1,
        MessageText,
        MessagePublishedAt,
        MessageReceivedAt,
        MessageIsBotCommand,
        MessageMarkedAsDeleted,
        MessageCustomAuthorAvatarUrl,
        MessageCustomAuthorName,

        MessageIsDonateSimple,
        MessageIsDonateWithText,
        MessageIsDonateWithImage,

        MessageIsServiceMessage,

        MessageBodyBackgroundForcedColor,

        MessageIsYouTubeChatMembership,

        MessageIsTwitchAction,

        AuthorServiceType,
        AuthorId,
        AuthorPageUrl,
        AuthorName,
        AuthorNicknameColor,
        AuthorAvatarUrl,
        AuthorLeftBadgesUrls,
        AuthorRightBadgesUrls,

        AuthorIsVerified,
        AuthorIsChatOwner,
        AuthorIsSponsor,
        AuthorIsModerator
    };

    QHash<int, QByteArray> roleNames() const override {
        return _roleNames;
    }

    void append(ChatMessage&& message);
    bool contains(const QString& id);
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QVariant dataByRole(const ChatMessage& message, int role) const;
    QVariant dataByNumId(const uint64_t &idNum, int role);
    bool removeRows(int position, int rows, const QModelIndex &parent = QModelIndex()) override;
    void clear();
    uint64_t lastIdNum() const;
    QModelIndex createIndexByPtr(QVariant* data) const;
    int getRow(QVariant* data);
    void applyAvatar(const QString& channelId, const QUrl& url);

    const ChatAuthor& softwareAuthor();

    const ChatAuthor* getAuthor(const QString& authorId) const { return _authorsById.value(authorId, nullptr); }
    ChatAuthor* getAuthor(const QString& authorId) { return _authorsById.value(authorId, nullptr); }
    void addAuthor(ChatAuthor* author);

private:
    static const QHash<int, QByteArray> _roleNames;
    QList<QVariant*> _data;//*data
    QHash<QString, QVariant*> _dataById;//message_id, *data
    QHash<QVariant*, QString> _idByData;//*data, message_id
    QHash<uint64_t, QVariant*> _dataByIdNum;//idNum, *data
    QHash<QVariant*, uint64_t> _idNumByData;//*data, idNum
    QHash<QString, QSet<uint64_t>> _needUpdateAvatarMessages;//channel_id, QSet<idNum>

    QHash<QString, ChatAuthor*> _authorsById;

    const int _maxSize  = 1000;

    uint64_t _lastIdNum = 0;
    uint64_t _removedRows = 0;
};

