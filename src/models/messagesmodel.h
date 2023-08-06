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
    bool contains(const QString& id);
    void clear();
    const Author* getAuthor(const QString& authorId) const { return const_cast<MessagesModel&>(*this).getAuthor(authorId); }
    Author* getAuthor(const QString& authorId) { return _authorsById.value(authorId, nullptr); }
    void insertAuthor(const Author& author);
    void setAuthorValues(const AxelChat::ServiceType serviceType, const QString& authorId, const QMap<Author::Role, QVariant>& values);
    QList<std::shared_ptr<Message>> getLastMessages(int count) const;

private:
    QVariant dataByRole(const Message& message, int role) const;
    QModelIndex createIndexById(const QString& id) const;
    QModelIndex createIndexByData(const std::shared_ptr<Message>& message) const;

    QList<std::shared_ptr<Message>> _data;
    QHash<QString, std::shared_ptr<Message>> _dataById;
    QHash<uint64_t, std::shared_ptr<Message>> dataByRow;
    std::unordered_map<QString, uint64_t> rowById;

    QHash<QString, Author*> _authorsById;

    uint64_t lastRow = 0;
    uint64_t removedRows = 0;
};

