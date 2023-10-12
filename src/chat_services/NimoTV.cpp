#include "NimoTV.h"

NimoTV::NimoTV(ChatManager &manager, QSettings &settings, const QString &settingsGroupPathParent, QNetworkAccessManager &network, cweqt::Manager &web, QObject *parent)
    : ChatService(manager, settings, settingsGroupPathParent, AxelChat::ServiceType::NimoTV, false, parent)
{
    ui.findBySetting(stream)->setItemProperty("name", tr("Channel"));
    ui.findBySetting(stream)->setItemProperty("placeholderText", tr("Link or channel name..."));
}

ChatService::ConnectionState NimoTV::getConnectionState() const
{
    if (isConnected())
    {
        return ChatService::ConnectionState::Connected;
    }
    else if (isEnabled() && !state.streamId.isEmpty())
    {
        return ChatService::ConnectionState::Connecting;
    }

    return ChatService::ConnectionState::NotConnected;
}

QString NimoTV::getMainError() const
{
    if (state.streamId.isEmpty())
    {
        return tr("Channel not specified");
    }

    return tr("Not connected");
}

void NimoTV::reconnectImpl()
{

}
