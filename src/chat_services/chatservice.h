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

    static QString getServiceTypeId(const AxelChat::ServiceType serviceType);
    static QString getName(const AxelChat::ServiceType serviceType);
    static QUrl getIconUrl(const AxelChat::ServiceType serviceType);

    explicit ChatService(QSettings& settings, const QString& settingsGroupPath, AxelChat::ServiceType serviceType_, QObject *parent = nullptr);

    QUrl getChatUrl() const;
    QUrl getControlPanelUrl() const;
    Q_INVOKABLE QUrl getStreamUrl() const;

    virtual ConnectionStateType getConnectionStateType() const = 0;
    virtual QString getStateDescription() const  = 0;
    AxelChat::ServiceType getServiceType() const;

    void reconnect();

    int getViewersCount() const;

    bool isEnabled() const;
    void setEnabled(const bool enabled_);

    const Setting<QString>& getLastSavedMessageId() const;
    void setLastSavedMessageId(const QString& messageId);

    QJsonObject toJson() const;

    const State& getState() const;

    Q_INVOKABLE QString getName() const;
    Q_INVOKABLE QUrl getIconUrl() const;

    Q_INVOKABLE int getParametersCount() const;
    Q_INVOKABLE QString getParameterName(int index) const;
    Q_INVOKABLE QString getParameterValueString(int index) const;
    Q_INVOKABLE void setParameterValueString(int index, const QString& value);
    Q_INVOKABLE bool getParameterValueBool(int index) const;
    Q_INVOKABLE void setParameterValueBool(int index, const bool value);
    Q_INVOKABLE int getParameterType(int index) const;
    Q_INVOKABLE bool isParameterHasFlag(int index, int flag) const;
    Q_INVOKABLE QString getParameterPlaceholder(int index) const;
    Q_INVOKABLE bool executeParameterInvoke(int index);

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

        emit stateChanged();
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
