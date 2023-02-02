#include "chatservice.h"

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
    case AxelChat::ServiceType::Telegram: return QUrl("qrc:/resources/images/telegram-icon.svg");
    case AxelChat::ServiceType::Discord: return QUrl("qrc:/resources/images/discord-icon.svg");
    }

    return QUrl();
}

ChatService::ChatService(QSettings& settings, const QString& settingsGroupPath, AxelChat::ServiceType serviceType_, QObject *parent)
    : QObject(parent)
    , serviceType(serviceType_)
    , enabled(settings, settingsGroupPath + "/enabled", false)
    , stream(settings, settingsGroupPath + "/stream")
    , lastSavedMessageId(settings, settingsGroupPath + "/lastSavedMessageId")
{
    parameters.append(std::shared_ptr<UIElementBridge>(UIElementBridge::createSwitch(&enabled, tr("Enabled"))));
    parameters.back()->resetFlag(UIElementBridge::Flag::Visible);

    parameters.append(std::shared_ptr<UIElementBridge>(UIElementBridge::createLineEdit(&stream, tr("Stream"))));
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
    onParameterChanged(*parameters[0]);
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

int ChatService::getParametersCount() const
{
    return parameters.count();
}

QString ChatService::getParameterName(int index) const
{
    if (index < 0 || index >= parameters.count())
    {
        qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
        return QString();
    }

    return parameters[index]->getName();
}

QString ChatService::getParameterValueString(int index) const
{
    if (index < 0 || index >= parameters.count())
    {
        qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
        return QString();
    }

    const Setting<QString>* setting = parameters[index]->getSettingString();
    if (!setting)
    {
        qWarning() << Q_FUNC_INFO << ": parameter" << index << "setting string is null";
        return QString();
    }

    return setting->get();
}

void ChatService::setParameterValueString(int index, const QString& value)
{
    if (index < 0 || index >= parameters.count())
    {
        qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
        return;
    }

    Setting<QString>* setting = parameters[index]->getSettingString();
    if (!setting)
    {
        qWarning() << Q_FUNC_INFO << ": parameter" << index << "setting string is null";
        return;
    }

    if (setting->set(value))
    {
        onParameterChanged(*parameters[index]);
        emit stateChanged();
    }
}

bool ChatService::getParameterValueBool(int index) const
{
    if (index < 0 || index >= parameters.count())
    {
        qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
        return false;
    }

    const Setting<bool>* setting = parameters[index]->getSettingBool();
    if (!setting)
    {
        qWarning() << Q_FUNC_INFO << ": parameter" << index << "setting bool is null";
        return false;
    }

    return setting->get();
}

void ChatService::setParameterValueBool(int index, const bool value)
{
    if (index < 0 || index >= parameters.count())
    {
        qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
        return;
    }

    Setting<bool>* setting = parameters[index]->getSettingBool();
    if (!setting)
    {
        qWarning() << Q_FUNC_INFO << ": parameter" << index << "setting bool is null";
        return;
    }

    if (setting->set(value))
    {
        onParameterChanged(*parameters[index]);
        emit stateChanged();
    }
}

int ChatService::getParameterType(int index) const
{
    if (index < 0 || index >= parameters.count())
    {
        qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
        return (int)UIElementBridge::Type::Unknown;
    }

    return (int)parameters[index]->getType();
}

bool ChatService::isParameterHasFlag(int index, int flag) const
{
    if (index < 0 || index >= parameters.count())
    {
        qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
        return false;
    }

    const std::set<UIElementBridge::Flag>& flags = parameters[index]->getFlags();

    return flags.find((UIElementBridge::Flag)flag) != flags.end();
}

QString ChatService::getParameterPlaceholder(int index) const
{
    if (index < 0 || index >= parameters.count())
    {
        qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
        return QString();
    }

    return parameters[index]->getPlaceholder();
}

bool ChatService::executeParameterInvoke(int index)
{
    if (index < 0 || index >= parameters.count())
    {
        qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
        return false;
    }

    return parameters[index]->executeInvokeCallback();
}

QJsonObject ChatService::toJson() const
{
    QJsonObject root;

    root.insert("type", getServiceTypeId(serviceType));
    root.insert("icon", getIconUrl(serviceType).toString().replace("qrc:/resources/", "./"));
    root.insert("viewersCount", getViewersCount());

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

    root.insert("connectionStateType", connectionStateType);

    return root;
}

