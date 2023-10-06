#include "chatservice.h"
#include "chathandler.h"

namespace
{

static const int ReconnectPeriod = 5 * 1000;
static const int FirstReconnectPeriod = 100;

}

const QString ChatService::UnknownBadge = "qrc:/resources/images/unknown-badge.png";

ChatService::ChatService(ChatHandler& manager_, QSettings& settings, const QString& settingsGroupPathParent, AxelChat::ServiceType serviceType_, const bool enabledThirdPartyEmotesDefault, QObject *parent)
    : QObject(parent)
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

    QTimer::singleShot(FirstReconnectPeriod, this, [this]()
    {
        if (!isEnabled() || isConnected())
        {
            return;
        }

        reconnect();
    });
}

QString ChatService::getServiceTypeId(const AxelChat::ServiceType serviceType)
{
    switch (serviceType)
    {
    case AxelChat::ServiceType::Unknown: return "unknown";
    case AxelChat::ServiceType::Software: return "software";

    case AxelChat::ServiceType::YouTube: return "youtube";
    case AxelChat::ServiceType::Twitch: return "twitch";
    case AxelChat::ServiceType::Trovo: return "trovo";
    case AxelChat::ServiceType::Kick: return "kick";
    case AxelChat::ServiceType::Odysee: return "odysee";
    case AxelChat::ServiceType::Rumble: return "rumble";
    case AxelChat::ServiceType::DLive: return "dlive";
    case AxelChat::ServiceType::GoodGame: return "goodgame";
    case AxelChat::ServiceType::VkPlayLive: return "vkplaylive";
    case AxelChat::ServiceType::VkVideo: return "vkvideo";
    case AxelChat::ServiceType::Wasd: return "wasd";
    case AxelChat::ServiceType::Telegram: return "telegram";
    case AxelChat::ServiceType::Discord: return "discord";

    case AxelChat::ServiceType::DonationAlerts: return "donationalerts";
    case AxelChat::ServiceType::DonatePayRu: return "donatepayru";
    }

    return "unknown";
}

QString ChatService::getName(const AxelChat::ServiceType serviceType)
{
    switch (serviceType)
    {
    case AxelChat::ServiceType::Unknown: return tr("Unknown");
    case AxelChat::ServiceType::Software: return QCoreApplication::applicationName();

    case AxelChat::ServiceType::YouTube: return tr("YouTube");
    case AxelChat::ServiceType::Twitch: return tr("Twitch");
    case AxelChat::ServiceType::Trovo: return tr("Trovo");
    case AxelChat::ServiceType::Kick: return tr("Kick");
    case AxelChat::ServiceType::Odysee: return tr("Odysee");
    case AxelChat::ServiceType::Rumble: return tr("Rumble");
    case AxelChat::ServiceType::DLive: return tr("DLive");
    case AxelChat::ServiceType::GoodGame: return tr("GoodGame");
    case AxelChat::ServiceType::VkPlayLive: return tr("VK Play Live");
    case AxelChat::ServiceType::VkVideo: return tr("VK Video");
    case AxelChat::ServiceType::Wasd: return tr("WASD");
    case AxelChat::ServiceType::Telegram: return tr("Telegram");
    case AxelChat::ServiceType::Discord: return tr("Discord");

    case AxelChat::ServiceType::DonationAlerts: return tr("DonationAlerts");
    case AxelChat::ServiceType::DonatePayRu: return tr("DonatePay.ru");
    }

    return tr("Unknown");
}

QUrl ChatService::getIconUrl(const AxelChat::ServiceType serviceType)
{
    switch (serviceType)
    {
    case AxelChat::ServiceType::Unknown: return QUrl();
    case AxelChat::ServiceType::Software: return QUrl("qrc:/resources/images/axelchat-icon.svg");

    case AxelChat::ServiceType::YouTube: return QUrl("qrc:/resources/images/youtube-icon.svg");
    case AxelChat::ServiceType::Twitch: return QUrl("qrc:/resources/images/twitch-icon.svg");
    case AxelChat::ServiceType::Trovo: return QUrl("qrc:/resources/images/trovo-icon.svg");
    case AxelChat::ServiceType::Kick: return QUrl("qrc:/resources/images/kick-icon.svg");
    case AxelChat::ServiceType::Odysee: return QUrl("qrc:/resources/images/odysee-icon.svg");
    case AxelChat::ServiceType::Rumble: return QUrl("qrc:/resources/images/rumble-icon.svg");
    case AxelChat::ServiceType::DLive: return QUrl("qrc:/resources/images/dlive-icon.svg");
    case AxelChat::ServiceType::GoodGame: return QUrl("qrc:/resources/images/goodgame-icon.svg");
    case AxelChat::ServiceType::VkPlayLive: return QUrl("qrc:/resources/images/vkplaylive-icon.svg");
    case AxelChat::ServiceType::VkVideo: return QUrl("qrc:/resources/images/vkvideo-icon.svg");
    case AxelChat::ServiceType::Wasd: return QUrl("qrc:/resources/images/wasd-icon.svg");
    case AxelChat::ServiceType::Telegram: return QUrl("qrc:/resources/images/telegram-icon.svg");
    case AxelChat::ServiceType::Discord: return QUrl("qrc:/resources/images/discord-icon.svg");

    case AxelChat::ServiceType::DonationAlerts: return QUrl("qrc:/resources/images/donationalerts-icon.svg");
    case AxelChat::ServiceType::DonatePayRu: return QUrl("qrc:/resources/images/donatepay-icon.svg");
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

AxelChat::ServiceType ChatService::getServiceType() const
{
    return serviceType;
}

void ChatService::reconnect()
{
    state = State();

    timerReconnect.stop();
    timerReconnect.start();

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

