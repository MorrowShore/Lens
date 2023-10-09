#include "QtStringUtils.h"
#include <QTimeZone>
#include <QDebug>

QString QtStringUtils::dateTimeToStringISO8601WithMsWithOffsetFromUtc(const QDateTime &dateTime)
{
    QString result = dateTime.toUTC().toString(Qt::DateFormat::ISODateWithMs);
    if (result.endsWith('Z', Qt::CaseSensitivity::CaseInsensitive))
    {
        result = result.left(result.length() - 1);
    }

    const int offsetMs = dateTime.offsetFromUtc();
    const bool belowZero = offsetMs < 0;

    const QTime offset = QTime::fromMSecsSinceStartOfDay(abs(offsetMs) * 1000);
    const QString offsetStr = QString("%1:%2").arg(offset.hour(), 2, 10, QChar('0')).arg(offset.minute(), 2, 10, QChar('0'));

    if (belowZero)
    {
        result += "-";
    }
    else
    {
        result += "+";
    }

    result += offsetStr;

    return result;
}
