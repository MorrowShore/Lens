#pragma once

#include "logmessage.h"
#include <QAbstractListModel>

class LogModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit LogModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void addMessage(LogMessage&& message);
    void clear();

private:
    QList<LogMessage> messages;
};
