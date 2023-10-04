#include "logmodel.h"
#include "logmessage.h"

namespace
{

static const int MaxSize  = 1000;
static const QHash<int, QByteArray> RoleNames = QHash<int, QByteArray>
{
    {(int)LogMessage::Role::Text,       "text"},
    {(int)LogMessage::Role::Type,       "type"},
    {(int)LogMessage::Role::File,       "file"},
    {(int)LogMessage::Role::Function,   "func"},
    {(int)LogMessage::Role::Line,       "line"},
    {(int)LogMessage::Role::Time,       "time"},
};
}

QHash<int, QByteArray> LogModel::roleNames() const
{
    return RoleNames;
}

int LogModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return messages.count();
}

QVariant LogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    const int row = index.row();
    if (row >= messages.size())
    {
        return QVariant();
    }

    return messages.at(row).getData((LogMessage::Role)role);
}

bool LogModel::removeRows(int firstRow, int count, const QModelIndex &parent)
{
    Q_UNUSED(parent)

    if (count > messages.count())
    {
        count = messages.count();
    }

    beginRemoveRows(QModelIndex(), firstRow, firstRow + count - 1);

    for (int i = 0; i < count; ++i)
    {
        messages.removeAt(firstRow);
    }

    endRemoveRows();

    return true;
}

void LogModel::addMessage(LogMessage &&message)
{
    beginInsertRows(QModelIndex(), messages.count(), messages.count());
    messages.append(std::move(message));
    endInsertRows();

    if (messages.count() >= MaxSize)
    {
        removeRows(0, messages.count() - MaxSize);
    }
}

void LogModel::clear()
{
    removeRows(0, rowCount(QModelIndex()));
}
