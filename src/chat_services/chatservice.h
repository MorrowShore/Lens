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
    Q_PROPERTY(QUrl                 streamUrl                    READ getStreamUrl                         NOTIFY stateChanged)
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
        }

        return QUrl();
    }

    explicit ChatService(QSettings& settings, const QString& settingsGroupPath, AxelChat::ServiceType serviceType_, QObject *parent = nullptr)
        : QObject(parent)
        , serviceType(serviceType_)
        , stream(settings, settingsGroupPath + "/stream")
        , lastSavedMessageId(settings, settingsGroupPath + "/lastSavedMessageId")
    {
        parameters.append(Parameter::createLineEdit(&stream, tr("Stream")));
    }

    QUrl getChatUrl() const { return state.chatUrl; }
    QUrl getControlPanelUrl() const { return state.controlPanelUrl; }
    Q_INVOKABLE QUrl getStreamUrl() const { return state.streamUrl; }

    virtual ConnectionStateType getConnectionStateType() const = 0;
    virtual QString getStateDescription() const  = 0;
    AxelChat::ServiceType getServiceType() const { return serviceType; }

    int getViewersCount() const { return state.viewersCount; }

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

    virtual void reconnect() = 0;

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
        if (index >= 0 && index < parameters.count())
        {
            return parameters[index].getName();
        }

        qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";

        return QString();
    }

    Q_INVOKABLE QString getParameterValue(int index) const
    {
        if (index >= 0 && index < parameters.count())
        {
            const auto* setting = parameters[index].getSetting();
            if (setting)
            {
                return setting->get();
            }
            else
            {
                qWarning() << Q_FUNC_INFO << ": parameter" << index << "setting is null";
                return QString();
            }
        }

        qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
        return QString();
    }

    Q_INVOKABLE int getParameterType(int index) const
    {
        if (index >= 0 && index < parameters.count())
        {
            return (int)parameters[index].getType();
        }

        qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";

        return (int)Parameter::Type::Unknown;
    }

    Q_INVOKABLE bool isParameterHasFlag(int index, int flag) const
    {
        if (index >= 0 && index < parameters.count())
        {
            const std::set<Parameter::Flag>& flags = parameters[index].getFlags();

            return flags.find((Parameter::Flag)flag) != flags.end();
        }

        qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";

        return false;
    }

    Q_INVOKABLE void setParameterValue(int index, const QString& value)
    {
        if (index >= 0 && index < parameters.count())
        {
            auto* setting = parameters[index].getSetting();
            if (setting)
            {
                if (setting->set(value))
                {
                    onParameterChanged(parameters[index]);
                    emit stateChanged();
                }
            }
            else
            {
                qWarning() << Q_FUNC_INFO << ": parameter" << index << "setting is null";
            }
        }
        else
        {
            qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
        }
    }

    Q_INVOKABLE QString getParameterPlaceholder(int index) const
    {
        if (index >= 0 && index < parameters.count())
        {
            return parameters[index].getPlaceholder();
        }
        else
        {
            qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
        }

        return QString();
    }

    Q_INVOKABLE bool executeParameterInvoke(int index)
    {
        if (index >= 0 && index < parameters.count())
        {
            return parameters[index].executeInvokeCallback();
        }
        else
        {
            qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
        }

        return false;
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
            PasswordEcho = 10
        };

        static Parameter createLineEdit(Setting<QString>* setting, const QString& name, const QString& placeHolder = QString(), const std::set<Flag>& flags = std::set<Flag>())
        {
            Parameter parameter;

            parameter.type = Type::LineEdit;
            parameter.setting = setting;
            parameter.name = name;
            parameter.placeHolder = placeHolder;
            parameter.flags = flags;

            return parameter;
        }

        static Parameter createButton(const QString& text, std::function<void(const QVariant&)> invokeCalback, const QVariant& invokeCallbackArgument_ = QVariant())
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

        Setting<QString>* getSetting() { return setting; }
        const Setting<QString>* getSetting() const { return setting; }
        QString getName() const { return name; }
        void setName(const QString& name_) { name = name_; }
        Type getType() const { return type; }
        const std::set<Flag>& getFlags() const { return flags; }
        void setFlag(const Flag flag) { flags.insert(flag); }

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

        Setting<QString>* setting;
        QString name;
        Type type;
        std::set<Flag> flags;
        QString placeHolder;
        std::function<void(const QVariant&)> invokeCalback;
        QVariant invokeCallbackArgument;
    };

    void onParameterChanged(Parameter& parameter)
    {
        Setting<QString>* setting = parameter.getSetting();

        if (setting && *&setting == &stream)
        {
            stream.set(stream.get().trimmed());
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
            if (parameter.getSetting() == &setting)
            {
                return &parameter;
            }
        }

        return nullptr;
    }

    QList<Parameter> parameters;
    const AxelChat::ServiceType serviceType;
    State state;
    Setting<QString> stream;
    Setting<QString> lastSavedMessageId;
};
