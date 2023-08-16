#include "donatepay.h"

DonatePay::DonatePay(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network_, cweqt::Manager& web, QObject *parent)
    : ChatService(settings, settingsGroupPathParent, AxelChat::ServiceType::DonatePay, false, parent)
    , network(network_)
{
    getUIElementBridgeBySetting(stream)->setItemProperty("visible", false);
}

ChatService::ConnectionStateType DonatePay::getConnectionStateType() const
{
    if (state.connected)
    {
        return ChatService::ConnectionStateType::Connected;
    }
    else if (enabled.get())
    {
        return ChatService::ConnectionStateType::Connecting;
    }

    return ChatService::ConnectionStateType::NotConnected;
}

QString DonatePay::getStateDescription() const
{
    switch (getConnectionStateType())
    {
    case ConnectionStateType::NotConnected:
        return tr("Not connected");

    case ConnectionStateType::Connecting:
        return tr("Connecting...");

    case ConnectionStateType::Connected:
        return tr("Successfully connected!");
    }

    return "<unknown_state>";
}

void DonatePay::reconnectImpl()
{

}
