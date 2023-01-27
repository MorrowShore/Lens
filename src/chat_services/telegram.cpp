#include "telegram.h"
#include <QDesktopServices>

Telegram::Telegram(QSettings& settings_, const QString& settingsGroupPath, QNetworkAccessManager& network_, QObject *parent)
    : ChatService(settings_, settingsGroupPath, AxelChat::ServiceType::Telegram, parent)
    , settings(settings_)
    , network(network_)
{
    getParameter(stream)->setName(tr("Bot token"));
    getParameter(stream)->setPlaceholder("0000000000:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    getParameter(stream)->setFlag(Parameter::Flag::PasswordEcho);

    parameters.append(Parameter(new Setting<QString>(settings, QString()), tr("1. Create a bot with @BotFather\n2. Add the bot to the desired group/channel and give it admin rights\n3. Specify your bot token above"), Parameter::Type::Label));

    parameters.append(Parameter(new Setting<QString>(settings, QString()), tr("Create bot with @BotFather"), Parameter::Type::Button, {}, [](const QVariant&)
    {
        QDesktopServices::openUrl(QUrl("https://telegram.me/botfather"));
    }));

    reconnect();
}

void Telegram::reconnect()
{

}

ChatService::ConnectionStateType Telegram::getConnectionStateType() const
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

QString Telegram::getStateDescription() const
{
    switch (getConnectionStateType())
    {
    case ConnectionStateType::NotConnected:
        if (stream.get().isEmpty())
        {
            return tr("Bot token not specified");
        }

        if (state.streamId.isEmpty())
        {
            return tr("Bot token is not correct");
        }

        return tr("Not connected");

    case ConnectionStateType::Connecting:
        return tr("Connecting...");

    case ConnectionStateType::Connected:
        return tr("Successfully connected!");

    }

    return "<unknown_state>";
}
