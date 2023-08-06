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
    QModelIndex createIndexByPtr(const std::shared_ptr<QVariant>& data) const;
    int getRow(const std::shared_ptr<QVariant>& data);

    const Author* getAuthor(const QString& authorId) const { return _authorsById.value(authorId, nullptr); }
    Author* getAuthor(const QString& authorId) { return _authorsById.value(authorId, nullptr); }
    void insertAuthor(const Author& author);
    void setAuthorValues(const AxelChat::ServiceType serviceType, const QString& authorId, const QMap<Author::Role, QVariant>& values);
    QList<Message> getLastMessages(int count) const;

private:
    QList<std::shared_ptr<QVariant>> _data;
    QHash<QString, std::shared_ptr<QVariant>> _dataById;
    QHash<uint64_t, std::shared_ptr<QVariant>> _dataByIdNum;
    std::unordered_map<std::shared_ptr<QVariant>, uint64_t> _idNumByData;

    QHash<QString, Author*> _authorsById;

    const int _maxSize  = 10; // 1000

    uint64_t _lastIdNum = 0;
    uint64_t _removedRows = 0;
};

