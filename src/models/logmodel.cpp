#include "logmodel.h"
#include "logmessage.h"
#include <QColor>
#include <QBrush>

namespace
{

static const int MaxSize  = 1000;

static const QBrush DebugBrush      = QBrush(QColor(68, 186, 255));
static const QBrush WarningBrush    = QBrush(QColor(255, 255, 0));
static const QBrush CriticalBrush   = QBrush(QColor(255, 66, 66));
static const QBrush FatalBrush      = QBrush(QColor(225, 0, 0));
static const QBrush InfoBrush       = QBrush(QColor(76, 255, 120));

}

int LogModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return messages.count();
}

int LogModel::columnCount(const QModelIndex &) const
{
    return 3;
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

    const LogMessage& message = messages.at(row);

    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
        case 0: return message.text;
        case 1: return message.file + QString(":%1").arg(message.line);
        case 2: return message.function;
        }
    }
    else if (role == Qt::BackgroundRole)
    {
        switch (message.type)
        {
        case LogMessage::Type::Other:       return QVariant();
        case LogMessage::Type::Debug:       return DebugBrush;
        case LogMessage::Type::Warning:     return WarningBrush;
        case LogMessage::Type::Critical:    return CriticalBrush;
        case LogMessage::Type::Fatal:       return FatalBrush;
        case LogMessage::Type::Info:        return InfoBrush;
            break;
        }
    }

    return QVariant();
}

bool LogModel::removeRows(int firstRow, int count, const QModelIndex &)
{
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

QVariant LogModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
    {
        switch (section)
        {
        case 0:
            return tr("Message");
        case 1:
            return tr("File");
        case 2:
            return tr("Function");
        }
    }
    return QVariant();
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
