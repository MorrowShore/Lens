#pragma once

#include "setting.h"
#include "ChatServiceType.h"
#include "UIBridge/UIBridge.h"
#include "UIBridge/UIBridgeElement.h"
#include "tcprequest.h"
#include "tcpreply.h"
#include "tcpserver.h"
#include "Feature.h"
#include "models/author.h"
#include "CefWebEngineQt/Manager.h"
#include <QSettings>
#include <QObject>
#include <QQmlEngine>
#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <set>

class ChatManager;
class Author;
class Message;

class ChatService : public Feature
{
    Q_OBJECT

public:
    Q_PROPERTY(bool                 enabled                      READ isEnabled     WRITE setEnabled     NOTIFY stateChanged)
    Q_PROPERTY(QUrl                 streamUrl                    READ getStreamUrl                       NOTIFY stateChanged)
    Q_PROPERTY(QUrl                 chatUrl                      READ getChatUrl                         NOTIFY stateChanged)
    Q_PROPERTY(QUrl                 controlPanelUrl              READ getControlPanelUrl                 NOTIFY stateChanged)

    Q_PROPERTY(ConnectionState      connectionState              READ getConnectionState                 NOTIFY stateChanged)
    Q_PROPERTY(QString              mainError                    READ getMainError                       NOTIFY stateChanged)
    Q_PROPERTY(QStringList          warnings                     READ getWarnings                       NOTIFY stateChanged)

    Q_PROPERTY(int                  viewersCount                 READ getViewers                         NOTIFY stateChanged)

    Q_PROPERTY(bool  enabledThirdPartyEmotes      READ isEnabledThirdPartyEmotes   WRITE setEnabledThirdPartyEmotes     NOTIFY stateChanged)

    enum class ConnectionState
    {
        NotConnected = 10,
        Connecting = 20,
        Connected = 30
    };
    Q_ENUM(ConnectionState)

    struct State
    {
        bool connected = false;
        QString streamId;
        QUrl streamUrl;
        QUrl chatUrl;
        QUrl controlPanelUrl;
        int viewers = -1;

        bool sendedState = false;
    };

    static const QString UnknownBadge;
    
    explicit ChatService(ChatManager& manager, QSettings& settings, const QString& settingsGroupPathParent, ChatServiceType serviceType_, const bool enabledThirdPartyEmotesDefault, QObject *parent = nullptr);
    virtual ~ChatService(){}

    ChatService (const ChatService&) = delete;
    ChatService (ChatService&&) = delete;
    ChatService& operator= (const ChatService&) = delete;
    ChatService&& operator= (ChatService&&) = delete;

    static QString getServiceTypeId(const ChatServiceType serviceType);
    static QString getName(const ChatServiceType serviceType);
    static QUrl getIconUrl(const ChatServiceType serviceType);

    QUrl getChatUrl() const;
    QUrl getControlPanelUrl() const;
    Q_INVOKABLE QUrl getStreamUrl() const;

    virtual ConnectionState getConnectionState() const = 0;
    virtual QString getMainError() const;
    virtual TcpReply processTcpRequest(const TcpRequest& request);
    ChatServiceType getServiceType() const;

    void reset();

    int getViewers() const;

    bool isEnabled() const;
    void setEnabled(const bool enabled);

    bool isEnabledThirdPartyEmotes() const;
    void setEnabledThirdPartyEmotes(const bool enabled);

    const Setting<QString>& getLastSavedMessageId() const;
    void setLastSavedMessageId(const QString& messageId);

    QJsonObject getStateJson() const;
    QJsonObject getStaticInfoJson() const;

    const State& getState() const;

    Q_INVOKABLE QString getName() const;
    Q_INVOKABLE QUrl getIconUrl() const;

    Q_INVOKABLE UIBridge* getUiBridge() const { return &const_cast<UIBridge&>(ui); }

#ifdef QT_QUICK_LIB
    static void declareQml()
    {
        qmlRegisterUncreatableType<ChatService> ("AxelChat.ChatService", 1, 0, "ChatService", "Type cannot be created in QML");
    }
#endif

signals:
    void stateChanged();
    void readyRead(const QList<std::shared_ptr<Message>>& messages, const QList<std::shared_ptr<Author>>& authors);
    void authorDataUpdated(const QString& authorId, const QMap<Author::Role, QVariant>& values);

private slots:
    void onUIElementChanged(const std::shared_ptr<UIBridgeElement>& element);

private:
    const ChatServiceType serviceType;
    const QString settingsGroupPath;

protected:
    virtual void resetImpl() = 0;

    virtual void connectImpl() = 0;
    virtual QStringList getWarnings() const { return QStringList(); }

    void connect();

    QString generateAuthorId(const QString& rawId) const;
    QString generateMessageId(const QString& rawId) const;

    void setConnected();
    bool isConnected() const;

    void setViewers(const int count);

    std::shared_ptr<Author> getServiceAuthor() const;

    const QString& getSettingsGroupPath() const { return settingsGroupPath; }
    
    ChatManager& manager;
    State state;
    Setting<QString> stream;
    UIBridge ui;

private:
    Setting<bool> enabled;
    QTimer timerReconnect;
    Setting<bool> enabledThirdPartyEmotes;
    Setting<QString> lastSavedMessageId;
};
