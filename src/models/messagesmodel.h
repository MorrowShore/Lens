#pragma once

#include "models/message.h"
#include "models/author.h"
#include <QAbstractListModel>
#include <unordered_map>

class MessagesModel : public QAbstractListModel
{
    Q_OBJECT
public:
    MessagesModel(QObject *parent = 0) : QAbstractListModel(parent) {}

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    void append(const std::shared_ptr<Message>& message);
    void insertAuthor(const std::shared_ptr<Author>& author);
    bool contains(const QString& id);
    void clear();
    const std::shared_ptr<Author> getAuthor(const QString& authorId) const { return const_cast<MessagesModel&>(*this).getAuthor(authorId); }
    std::shared_ptr<Author> getAuthor(const QString& authorId) { return _authorsById.value(authorId, nullptr); }
    void setAuthorValues(const AxelChat::ServiceType serviceType, const QString& authorId, const QMap<Author::Role, QVariant>& values);
    QList<std::shared_ptr<Message>> getLastMessages(int count) const;

private:
    QVariant dataByRole(const std::shared_ptr<Message>& message, int role) const;
    QModelIndex createIndexByExistingMessage(const QString& id) const;

    QList<std::shared_ptr<Message>> messages;
    QHash<QString, std::shared_ptr<Message>> messagesById;
    QHash<uint64_t, std::shared_ptr<Message>> messagesByRow;

    QHash<QString, std::shared_ptr<Author>> _authorsById;

    uint64_t lastRow = 0;
    uint64_t removedRows = 0;
};

