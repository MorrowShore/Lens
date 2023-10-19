#pragma once

#include "models/message.h"
#include "models/author.h"
#include <QAbstractListModel>
#include <unordered_map>

class MessagesModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit MessagesModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    void addMessage(const std::shared_ptr<Message>& message);
    void addAuthor(const std::shared_ptr<Author>& author);
    bool contains(const QString& id);
    void clear();
    const std::shared_ptr<Author> getAuthor(const QString& authorId) const { return const_cast<MessagesModel&>(*this).getAuthor(authorId); }
    std::shared_ptr<Author> getAuthor(const QString& authorId) { return authors.value(authorId, nullptr); }
    void setAuthorValues(const ChatServiceType serviceType, const QString& authorId, const QMap<Author::Role, QVariant>& values);
    QList<std::shared_ptr<Message>> getLastMessages(int count) const;

private:
    QVariant dataByRole(const std::shared_ptr<Message>& message, int role) const;
    QModelIndex createIndexByExistingMessageId(const QString& id) const;

    QList<std::shared_ptr<Message>> messages;
    QHash<QString, std::shared_ptr<Message>> messagesById;

    QHash<QString, std::shared_ptr<Author>> authors;

    uint64_t lastRow = 0;
    uint64_t removedRows = 0;
};
