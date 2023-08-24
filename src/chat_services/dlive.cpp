#include "dlive.h"

DLive::DLive(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager&, cweqt::Manager&, QObject *parent)
    : ChatService(settings, settingsGroupPathParent, AxelChat::ServiceType::DLive, false, parent)
{
    ui.findBySetting(stream)->setItemProperty("name", tr("Channel"));
    ui.findBySetting(stream)->setItemProperty("placeholderText", tr("Link or channel name..."));

    reconnect();
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

void DLive::reconnectImpl()
{
    state = State();

    state.controlPanelUrl = "https://dlive.tv/s/dashboard#0";

    state.streamId = extractChannelName(stream.get());

    if (state.streamId.isEmpty())
    {
        return;
    }

    state.streamUrl = "https://dlive.tv/" + state.streamId;

    if (!enabled.get())
    {
        return;
    }
}

QString DLive::extractChannelName(const QString &stream)
{
    QRegExp rx;

    const QString simpleUserSpecifiedUserChannel = AxelChat::simplifyUrl(stream);
    rx = QRegExp("^dlive.tv/([^/]*)$", Qt::CaseInsensitive);
    if (rx.indexIn(simpleUserSpecifiedUserChannel) != -1)
    {
        return rx.cap(1);
    }

    rx = QRegExp("^[a-zA-Z0-9_]+$", Qt::CaseInsensitive);
    if (rx.indexIn(stream) != -1)
    {
        return stream;
    }

    return QString();
}
