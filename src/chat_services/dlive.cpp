#include "dlive.h"

DLive::DLive(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager&, cweqt::Manager&, QObject *parent)
    : ChatService(settings, settingsGroupPathParent, AxelChat::ServiceType::DLive, false, parent)
{

}

ChatService::ConnectionStateType DLive::getConnectionStateType() const
{
    if (state.connected)
    {
        return ChatService::ConnectionStateType::Connected;
    }
    else if (enabled.get() && !state.streamId.isEmpty())
    {
        return ChatService::ConnectionStateType::Connecting;
    }

    return ChatService::ConnectionStateType::NotConnected;
}

QString DLive::getStateDescription() const
{
    switch (getConnectionStateType())
    {
    case ConnectionStateType::NotConnected:
        if (stream.get().isEmpty())
        {
            return tr("Channel not specified");
        }

        if (state.streamId.isEmpty())
        {
            return tr("The channel is not correct");
        }

        return tr("Not connected");

    case ConnectionStateType::Connecting:
        return tr("Connecting...");

    case ConnectionStateType::Connected:
        return tr("Successfully connected!");

    }

    return "<unknown_state>";
}

void DLive::reconnectImpl()
{
    state = State();
}
