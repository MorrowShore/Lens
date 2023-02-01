#pragma once

#include "utils.h"
#include "setting.h"
#include "chatservicestypes.h"
#include "models/author.h"
#include <QSettings>
#include <QObject>
#include <QQmlEngine>
#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <set>

class ChatHandler;
class Author;
class Message;

class ChatService : public QObject
{
    Q_OBJECT

public:
    Q_PROPERTY(bool                 enabled                      READ isEnabled     WRITE setEnabled     NOTIFY stateChanged)
    Q_PROPERTY(QUrl                 streamUrl                    READ getStreamUrl                       NOTIFY stateChanged)
    Q_PROPERTY(QUrl                 chatUrl                      READ getChatUrl                         NOTIFY stateChanged)
    Q_PROPERTY(QUrl                 controlPanelUrl              READ getControlPanelUrl                 NOTIFY stateChanged)

    Q_PROPERTY(ConnectionStateType  connectionStateType          READ getConnectionStateType             NOTIFY stateChanged)
    Q_PROPERTY(QString              stateDescription             READ getStateDescription                NOTIFY stateChanged)

    Q_PROPERTY(int                  viewersCount                 READ getViewersCount                    NOTIFY stateChanged)

    enum class ConnectionStateType
    {
        NotConnected = 10,
        Connecting = 20,
        Connected = 30
    };
    Q_ENUM(ConnectionStateType)

    struct State
    {
        bool connected = false;
        QString streamId;
        QUrl streamUrl;
        QUrl chatUrl;
        QUrl controlPanelUrl;
        int viewersCount = -1;
    };

    static QString getServiceTypeId(const AxelChat::ServiceType serviceType)
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

    static QString getName(const AxelChat::ServiceType serviceType)
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

    static QUrl getIconUrl(const AxelChat::ServiceType serviceType)
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

    explicit ChatService(QSettings& settings, const QString& settingsGroupPath, AxelChat::ServiceType serviceType_, QObject *parent = nullptr)
        : QObject(parent)
        , serviceType(serviceType_)
        , enabled(settings, settingsGroupPath + "/enabled", false)
        , stream(settings, settingsGroupPath + "/stream")
        , lastSavedMessageId(settings, settingsGroupPath + "/lastSavedMessageId")
    {
        parameters.append(Parameter::createSwitch(&enabled, tr("Enabled")));
        parameters.back().resetFlag(Parameter::Flag::Visible);

        parameters.append(Parameter::createLineEdit(&stream, tr("Stream")));
    }

    QUrl getChatUrl() const { return state.chatUrl; }
    QUrl getControlPanelUrl() const { return state.controlPanelUrl; }
    Q_INVOKABLE QUrl getStreamUrl() const { return state.streamUrl; }

    virtual ConnectionStateType getConnectionStateType() const = 0;
    virtual QString getStateDescription() const  = 0;
    AxelChat::ServiceType getServiceType() const { return serviceType; }

    void reconnect()
    {
        reconnectImpl();
        emit stateChanged();
    }

    int getViewersCount() const { return state.viewersCount; }

    bool isEnabled() const { return enabled.get(); }
    void setEnabled(const bool enabled_)
    {
        enabled.set(enabled_);
        onParameterChanged(parameters[0]);
        emit stateChanged();
    }

    const Setting<QString>& getLastSavedMessageId() const { return lastSavedMessageId; }
    void setLastSavedMessageId(const QString& messageId) { lastSavedMessageId.set(messageId); }

    QJsonObject toJson() const
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

    const State& getState() const { return state; }

    Q_INVOKABLE QString getName() const
    {
        return getName(serviceType);
    }

    Q_INVOKABLE QUrl getIconUrl() const
    {
        return getIconUrl(serviceType);
    }

    Q_INVOKABLE int getParametersCount() const { return parameters.count(); }

    Q_INVOKABLE QString getParameterName(int index) const
    {
        if (index < 0 || index >= parameters.count())
        {
            qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
            return QString();
        }

        return parameters[index].getName();
    }

    Q_INVOKABLE QString getParameterValueString(int index) const
    {
        if (index < 0 || index >= parameters.count())
        {
            qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
            return QString();
        }

        const Setting<QString>* setting = parameters[index].getSettingString();
        if (!setting)
        {
            qWarning() << Q_FUNC_INFO << ": parameter" << index << "setting string is null";
            return QString();
        }

        return setting->get();
    }

    Q_INVOKABLE void setParameterValueString(int index, const QString& value)
    {
        if (index < 0 || index >= parameters.count())
        {
            qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
            return;
        }

        Setting<QString>* setting = parameters[index].getSettingString();
        if (!setting)
        {
            qWarning() << Q_FUNC_INFO << ": parameter" << index << "setting string is null";
            return;
        }

        if (setting->set(value))
        {
            onParameterChanged(parameters[index]);
            emit stateChanged();
        }
    }

    Q_INVOKABLE bool getParameterValueBool(int index) const
    {
        if (index < 0 || index >= parameters.count())
        {
            qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
            return false;
        }

        const Setting<bool>* setting = parameters[index].getSettingBool();
        if (!setting)
        {
            qWarning() << Q_FUNC_INFO << ": parameter" << index << "setting bool is null";
            return false;
        }

        return setting->get();
    }

