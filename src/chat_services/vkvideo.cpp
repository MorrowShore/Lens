#include "vkvideo.h"

VkVideo::VkVideo(QSettings &settings, const QString &settingsGroupPath, QNetworkAccessManager &network_, QObject *parent)
    : ChatService(settings, settingsGroupPath, AxelChat::ServiceType::VkVideo, parent)
    , network(network_)
{

}

ChatService::ConnectionStateType VkVideo::getConnectionStateType() const
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

QString VkVideo::getStateDescription() const
{
    switch (getConnectionStateType())
    {
    case ConnectionStateType::NotConnected:
        if (stream.get().isEmpty())
        {
            return tr("Broadcast not specified");
        }

        if (state.streamId.isEmpty())
        {
            return tr("The broadcast is not correct");
        }

        return tr("Not connected");

    case ConnectionStateType::Connecting:
        return tr("Connecting...");

    case ConnectionStateType::Connected:
        return tr("Successfully connected!");

    }

    return "<unknown_state>";
}

void VkVideo::reconnectImpl()
{

}
