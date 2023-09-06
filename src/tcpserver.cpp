#include "tcpserver.h"
#include "chat_services/chatservice.h"
#include <QTcpSocket>
#include <QCoreApplication>

TcpServer::TcpServer(QList<std::shared_ptr<ChatService>>& services_, QObject *parent)
    : QObject{parent}
    , services(services_)
{
    connect(&server, &QTcpServer::acceptError, this, [](QAbstractSocket::SocketError socketError)
    {
        qWarning() << "server error:" << socketError;
    });

    if (!server.listen(QHostAddress::Any, Port))
    {
        qCritical() << "server: failed to listen port";
    }

    connect(&server, &QTcpServer::newConnection, this, [this]()
    {
        QTcpSocket* socket = server.nextPendingConnection();
        if (!socket)
        {
            qWarning() << "socket is null";
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
                    socket->write(TcpReply::createTextHtmlError("Bad header").getData());
                    qWarning() << "bad header";
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
                    for (const std::shared_ptr<ChatService>& service : qAsConst(services))
                    {
                        if (!service)
                        {
                            qWarning() << "servies is null";
                            continue;
                        }

                        if (serviceId == ChatService::getServiceTypeId(service->getServiceType()))
                        {
                            socket->write(service->processTcpRequest(TcpRequest(method, QUrl(url))).getData());
                            found = true;
                        }
                    }

                    if (!found)
                    {
                        qWarning() << "service id" << serviceId << "not found";
                        socket->write(TcpReply::createTextHtmlError(QString("Service id \"%1\" not found").arg(serviceId)).getData());
                    }
                }
                else
                {
                    qWarning() << "unknown chat_service url";
                    socket->write(TcpReply::createTextHtmlError("Unknown chat_service url").getData());
                }
            }
            else if (url == "/favicon.ico")
            {
                // ignore
            }
            else
            {
                qWarning() << "unknown root url";
                socket->write(TcpReply::createTextHtmlError("Unknown root url").getData());
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
