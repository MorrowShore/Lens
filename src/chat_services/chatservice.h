#pragma once

#include "utils.h"
#include "setting.h"
#include "chatservicestypes.h"
#include "uielementbridge.h"
#include "tcprequest.h"
#include "tcpreply.h"
#include "tcpserver.h"
#include "models/author.h"
#include <QSettings>
#include <QObject>
#include <QQmlEngine>
#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <QQuickItem>
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

    explicit ChatService(QSettings& settings, const QString& settingsGroupPath, AxelChat::ServiceType serviceType_, QObject *parent = nullptr);

    static QString getServiceTypeId(const AxelChat::ServiceType serviceType);
    static QString getName(const AxelChat::ServiceType serviceType);
    static QUrl getIconUrl(const AxelChat::ServiceType serviceType);

    QUrl getChatUrl() const;
    QUrl getControlPanelUrl() const;
    Q_INVOKABLE QUrl getStreamUrl() const;

    virtual ConnectionStateType getConnectionStateType() const = 0;
    virtual QString getStateDescription() const  = 0;
    virtual TcpReply processTcpRequest(const TcpRequest& request);
    AxelChat::ServiceType getServiceType() const;

    void reconnect();

    int getViewersCount() const;

    bool isEnabled() const;
    void setEnabled(const bool enabled_);

    const Setting<QString>& getLastSavedMessageId() const;
    void setLastSavedMessageId(const QString& messageId);

    QJsonObject getStateJson() const;
    QJsonObject getStaticInfoJson() const;

    const State& getState() const;

    Q_INVOKABLE QString getName() const;
    Q_INVOKABLE QUrl getIconUrl() const;

    Q_INVOKABLE int getUIElementBridgesCount() const;
    Q_INVOKABLE int getUIElementBridgeType(const int index) const;
    Q_INVOKABLE void bindQmlItem(const int index, QQuickItem* item);

#ifdef QT_QUICK_LIB
    static void declareQml()
    {
        qmlRegisterUncreatableType<ChatService> ("AxelChat.ChatService", 1, 0, "ChatService", "Type cannot be created in QML");
    }
#endif

signals:
    void stateChanged();
    void readyRead(QList<Message>& messages, QList<Author>& authors);
    void connectedChanged(const bool connected);
    void authorDataUpdated(const QString& authorId, const QMap<Author::Role, QVariant>& values);

protected:

    virtual void reconnectImpl() = 0;

    void addUIElement(std::shared_ptr<UIElementBridge> element);
    void onUIElementChanged(const std::shared_ptr<UIElementBridge> element);

    virtual void onUiElementChangedImpl(const std::shared_ptr<UIElementBridge> element)
    {
        Q_UNUSED(element)
    }

    std::shared_ptr<UIElementBridge> getUIElementBridgeBySetting(const Setting<QString>& setting);

    const AxelChat::ServiceType serviceType;
    State state;
    Setting<bool> enabled;
    Setting<QString> stream;
    Setting<QString> lastSavedMessageId;

private:
    QList<std::shared_ptr<UIElementBridge>> uiElements;
};
