#include "discord.h"
#include "apikeys.h"
#include "string_obfuscator/obfuscator.hpp"
#include <QDesktopServices>
#include <QTcpSocket>

namespace
{

static const QString ClientID = OBFUSCATE(DISCORD_CLIENT_ID);
static const int ServerPort = 8356;

static const QByteArray makeHttpResponse(const QByteArray& data)
{
    return QString("HTTP/1.1 200 OK\n"
           "Content-Length: %1\n"
           "Content-Type: text/html;charset=UTF-8\n"
           "\n").arg(data.length()).toUtf8() + data;
}

}

Discord::Discord(QSettings &settings_, const QString &settingsGroupPath, QNetworkAccessManager &network_, QObject *parent)
    : ChatService(settings_, settingsGroupPath, AxelChat::ServiceType::Discord, parent)
    , settings(settings_)
    , network(network_)
    , oauthToken(settings_, settingsGroupPath + "/oauth_token")
{
    parameters.append(Parameter::createButton(tr("Login"), [](const QVariant&)
    {
        QDesktopServices::openUrl(QUrl(QString("https://discord.com/api/oauth2/authorize?client_id=%1&redirect_uri=http%3A%2F%2Flocalhost%3A8356&response_type=code&scope=messages.read").arg(ClientID)));
    }));

    parameters.append(Parameter::createButton(tr("Logout"), [this](const QVariant&)
    {
        oauthToken.set(QString());
        emit stateChanged();
    }));

    connect(&server, &QTcpServer::acceptError, this, [](QAbstractSocket::SocketError socketError)
    {
        qWarning() << Q_FUNC_INFO << "server error:" << socketError;
    });

    if (!server.listen(QHostAddress::Any, ServerPort))
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
            if (data.startsWith("GET /?code="))
            {
                static const QRegExp rx("GET /\\?code=([a-zA-Z0-9_\\-]+)");
                if (rx.indexIn(data) != -1)
                {
                    const QString code = rx.cap(1).trimmed();

                    oauthToken.set(code);

                    if (code.isEmpty())
                    {
                        qWarning() << Q_FUNC_INFO << "socket code is mepty";

                        socket->write(makeHttpResponse(QString("<html><body><h1>%1</h1></body></html>").arg(
                                                           tr("Error, please try again or report the problem to the developer")).toUtf8()));
                    }
                    else
                    {
                        socket->write(makeHttpResponse(QString("<html><body><h1>%1</h1></body></html>").arg(
                                                           tr("Now you can close the page and return to %1").arg(QCoreApplication::applicationName())).toUtf8()));
                    }

                    emit stateChanged();
                }
            }
            else
            {
                qWarning() << Q_FUNC_INFO << "socket unknown received data";
            }
        });
    });
}

ChatService::ConnectionStateType Discord::getConnectionStateType() const
{
    if (state.connected)
    {
        return ChatService::ConnectionStateType::Connected;
    }
    else if (!oauthToken.get().isEmpty() && !state.streamId.isEmpty())
    {
        return ChatService::ConnectionStateType::Connecting;
    }

    return ChatService::ConnectionStateType::NotConnected;
}

QString Discord::getStateDescription() const
{
    if (oauthToken.get().isEmpty())
    {
        return tr("Not authorized");
    }

    switch (getConnectionStateType())
    {
    case ConnectionStateType::NotConnected:
        if (state.streamId.isEmpty())
        {
            return tr("Channel not specified");
        }

        return tr("Not connected");

    case ConnectionStateType::Connecting:
        return tr("Connecting...");

    case ConnectionStateType::Connected:
        return tr("Successfully connected!");

    }

    return "<unknown_state>";
}

void Discord::reconnectImpl()
{

}
