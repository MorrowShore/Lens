#include "chatservice.h"
#include "ChatManager.h"

namespace
{

static const int ReconnectPeriod = 5 * 1000;
static const int FirstReconnectPeriod = 100;

}

const QString ChatService::UnknownBadge = "qrc:/resources/images/unknown-badge.png";

ChatService::ChatService(ChatManager& manager_, QSettings& settings, const QString& settingsGroupPathParent, ChatServiceType serviceType_, const bool enabledThirdPartyEmotesDefault, QObject *parent)
    : Feature(manager_.backend, "ChatService:" + getServiceTypeId(serviceType_), parent)
    , serviceType(serviceType_)
    , settingsGroupPath(settingsGroupPathParent + "/" + getServiceTypeId(serviceType_))
    , manager(manager_)
    , stream(settings, getSettingsGroupPath() + "/stream")
    , enabled(settings, getSettingsGroupPath() + "/enabled", false)
    , enabledThirdPartyEmotes(settings, getSettingsGroupPath() + "/enabledThirdPartyEmotes", enabledThirdPartyEmotesDefault)
    , lastSavedMessageId(settings, getSettingsGroupPath() + "/lastSavedMessageId")
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);

    ui.addLineEdit(&stream, tr("Stream"));
    
    connect(&ui, &UIBridge::elementChanged, this, &ChatService::onUIElementChanged);

    QObject::connect(&timerReconnect, &QTimer::timeout, this, [this]()
    {
        if (!isEnabled() || isConnected())
        {
            return;
        }

        reconnect();
    });
    timerReconnect.start(ReconnectPeriod);

    QTimer::singleShot(FirstReconnectPeriod, this, [this]() { reconnect(); });
}

QString ChatService::getServiceTypeId(const ChatServiceType serviceType)
{
    switch (serviceType)
    {
    case ChatServiceType::Unknown:            return "unknown";
    case ChatServiceType::Software:           return "software";

    case ChatServiceType::YouTube:            return "youtube";
    case ChatServiceType::Twitch:             return "twitch";
    case ChatServiceType::Trovo:              return "trovo";
    case ChatServiceType::Kick:               return "kick";
    case ChatServiceType::Odysee:             return "odysee";
    case ChatServiceType::Rumble:             return "rumble";
    case ChatServiceType::DLive:              return "dlive";
    case ChatServiceType::NimoTV:             return "nimotv";
    case ChatServiceType::BigoLive:           return "bigolive";
    case ChatServiceType::GoodGame:           return "goodgame";
    case ChatServiceType::VkPlayLive:         return "vkplaylive";
    case ChatServiceType::VkVideo:            return "vkvideo";
    case ChatServiceType::Wasd:               return "wasd";
    case ChatServiceType::Rutube:               return "rutube";
    case ChatServiceType::Telegram:           return "telegram";
    case ChatServiceType::Discord:            return "discord";

    case ChatServiceType::DonationAlerts:     return "donationalerts";
    case ChatServiceType::DonatePayRu:        return "donatepayru";
    }

    return "unknown";
}

QString ChatService::getName(const ChatServiceType serviceType)
{
    switch (serviceType)
    {
    case ChatServiceType::Unknown:            return tr("Unknown");
    case ChatServiceType::Software:           return QCoreApplication::applicationName();

    case ChatServiceType::YouTube:            return tr("YouTube");
    case ChatServiceType::Twitch:             return tr("Twitch");
    case ChatServiceType::Trovo:              return tr("Trovo");
    case ChatServiceType::Kick:               return tr("Kick");
    case ChatServiceType::Odysee:             return tr("Odysee");
    case ChatServiceType::Rumble:             return tr("Rumble");
    case ChatServiceType::DLive:              return tr("DLive");
    case ChatServiceType::NimoTV:             return tr("Nimo TV");
    case ChatServiceType::BigoLive:           return tr("Bigo Live");
    case ChatServiceType::GoodGame:           return tr("GoodGame");
    case ChatServiceType::VkPlayLive:         return tr("VK Play Live");
    case ChatServiceType::VkVideo:            return tr("VK Video");
    case ChatServiceType::Wasd:               return tr("WASD");
    case ChatServiceType::Rutube:             return tr("Rutube");
    case ChatServiceType::Telegram:           return tr("Telegram");
    case ChatServiceType::Discord:            return tr("Discord");

    case ChatServiceType::DonationAlerts:     return tr("DonationAlerts");
    case ChatServiceType::DonatePayRu:        return tr("DonatePay.ru");
    }

    return tr("Unknown");
}

