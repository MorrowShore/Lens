#include "QtAxelChatUtils.h"
#include <QDebug>
#include <QDir>
#include <QColor>

const QByteArray QtAxelChatUtils::UserAgentNetworkHeaderName = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/111.0.0.0 Safari/537.36";
const QString QtAxelChatUtils::DateTimeFileNameFormat = "yyyy-MM-ddThh-mm-ss.zzz";

QColor QtAxelChatUtils::generateColor(const QString& hash, const QList<QColor>& colors)
{
    if (colors.isEmpty())
    {
        qWarning() << "colors is empty";
        return QColor();
    }

    uint32_t sum = 0;

    for (const QChar& c : qAsConst(hash))
    {
        sum += c.unicode();
    }

    return colors.at(sum % colors.count());
}
