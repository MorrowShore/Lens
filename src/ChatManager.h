#pragma once

#include "utils/QtAxelChatUtils.h"
#include "authorqmlprovider.h"
#include "websocket.h"
#include "tcpserver.h"
#include "emotesprocessor.h"
#include "BackendManager.h"
#include "CefWebEngineQt/Manager.h"
#include <QSettings>
#include <QMap>
#include <QDateTime>
#include <QNetworkProxy>
#include <memory>

#ifdef QT_MULTIMEDIA_LIB
#include <QSound>
#endif

#ifndef AXELCHAT_LIBRARY
#include "outputtofile.h"
#include "chatbot.h"
#endif

class ChatManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int  connectedCount                   READ connectedCount                                                             NOTIFY connectedCountChanged)
    Q_PROPERTY(int  viewersTotalCount                READ getTotalViewers                                                            NOTIFY viewersTotalCountChanged)
    Q_PROPERTY(bool knownViewesServicesMoreOne       READ isKnownViewesServicesMoreOne                                               NOTIFY viewersTotalCountChanged)
    Q_PROPERTY(bool enabledSoundNewMessage           READ enabledSoundNewMessage           WRITE setEnabledSoundNewMessage           NOTIFY enabledSoundNewMessageChanged)
    Q_PROPERTY(bool enabledShowAuthorNameChanged     READ enabledShowAuthorNameChanged     WRITE setEnabledShowAuthorNameChanged     NOTIFY enabledShowAuthorNameChangedChanged)

    Q_PROPERTY(bool    proxyEnabled       READ proxyEnabled       WRITE setProxyEnabled       NOTIFY proxyChanged)
    Q_PROPERTY(QString proxyServerAddress READ proxyServerAddress WRITE setProxyServerAddress NOTIFY proxyChanged)
    Q_PROPERTY(int     proxyServerPort    READ proxyServerPort    WRITE setProxyServerPort    NOTIFY proxyChanged)

public:
    BackendManager& backend;

    explicit ChatManager(QSettings& settings, QNetworkAccessManager& network, cweqt::Manager& web, BackendManager& backend, QObject *parent = nullptr);
    ~ChatManager();

    MessagesModel& getMessagesModel();

#ifndef AXELCHAT_LIBRARY
    OutputToFile& getOutputToFile();
    ChatBot& getBot();
    AuthorQMLProvider& getAuthorQMLProvider() { return authorQMLProvider; }
#endif

#ifdef QT_QUICK_LIB
    static void declareQml()
    {
        qmlRegisterUncreatableType<ChatManager> ("AxelChat.ChatManager",
                                                 1, 0, "ChatManager", "Type cannot be created in QML");

        qmlRegisterUncreatableType<OutputToFile> ("AxelChat.OutputToFile",
                                                  1, 0, "OutputToFile", "Type cannot be created in QML");

        UIBridge::declareQml();
        ChatService::declareQml();

        AuthorQMLProvider::declareQML();
        ChatBot::declareQml();
    }
#endif

    inline bool enabledSoundNewMessage() const { return _enabledSoundNewMessage; }
    void setEnabledSoundNewMessage(bool enabled);

    inline bool enabledShowAuthorNameChanged() const { return _enableShowAuthorNameChanged; }
    void setEnabledShowAuthorNameChanged(bool enabled);

    int connectedCount() const;
    int getTotalViewers() const;

    bool isKnownViewesServicesMoreOne() const;

    void setProxyEnabled(bool enabled);
    inline bool proxyEnabled() const { return _enabledProxy; }

    void setProxyServerAddress(QString address);
    inline QString proxyServerAddress() const { return _proxy.hostName(); }
    void setProxyServerPort(int port);
    inline int proxyServerPort() const { return _proxy.port(); }

    QNetworkProxy proxy() const;

    Q_INVOKABLE int getServicesCount() const;
    Q_INVOKABLE ChatService* getServiceAtIndex(int index) const;
    Q_INVOKABLE ChatService* getServiceByType(int type) const;
    Q_INVOKABLE QUrl getServiceIconUrl(int serviceType) const;
    Q_INVOKABLE QUrl getServiceName(int serviceType) const;
    const QList<std::shared_ptr<ChatService>>& getServices() const { return services; }

signals:
    void connectedCountChanged();
    void viewersTotalCountChanged();
    void enabledSoundNewMessageChanged();
    void enabledShowAuthorNameChangedChanged();
    void proxyChanged();
    void messagesDataChanged();

public slots:
    void onReadyRead(const QList<std::shared_ptr<Message>>& messages, const QList<std::shared_ptr<Author>>& authors);
    void sendTestMessage(const QString& text);
    void sendSoftwareMessage(const QString& text);
    void playNewMessageSound();
    void clearMessages();
    void onStateChanged();

#ifndef AXELCHAT_LIBRARY
    void openProgramFolder();
#endif

private slots:
    void onAuthorDataUpdated(const QString& authorId, const QMap<Author::Role, QVariant>& values);
    void onAuthorNameChanged(const Author& author, const QString& prevName, const QString& newName);

private:
    void updateProxy();
    template <typename ChatServiceInheritedClass> void addService();
    void removeService(const int index);
    void addTestMessages();

    MessagesModel messagesModel;

    QSettings& settings;
    QNetworkAccessManager& network;
    cweqt::Manager& web;

    QList<std::shared_ptr<ChatService>> services;

    EmotesProcessor emotesProcessor;

#ifndef AXELCHAT_LIBRARY
    OutputToFile outputToFile;
    ChatBot bot;
    AuthorQMLProvider authorQMLProvider;
    WebSocket webSocket;
#endif

    bool _enabledSoundNewMessage = false;
    bool _enableShowAuthorNameChanged = true;

#ifdef QT_MULTIMEDIA_LIB
    std::unique_ptr<QSound> _soundDefaultNewMessage = std::unique_ptr<QSound>(new QSound(":/resources/sound/ui/beep1.wav"));
#endif

    bool _enabledProxy = false;
    QNetworkProxy _proxy = QNetworkProxy(QNetworkProxy::ProxyType::Socks5Proxy/*HttpProxy*/);

    TcpServer tcpServer;
};

