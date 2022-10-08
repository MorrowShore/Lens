#ifndef CHATSERVICE_H
#define CHATSERVICE_H

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
        }

        return QUrl();
    }

    explicit ChatService(QSettings& settings, const QString& settingsGroupPath, AxelChat::ServiceType serviceType_, QObject *parent = nullptr)
        : QObject(parent)
        , serviceType(serviceType_)
        , stream(settings, settingsGroupPath + "/stream")
        , lastSavedMessageId(settings, settingsGroupPath + "/lastSavedMessageId")
    {
        parameters.append(Parameter(&stream, tr("Stream"), Parameter::Type::String, {}));
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
        if (index < parameters.count())
        {
            return parameters[index].getName();
        }

        qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";

        return QString();
    }

    Q_INVOKABLE QString getParameterValue(int index) const
    {
        if (index < parameters.count())
        {
            return parameters[index].getSetting()->get();
        }

        qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";

        return QString();
    }

    Q_INVOKABLE int getParameterType(int index) const
    {
        if (index < parameters.count())
        {
            return (int)parameters[index].getType();
        }

        qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";

        return (int)Parameter::Type::Unknown;
    }

    Q_INVOKABLE bool isParameterHasFlag(int index, int flag) const
    {
        if (index < parameters.count())
        {
            const std::set<Parameter::Flag>& flags = parameters[index].getFlags();

            return flags.find((Parameter::Flag)flag) != flags.end();
        }

        qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";

        return false;
    }

    Q_INVOKABLE void setParameterValue(int index, const QString& value)
    {
        if (index < parameters.count())
        {
            if (parameters[index].getSetting()->set(value))
            {
                onParameterChanged(parameters[index]);
                emit stateChanged();
            }
        }
        else
        {
            qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
        }
    }

    Q_INVOKABLE QString getParameterPlaceholder(int index) const
    {
        if (index < parameters.count())
        {
            return parameters[index].getPlaceholder();
        }
        else
        {
            qWarning() << Q_FUNC_INFO << ": parameter index" << index << "not valid";
        }

        return QString();
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
            String = 10,
            ButtonUrl = 20,
            Label = 21,
        };

        enum class Flag
        {
            PasswordEcho = 10
        };

        Parameter(Setting<QString>* setting_, const QString& name_, const Type type_, const std::set<Flag> flags_)
            : setting(setting_)
            , name(name_)
            , type(type_)
            , flags(flags_)
        {}

        Setting<QString>* getSetting() { return setting; }
        const Setting<QString>* getSetting() const { return setting; }
        QString getName() const { return name; }
        Type getType() const { return type; }
        const std::set<Flag>& getFlags() const { return flags; }

        void setPlaceholder(const QString& placeHolder_) { placeHolder = placeHolder_; }
        QString getPlaceholder() const { return placeHolder; }

    private:
        Setting<QString>* setting;
        QString name;
        Type type;
        std::set<Flag> flags;
        QString placeHolder;
    };

    void onParameterChanged(Parameter& parameter)
    {
        Setting<QString>& setting = *parameter.getSetting();

        if (&setting == &stream)
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

#endif // CHATSERVICE_H
