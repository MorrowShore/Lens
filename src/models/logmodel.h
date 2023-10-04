#pragma once

#include "logmessage.h"
#include <QAbstractListModel>

class LogModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit LogModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    void addMessage(LogMessage&& message);
    void clear();

private:
    QList<LogMessage> messages;
};
