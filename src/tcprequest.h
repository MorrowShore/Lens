#pragma once

#include <QUrl>
#include <QUrlQuery>
#include <QByteArray>

class TcpRequest
{
public:
    friend class TcpServer;

    const QByteArray& getMethod() const { return method; }
    const QUrl& getUrl() const { return url; }
    const QUrlQuery& getUrlQuery() const { return urlQuery; }

private:
    TcpRequest(const QByteArray& method_, const QUrl& url_)
        : method(method_)
        , url(url_)
        , urlQuery(url_)
    {

    }

    const QByteArray method;
    const QUrl url;
    const QUrlQuery urlQuery;
};
