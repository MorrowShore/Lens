#pragma once

#include <QUrl>
#include <QByteArray>

class TcpReply
{
public:
    static TcpReply createTextHtmlOK(const QString& text)
    {
        return TcpReply(QString("<html><body><h1>%1</h1></body></html>").arg(text).toUtf8());
    }

    const QByteArray getData() const
    {
        return data;
    }

private:
    TcpReply(const QByteArray &html)
    {
        QByteArray& data_ = const_cast<QByteArray&>(data);

        data_ = "HTTP/1.1 200 OK\n"
                "Content-Type: text/html;charset=UTF-8\n"
                "Content-Length: ";

        data_ += QByteArray::number(html.length()) + "\n";

        data_ += "\n" + html;
    }

    const QByteArray data;
};