QUrl ChatService::getIconUrl(const ChatServiceType serviceType)
{
    switch (serviceType)
    {
    case ChatServiceType::Unknown:            return QUrl();
    case ChatServiceType::Software:           return QUrl("qrc:/resources/images/axelchat-icon.svg");

    case ChatServiceType::YouTube:            return QUrl("qrc:/resources/images/youtube-icon.svg");
    case ChatServiceType::Twitch:             return QUrl("qrc:/resources/images/twitch-icon.svg");
    case ChatServiceType::Trovo:              return QUrl("qrc:/resources/images/trovo-icon.svg");
    case ChatServiceType::Kick:               return QUrl("qrc:/resources/images/kick-icon.svg");
    case ChatServiceType::Odysee:             return QUrl("qrc:/resources/images/odysee-icon.svg");
    case ChatServiceType::Rumble:             return QUrl("qrc:/resources/images/rumble-icon.svg");
    case ChatServiceType::DLive:              return QUrl("qrc:/resources/images/dlive-icon.svg");
    case ChatServiceType::NimoTV:             return QUrl("qrc:/resources/images/nimotv-icon.svg");
    case ChatServiceType::BigoLive:           return QUrl("qrc:/resources/images/bigolive-icon.svg");
    case ChatServiceType::GoodGame:           return QUrl("qrc:/resources/images/goodgame-icon.svg");
    case ChatServiceType::VkPlayLive:         return QUrl("qrc:/resources/images/vkplaylive-icon.svg");
    case ChatServiceType::VkVideo:            return QUrl("qrc:/resources/images/vkvideo-icon.svg");
    case ChatServiceType::Wasd:               return QUrl("qrc:/resources/images/wasd-icon.svg");
    case ChatServiceType::Rutube:             return QUrl("qrc:/resources/images/rutube-icon.svg");
    case ChatServiceType::Telegram:           return QUrl("qrc:/resources/images/telegram-icon.svg");
    case ChatServiceType::Discord:            return QUrl("qrc:/resources/images/discord-icon.svg");

    case ChatServiceType::DonationAlerts:     return QUrl("qrc:/resources/images/donationalerts-icon.svg");
    case ChatServiceType::DonatePayRu:        return QUrl("qrc:/resources/images/donatepay-icon.svg");
    }

    return QUrl();
}

QUrl ChatService::getChatUrl() const
{
    return state.chatUrl;
}

QUrl ChatService::getControlPanelUrl() const
{
    return state.controlPanelUrl;
}

QUrl ChatService::getStreamUrl() const
{
    return state.streamUrl;
}

QString ChatService::getMainError() const
{
    return tr("Not connected");
}

TcpReply ChatService::processTcpRequest(const TcpRequest&)
{
    qCritical() << "not implemented for" << getServiceTypeId(getServiceType());

    return TcpReply::createTextHtmlOK(QString("Not implemented for %1 (%2)").arg(getName(), getServiceTypeId(getServiceType())));
}

ChatServiceType ChatService::getServiceType() const
{
    return serviceType;
}

void ChatService::reconnect()
{
    state = State();

    timerReconnect.stop();
    timerReconnect.start();

    if (enabled.get())
    {
        Feature::setAsUsed();
    }

    reconnectImpl();

    emit stateChanged();
}

bool ChatService::isEnabled() const
{
    return enabled.get();
}

void ChatService::setEnabled(const bool enabled_)
{
    enabled.set(enabled_);
    onUIElementChanged(ui.getElements().first());
    emit stateChanged();
}

bool ChatService::isEnabledThirdPartyEmotes() const
{
    return enabledThirdPartyEmotes.get();
}

void ChatService::setEnabledThirdPartyEmotes(const bool enabled)
{
    enabledThirdPartyEmotes.set(enabled);
    emit stateChanged();
}

int ChatService::getViewers() const
{
    return state.viewers;
}

const Setting<QString>& ChatService::getLastSavedMessageId() const
{
    return lastSavedMessageId;
}

void ChatService::setLastSavedMessageId(const QString& messageId)
{
    lastSavedMessageId.set(messageId);
}

const ChatService::State& ChatService::getState() const
{
    return state;
}

QString ChatService::getName() const
{
    return getName(serviceType);
}

QUrl ChatService::getIconUrl() const
{
    return getIconUrl(serviceType);
}

void ChatService::onUIElementChanged(const std::shared_ptr<UIBridgeElement> &element)
{
    if (!element)
    {
        qCritical() << "element is null";
        return;
    }

    Setting<QString>* settingString = element->getSettingString();
    Setting<bool>* settingBool = element->getSettingBool();

    if (settingString && *&settingString == &stream)
    {
        stream.set(stream.get().trimmed());
        reconnect();
    }
    else if (settingBool && *&settingBool == &enabled)
    {
        reconnect();
    }

    emit stateChanged();
}

QString ChatService::generateAuthorId(const QString &rawId) const
{
    return getServiceTypeId(getServiceType()) + "_" + rawId;
}

QString ChatService::generateMessageId(const QString &rawId_) const
{
    QString rawId = rawId_;
    if (rawId_.isEmpty())
    {
        rawId = QUuid::createUuid().toString(QUuid::Id128);
    }

    return getServiceTypeId(getServiceType()) + "_" + rawId;
}

void ChatService::setConnected(const bool connected)
{
    if (connected)
    {
        state.connected = true;
    }
    else
    {
        state = State();
    }

    emit stateChanged();
}

bool ChatService::isConnected() const
{
    return state.connected;
}

void ChatService::setViewers(const int count)
{
    state.viewers = count;
    emit stateChanged();

    if (!state.sendedState && state.connected && count != -1)
    {
        manager.backend.setService(*this);
        state.sendedState = true;
    }
}

std::shared_ptr<Author> ChatService::getServiceAuthor() const
{
    return Author::Builder(
                getServiceType(),
                getServiceTypeId(getServiceType()),
                getName())
                .setAvatar(getIconUrl().toString())
                .build();
}

QJsonObject ChatService::getStateJson() const
{
    QJsonObject root;

    root.insert("type_id", getServiceTypeId(serviceType));
    root.insert("enabled", isEnabled());
    root.insert("viewers", getViewers());

    QString connectionState;

    switch (getConnectionState())
    {
    case ConnectionState::NotConnected:
        connectionState = "not_connected";
        break;
    case ConnectionState::Connecting:
        connectionState = "connecting";
        break;
    case ConnectionState::Connected:
        connectionState = "connected";
        break;
    }

    root.insert("connection_state", connectionState);

    return root;
}

QJsonObject ChatService::getStaticInfoJson() const
{
    QJsonObject root;

    root.insert("type_id", getServiceTypeId(serviceType));
    root.insert("name", getName());

    return root;
}

