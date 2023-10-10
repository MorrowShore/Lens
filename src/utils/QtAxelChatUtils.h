#pragma once

#include <qwindowdefs.h>
#include <QString>
#include <functional>

class QtAxelChatUtils
{
public:
    static const QByteArray UserAgentNetworkHeaderName;
    static const QString DateTimeFileNameFormat;

    static QColor generateColor(const QString& hash, const QList<QColor>& colors);

private:
    QtAxelChatUtils(){}
};
