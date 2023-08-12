#pragma once

#include "tcprequest.h"
#include "tcpreply.h"
#include <QTcpServer>

class ChatService;

class TcpServer : public QObject
{
    Q_OBJECT
public:
    explicit TcpServer(QList<std::shared_ptr<ChatService>>& services, QObject *parent = nullptr);

    static const int Port = 8356;

    static bool checkAndRemovePrefix(QByteArray& data, const QByteArray& prefix);

signals:

private:
    QList<std::shared_ptr<ChatService>>& services;

    QTcpServer server;
};

