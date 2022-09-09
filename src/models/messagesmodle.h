#pragma once

#include "models/message.h"
#include "models/author.h"
#include <QAbstractListModel>

class MessagesModel : public QAbstractListModel
{
    Q_OBJECT
public:
    MessagesModel(QObject *parent = 0) : QAbstractListModel(parent) {}

    QHash<int, QByteArray> roleNames() const override {
        return _roleNames;
    }

    void append(Message&& message);
    bool contains(const QString& id);
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QVariant dataByRole(const Message& message, int role) const;
    QVariant dataByNumId(const uint64_t &idNum, int role);
    bool removeRows(int position, int rows, const QModelIndex &parent = QModelIndex()) override;
    void clear();
    uint64_t lastIdNum() const;
    QModelIndex createIndexByPtr(QVariant* data) const;
    int getRow(QVariant* data);
    void setAuthorData(const QString& authorId, const QMap<Author::Role, QVariant>& values);

    const Author* getAuthor(const QString& authorId) const { return _authorsById.value(authorId, nullptr); }
    Author* getAuthor(const QString& authorId) { return _authorsById.value(authorId, nullptr); }
    void insertAuthor(const Author& author);

private:
    static const QHash<int, QByteArray> _roleNames;
    QList<QVariant*> _data;//*data
    QHash<QString, QVariant*> _dataById;//message_id, *data
    QHash<QVariant*, QString> _idByData;//*data, message_id
    QHash<uint64_t, QVariant*> _dataByIdNum;//idNum, *data
    QHash<QVariant*, uint64_t> _idNumByData;//*data, idNum

    QHash<QString, Author*> _authorsById;

    const int _maxSize  = 1000;

    uint64_t _lastIdNum = 0;
    uint64_t _removedRows = 0;
};

