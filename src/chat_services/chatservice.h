#pragma once

#include "utils.h"
#include "setting.h"
#include "chatservicestypes.h"
#include "uielementbridge.h"
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

    virtual void reconnectImpl() = 0;

    void onParameterChanged(UIElementBridge& parameter)
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

    virtual void onParameterChangedImpl(UIElementBridge& parameter)
    {
        Q_UNUSED(parameter)
    }

    std::shared_ptr<UIElementBridge> getParameter(const Setting<QString>& setting)
    {
        for (std::shared_ptr<UIElementBridge>& parameter : parameters)
        {
            if (parameter->getSettingString() == &setting)
            {
                return parameter;
            }
        }

        return nullptr;
    }

    QList<std::shared_ptr<UIElementBridge>> parameters;
    const AxelChat::ServiceType serviceType;
    State state;
    Setting<bool> enabled;
    Setting<QString> stream;
    Setting<QString> lastSavedMessageId;
};
