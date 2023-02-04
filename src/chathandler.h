#pragma once

#include "utils.h"
#include "authorqmlprovider.h"
#include "websocket.h"
#include "tcpserver.h"
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

class ChatHandler : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int  connectedCount                   READ connectedCount                                                             NOTIFY connectedCountChanged)
    Q_PROPERTY(int  viewersTotalCount                READ getViewersTotalCount                                                       NOTIFY viewersTotalCountChanged)
    Q_PROPERTY(bool enabledSoundNewMessage           READ enabledSoundNewMessage           WRITE setEnabledSoundNewMessage           NOTIFY enabledSoundNewMessageChanged)
    Q_PROPERTY(bool enabledClearMessagesOnLinkChange READ enabledClearMessagesOnLinkChange WRITE setEnabledClearMessagesOnLinkChange NOTIFY enabledClearMessagesOnLinkChangeChanged)
    Q_PROPERTY(bool enabledShowAuthorNameChanged     READ enabledShowAuthorNameChanged     WRITE setEnabledShowAuthorNameChanged     NOTIFY enabledShowAuthorNameChangedChanged)

    Q_PROPERTY(bool    proxyEnabled       READ proxyEnabled       WRITE setProxyEnabled       NOTIFY proxyChanged)
    Q_PROPERTY(QString proxyServerAddress READ proxyServerAddress WRITE setProxyServerAddress NOTIFY proxyChanged)
    Q_PROPERTY(int     proxyServerPort    READ proxyServerPort    WRITE setProxyServerPort    NOTIFY proxyChanged)

public:
    explicit ChatHandler(QSettings& settings, QNetworkAccessManager& network, QObject *parent = nullptr);

    MessagesModel& getMessagesModel();

#ifndef AXELCHAT_LIBRARY
    OutputToFile& getOutputToFile();
    ChatBot& getBot();
    AuthorQMLProvider& getAuthorQMLProvider() { return authorQMLProvider; }
#endif

#ifdef QT_QUICK_LIB
    static void declareQml()
    {
        qmlRegisterUncreatableType<ChatHandler> ("AxelChat.ChatHandler",
                                                 1, 0, "ChatHandler", "Type cannot be created in QML");

        qmlRegisterUncreatableType<OutputToFile> ("AxelChat.OutputToFile",
                                                  1, 0, "OutputToFile", "Type cannot be created in QML");

        ChatService::declareQml();

        AuthorQMLProvider::declareQML();
        ChatBot::declareQml();
    }
#endif

    inline bool enabledSoundNewMessage() const { return _enabledSoundNewMessage; }
    void setEnabledSoundNewMessage(bool enabled);

    inline bool enabledShowAuthorNameChanged() const { return _enableShowAuthorNameChanged; }
    void setEnabledShowAuthorNameChanged(bool enabled);

    inline bool enabledClearMessagesOnLinkChange() const { return _enabledClearMessagesOnLinkChange; }
    void setEnabledClearMessagesOnLinkChange(bool enabled);

    int connectedCount() const;
    int getViewersTotalCount() const;

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
    const QList<ChatService*>& getServices() const { return services; }

signals:
    void connectedCountChanged();
    void viewersTotalCountChanged();
    void enabledSoundNewMessageChanged();
    void enabledClearMessagesOnLinkChangeChanged();
    void enabledShowAuthorNameChangedChanged();
    void proxyChanged();
    void messagesDataChanged();

public slots:
    void onReadyRead(QList<Message>& messages, QList<Author>& authors);
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
    void onConnectedChanged(const bool connected, const QString& name);
    void onAuthorNameChanged(const Author& author, const QString& prevName, const QString& newName);

private:
    void updateProxy();
    void addService(ChatService* service);
    void addTestMessages();

    MessagesModel messagesModel;

    QSettings& settings;
    QNetworkAccessManager& network;

    QList<ChatService*> services;

#ifndef AXELCHAT_LIBRARY
    OutputToFile outputToFile;
    ChatBot bot;
    AuthorQMLProvider authorQMLProvider;
    WebSocket webSocket;
#endif

    bool _enabledSoundNewMessage = false;
    bool _enabledClearMessagesOnLinkChange = false;
    bool _enableShowAuthorNameChanged = true;

#ifdef QT_MULTIMEDIA_LIB
    std::unique_ptr<QSound> _soundDefaultNewMessage = std::unique_ptr<QSound>(new QSound(":/resources/sound/ui/beep1.wav"));
#endif

    bool _enabledProxy = false;
    QNetworkProxy _proxy = QNetworkProxy(QNetworkProxy::ProxyType::Socks5Proxy/*HttpProxy*/);

    TcpServer tcpServer;
};