    Q_INVOKABLE void setParameterValueBool(int index, const bool value)
    {
        if (index < 0 || index >= parameters.count())
        {
            qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
            return;
        }

        Setting<bool>* setting = parameters[index].getSettingBool();
        if (!setting)
        {
            qWarning() << Q_FUNC_INFO << ": parameter" << index << "setting bool is null";
            return;
        }

        if (setting->set(value))
        {
            onParameterChanged(parameters[index]);
            emit stateChanged();
        }
    }

    Q_INVOKABLE int getParameterType(int index) const
    {
        if (index < 0 || index >= parameters.count())
        {
            qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
            return (int)Parameter::Type::Unknown;
        }

        return (int)parameters[index].getType();
    }

    Q_INVOKABLE bool isParameterHasFlag(int index, int flag) const
    {
        if (index < 0 || index >= parameters.count())
        {
            qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
            return false;
        }

        const std::set<Parameter::Flag>& flags = parameters[index].getFlags();

        return flags.find((Parameter::Flag)flag) != flags.end();
    }

    Q_INVOKABLE QString getParameterPlaceholder(int index) const
    {
        if (index < 0 || index >= parameters.count())
        {
            qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
            return QString();
        }

        return parameters[index].getPlaceholder();
    }

    Q_INVOKABLE bool executeParameterInvoke(int index)
    {
        if (index < 0 || index >= parameters.count())
        {
            qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
            return false;
        }

        return parameters[index].executeInvokeCallback();
    }

#ifdef QT_QUICK_LIB
    static void declareQml()
    {
        qmlRegisterUncreatableType<ChatService> ("AxelChat.ChatService", 1, 0, "ChatService", "Type cannot be created in QML");
    }
#endif

signals:
    void stateChanged();
    void readyRead(QList<Message>& messages, QList<Author>& authors);
    void connectedChanged(const bool connected, const QString& name);
    void authorDataUpdated(const QString& authorId, const QMap<Author::Role, QVariant>& values);

protected:
    struct Parameter {
        enum class Type
        {
            Unknown = -1,
            LineEdit = 10,
            Button = 20,
            Label = 21,
            Switch = 22,
        };

        enum class Flag
        {
            Visible = 1,
            PasswordEcho = 10,
        };

        static Parameter createLineEdit(Setting<QString>* setting, const QString& name, const QString& placeHolder = QString(), const std::set<Flag>& additionalFlags = std::set<Flag>())
        {
            Parameter parameter;

            parameter.type = Type::LineEdit;
            parameter.settingString = setting;
            parameter.name = name;
            parameter.placeHolder = placeHolder;
            parameter.flags.insert(additionalFlags.begin(), additionalFlags.end());

            return parameter;
        }

        static Parameter createButton(const QString& text, std::function<void(const QVariant& argument)> invokeCalback, const QVariant& invokeCallbackArgument_ = QVariant())
        {
            Parameter parameter;

            parameter.type = Type::Button;
            parameter.name = text;
            parameter.invokeCalback = invokeCalback;
            parameter.invokeCallbackArgument = invokeCallbackArgument_;

            return parameter;
        }

        static Parameter createLabel(const QString& text)
        {
            Parameter parameter;

            parameter.type = Type::Label;
            parameter.name = text;

            return parameter;
        }

        static Parameter createSwitch(Setting<bool>* settingBool, const QString& name)
        {
            Parameter parameter;

            parameter.type = Type::Switch;
            parameter.settingBool = settingBool;
            parameter.name = name;

            return parameter;
        }

        Setting<QString>* getSettingString() { return settingString; }
        const Setting<QString>* getSettingString() const { return settingString; }

        Setting<bool>* getSettingBool() { return settingBool; }
        const Setting<bool>* getSettingBool() const { return settingBool; }

        QString getName() const { return name; }
        void setName(const QString& name_) { name = name_; }
        Type getType() const { return type; }
        const std::set<Flag>& getFlags() const { return flags; }
        void setFlag(const Flag flag) { flags.insert(flag); }
        void resetFlag(const Flag flag) { flags.erase(flag); }

        void setPlaceholder(const QString& placeHolder_) { placeHolder = placeHolder_; }
        QString getPlaceholder() const { return placeHolder; }

        bool executeInvokeCallback()
        {
            if (!invokeCalback)
            {
                qCritical() << Q_FUNC_INFO << "invoke callback not setted";
                return false;
            }

            invokeCalback(invokeCallbackArgument);

            return true;
        }

    private:
        Parameter(){}

        Setting<QString>* settingString;
        Setting<bool>* settingBool;

        QString name;
        Type type;
        std::set<Flag> flags = { Flag::Visible };
        QString placeHolder;
        std::function<void(const QVariant& argument)> invokeCalback;
        QVariant invokeCallbackArgument;
    };

    virtual void reconnectImpl() = 0;

    void onParameterChanged(Parameter& parameter)
    {
        Setting<QString>* settingString = parameter.getSettingString();
        Setting<bool>* settingBool = parameter.getSettingBool();

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
            onParameterChangedImpl(parameter);
        }
    }

    virtual void onParameterChangedImpl(Parameter& parameter)
    {
        Q_UNUSED(parameter)
    }

    Parameter* getParameter(const Setting<QString>& setting)
    {
        for (Parameter& parameter : parameters)
        {
            if (parameter.getSettingString() == &setting)
            {
                return &parameter;
            }
        }

        return nullptr;
    }

    QList<Parameter> parameters;
    const AxelChat::ServiceType serviceType;
    State state;
    Setting<bool> enabled;
    Setting<QString> stream;
    Setting<QString> lastSavedMessageId;
};
