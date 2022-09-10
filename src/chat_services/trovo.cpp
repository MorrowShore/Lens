#include "trovo.h"

Trovo::Trovo(QSettings &settings_, const QString &settingsGroupPath, QNetworkAccessManager &, QObject *parent)
    : ChatService(settings_, settingsGroupPath, AxelChat::ServiceType::Trovo, parent)
{

}

ChatService::ConnectionStateType Trovo::getConnectionStateType() const
{
    if (state.connected)
    {
        return ChatService::ConnectionStateType::Connected;
    }
    else if (!state.streamId.isEmpty())
    {
        return ChatService::ConnectionStateType::Connecting;
    }

    return ChatService::ConnectionStateType::NotConnected;
}

QString Trovo::getStateDescription() const
{
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

void Trovo::reconnect()
{

}
