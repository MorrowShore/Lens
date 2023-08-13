#pragma once

#include <QString>

class YouTubeUtils
{
public:
    YouTubeUtils() = delete;

    static QString extractBroadcastId(const QString& link);
};
