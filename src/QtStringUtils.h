#pragma once

#include <QString>
#include <QDateTime>

class QtStringUtils
{
public:
    static QString dateTimeToStringISO8601WithMsWithOffsetFromUtc(const QDateTime& dateTime);

private:
    QtStringUtils(){}
};
