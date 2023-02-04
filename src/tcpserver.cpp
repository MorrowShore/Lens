#include "tcpserver.h"
#include "chat_services/chatservice.h"
#include <QTcpSocket>
#include <QCoreApplication>

TcpServer::TcpServer(QList<ChatService*>& services_, QObject *parent)
    : QObject{parent}
    , services(services_)
{
    connect(&server, &QTcpServer::acceptError, this, [](QAbstractSocket::SocketError socketError)
    {
        qWarning() << Q_FUNC_INFO << "server error:" << socketError;
    });

    if (!server.listen(QHostAddress::Any, Port))
    {
        qCritical() << Q_FUNC_INFO << "server: failed to listen port";
    }

    connect(&server, &QTcpServer::newConnection, this, [this]()
    {
        QTcpSocket* socket = server.nextPendingConnection();
        if (!socket)
        {
            qWarning() << Q_FUNC_INFO << "socket is null";
            return;
        }

        connect(socket, &QTcpSocket::readyRead, this, [this, socket]()
        {
            const QByteArray data = socket->readAll();

            //qDebug("\ntcp server received:\n" + data + "\n");

            QByteArray method;
            QByteArray url;

            if (data.contains('\n'))
            {
                const QList<QByteArray> parts = data.left(data.indexOf('\n')).split(' ');

                if (parts.count() == 3)
                {
                    method = parts[0];
                    url = parts[1];
                }
                else
                {
                    socket->write(TcpReply::createTextHtmlOK("Bad header").getData());
                    qWarning() << Q_FUNC_INFO << "bad header";
                    return;
                }
            }

            if (checkAndRemovePrefix(url, "/chat_service/"))
            {
                if (url.contains('/'))
                {
                    const QByteArray serviceIdBa = url.left(url.indexOf('/'));
                    const QString serviceId = QString::fromUtf8(serviceIdBa);
                    url = url.mid(serviceIdBa.length());

                    bool found = false;
                    for (ChatService* service : qAsConst(services))
                    {
                        if (serviceId == ChatService::getServiceTypeId(service->getServiceType()))
                        {
                            socket->write(service->processTcpRequest(TcpRequest(method, QUrl(url))).getData());
                            found = true;
                        }
                    }

                    if (!found)
                    {
                        qWarning() << Q_FUNC_INFO << "service id" << serviceId << "not found";
                        socket->write(TcpReply::createTextHtmlOK(QString("Service id \"%1\" not found").arg(serviceId)).getData());
                    }
                }
                else
                {
                    qWarning() << Q_FUNC_INFO << "unknown chat_service url";
                    socket->write(TcpReply::createTextHtmlOK("Unknown chat_service url").getData());
                }
            }
            else if (url == "/favicon.ico")
            {
                // ignore
            }
            else
            {
                qWarning() << Q_FUNC_INFO << "unknown root url";
                socket->write(TcpReply::createTextHtmlOK("Unknown root url").getData());
            }
        });
    });
}

bool TcpServer::checkAndRemovePrefix(QByteArray &data, const QByteArray &prefix)
{
    if (!data.contains(prefix))
    {
        return false;
    }

    data = data.mid(prefix.length());
    return true;
}
