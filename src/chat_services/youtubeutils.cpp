#include "youtubeutils.h"
#include "utils.h"
#include <QUrlQuery>

QString YouTubeUtils::extractBroadcastId(const QString &link)
{
    const QString simpleUrl = AxelChat::simplifyUrl(link);

    const QUrlQuery& urlQuery = QUrlQuery(QUrl(link).query());

    const QString& vParameter = urlQuery.queryItemValue("v");

    QString broadcastId;
    QRegExp rx;

    //youtu.be/rSjMyeISW7w
    if (broadcastId.isEmpty())
    {
        rx = QRegExp("^youtu.be/([^/]*)$", Qt::CaseInsensitive);
        if (rx.indexIn(simpleUrl) != -1)
        {
            broadcastId = rx.cap(1);
        }
    }

    //studio.youtube.com/video/rSjMyeISW7w/livestreaming
    if (broadcastId.isEmpty())
    {
        rx = QRegExp("^(studio\\.)?youtube.com/video/([^/]*)/livestreaming$", Qt::CaseInsensitive);
        if (rx.indexIn(simpleUrl) != -1)
        {
            broadcastId = rx.cap(2);
        }
    }

    //youtube.com/video/rSjMyeISW7w
    //studio.youtube.com/video/rSjMyeISW7w
    if (broadcastId.isEmpty())
    {
        rx = QRegExp("^(studio\\.)?youtube.com/video/([^/]*)$", Qt::CaseInsensitive);
        if (rx.indexIn(simpleUrl) != -1)
        {
            broadcastId = rx.cap(2);
        }
    }

    //youtube.com/watch/rSjMyeISW7w
    //studio.youtube.com/watch/rSjMyeISW7w
    if (broadcastId.isEmpty())
    {
        rx = QRegExp("^(studio\\.)?youtube.com/watch/([^/]*)$", Qt::CaseInsensitive);
        if (rx.indexIn(simpleUrl) != -1)
        {
            broadcastId = rx.cap(2);
        }
    }

    //youtube.com/live_chat?is_popout=1&v=rSjMyeISW7w
    //studio.youtube.com/live_chat?v=rSjMyeISW7w&is_popout=1
    if (broadcastId.isEmpty())
    {
        rx = QRegExp("^(studio\\.)?youtube.com/live_chat$", Qt::CaseInsensitive);
        if (rx.indexIn(simpleUrl) != -1 && !vParameter.isEmpty())
        {
            broadcastId = vParameter;
        }
    }

    //https://www.youtube.com/watch?v=rSjMyeISW7w&feature=youtu.be
    //https://www.youtube.com/watch?v=rSjMyeISW7w
    if (broadcastId.isEmpty())
    {
        rx = QRegExp("^(studio\\.)?youtube.com/watch$", Qt::CaseInsensitive);
        if (rx.indexIn(simpleUrl) != -1 && !vParameter.isEmpty())
        {
            broadcastId = vParameter;
        }
    }

    //Broadcast id only
    //rSjMyeISW7w
    if (broadcastId.isEmpty())
    {
        rx = QRegExp("^[a-zA-Z0-9_\\-]+$", Qt::CaseInsensitive);
        if (rx.indexIn(link) != -1)
        {
            broadcastId = link;
        }
    }

    return broadcastId;
}
