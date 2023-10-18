#include "Rutube.h"

Rutube::Rutube(ChatManager &manager, QSettings &settings, const QString &settingsGroupPathParent, QNetworkAccessManager &network_, cweqt::Manager &, QObject *parent)
    : ChatService(manager, settings, settingsGroupPathParent, AxelChat::ServiceType::Rutube, false, parent)
    , network(network_)
{
    ui.findBySetting(stream)->setItemProperty("placeholderText", tr("Broadcast link..."));
}

ChatService::ConnectionState Rutube::getConnectionState() const
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

QString Rutube::getMainError() const
{
    if (stream.get().isEmpty())
    {
        return tr("Broadcast not specified");
    }

    if (state.streamId.isEmpty())
    {
        return tr("The broadcast is not correct");
    }

    return tr("Not connected");
}

void Rutube::reconnectImpl()
{
    info = Info();

    state.streamId = "3b4b0a568b91ef8f990365b46e10d549"; // TODO

    if (!state.streamId.isEmpty())
    {
        state.streamUrl = QUrl(QString("https://rutube.ru/video/%1/").arg(state.streamId));

        state.controlPanelUrl = QUrl(QString("https://rutube.ru/livestreaming/%1/").arg(state.streamId));
    }

    if (!isEnabled())
    {
        return;
    }


}
