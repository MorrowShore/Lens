#include "chatservice.h"
#include "tcpserver.h"

ChatService::ChatService(QSettings& settings, const QString& settingsGroupPath, AxelChat::ServiceType serviceType_, QObject *parent)
    : QObject(parent)
    , serviceType(serviceType_)
    , enabled(settings, settingsGroupPath + "/enabled", false)
    , stream(settings, settingsGroupPath + "/stream")
    , lastSavedMessageId(settings, settingsGroupPath + "/lastSavedMessageId")
{
    addUIElement(std::shared_ptr<UIElementBridge>(UIElementBridge::createLineEdit(&stream, tr("Stream"))));
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
    case AxelChat::ServiceType::GoodGame: return "goodgame";
    case AxelChat::ServiceType::VkPlayLive: return "vkplaylive";
    case AxelChat::ServiceType::VkVideo: return "vkvideo";
    case AxelChat::ServiceType::Telegram: return "telegram";
    case AxelChat::ServiceType::Discord: return "discord";
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
    case AxelChat::ServiceType::GoodGame: return tr("GoodGame");
    case AxelChat::ServiceType::VkPlayLive: return tr("VK Play Live");
    case AxelChat::ServiceType::VkVideo: return tr("VK Video");
    case AxelChat::ServiceType::Telegram: return tr("Telegram");
    case AxelChat::ServiceType::Discord: return tr("Discord");
    }

    return tr("Unknown");
}

QUrl ChatService::getIconUrl(const AxelChat::ServiceType serviceType)
{
    switch (serviceType)
    {
    case AxelChat::ServiceType::Unknown: return QUrl();
    case AxelChat::ServiceType::Software: return QUrl("qrc:/resources/images/axelchat-rounded.svg");

    case AxelChat::ServiceType::YouTube: return QUrl("qrc:/resources/images/youtube-icon.svg");
    case AxelChat::ServiceType::Twitch: return QUrl("qrc:/resources/images/twitch-icon.svg");
    case AxelChat::ServiceType::Trovo: return QUrl("qrc:/resources/images/trovo-icon.svg");
    case AxelChat::ServiceType::GoodGame: return QUrl("qrc:/resources/images/goodgame-icon.svg");
    case AxelChat::ServiceType::VkPlayLive: return QUrl("qrc:/resources/images/vkplaylive-icon.svg");
    case AxelChat::ServiceType::VkVideo: return QUrl("qrc:/resources/images/vkvideo-icon.svg");
    case AxelChat::ServiceType::Telegram: return QUrl("qrc:/resources/images/telegram-icon.svg");
    case AxelChat::ServiceType::Discord: return QUrl("qrc:/resources/images/discord-icon.svg");
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

TcpReply ChatService::processTcpRequest(const TcpRequest&)
{
    qCritical() << Q_FUNC_INFO << "not implemented for" << getServiceTypeId(getServiceType());

    return TcpReply::createTextHtmlOK(QString("Not implemented for %1 (%2)").arg(getName(), getServiceTypeId(getServiceType())));
}

AxelChat::ServiceType ChatService::getServiceType() const
{
    return serviceType;
}

void ChatService::reconnect()
{
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
    onUIElementChanged(uiElements[0]);
    emit stateChanged();
}

int ChatService::getViewersCount() const
{
    return state.viewersCount;
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

int ChatService::getUIElementBridgesCount() const
{
    return uiElements.count();
}

int ChatService::getUIElementBridgeType(const int index) const
{
    if (index < 0 || index >= uiElements.count())
    {
        qCritical() << Q_FUNC_INFO << "invalid index" << index;
        return -1;
    }

    return uiElements[index]->getTypeInt();
}

void ChatService::bindQmlItem(const int index, QQuickItem *item)
{
    if (index < 0 || index >= uiElements.count())
    {
        qCritical() << Q_FUNC_INFO << "invalid index" << index;
        return ;
    }

    uiElements[index]->bindQmlItem(item);
}

void ChatService::addUIElement(std::shared_ptr<UIElementBridge> element)
{
    if (!element)
    {
        qCritical() << Q_FUNC_INFO << "!element";
        return;
    }

    connect(element.get(), &UIElementBridge::valueChanged, this, [this, element]()
    {
        onUIElementChanged(element);
    });

    uiElements.append(element);
}

void ChatService::onUIElementChanged(const std::shared_ptr<UIElementBridge> element)
{
    if (!element)
    {
        qCritical() << Q_FUNC_INFO << "!element";
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
    else
    {
        onUiElementChangedImpl(element);
    }

    emit stateChanged();
}

std::shared_ptr<UIElementBridge> ChatService::getUIElementBridgeBySetting(const Setting<QString> &setting)
{
    for (const std::shared_ptr<UIElementBridge>& uiElement : qAsConst(uiElements))
    {
        if (uiElement && uiElement->getSettingString() == &setting)
        {
            return uiElement;
        }
    }

    return nullptr;
}

QJsonObject ChatService::getStateJson() const
{
    QJsonObject root;

    root.insert("type_id", getServiceTypeId(serviceType));
    root.insert("enabled", enabled.get());
    root.insert("viewers", getViewersCount());

    QString connectionStateType;

    switch (getConnectionStateType())
    {
    case ConnectionStateType::NotConnected:
        connectionStateType = "not_connected";
        break;
    case ConnectionStateType::Connecting:
        connectionStateType = "connecting";
        break;
    case ConnectionStateType::Connected:
        connectionStateType = "connected";
        break;
    }

    root.insert("connection_state", connectionStateType);

    return root;
}

QJsonObject ChatService::getStaticInfoJson() const
{
    QJsonObject root;

    root.insert("type_id", getServiceTypeId(serviceType));
    root.insert("name", getName());

    return root;
}

