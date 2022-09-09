#pragma once

#include "utils.h"
#include "chat_services/youtube.h"
#include "chat_services/twitch.h"
#include "chat_services/goodgame.h"
#include "authorqmlprovider.h"
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
    Q_PROPERTY(int  viewersTotalCount                READ viewersTotalCount                                                          NOTIFY viewersTotalCountChanged)
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
    static void declareQml();
#endif

    inline bool enabledSoundNewMessage() const { return _enabledSoundNewMessage; }
    void setEnabledSoundNewMessage(bool enabled);

    inline bool enabledShowAuthorNameChanged() const { return _enableShowAuthorNameChanged; }
    void setEnabledShowAuthorNameChanged(bool enabled);

    inline bool enabledClearMessagesOnLinkChange() const { return _enabledClearMessagesOnLinkChange; }
    void setEnabledClearMessagesOnLinkChange(bool enabled);

    int connectedCount() const;
    int viewersTotalCount() const;

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
    void onAuthorChanged(const QString& authorId, const QUrl& url);
    void clearMessages();
    void onStateChanged();

#ifndef AXELCHAT_LIBRARY
    void openProgramFolder();
#endif

private slots:
    void onConnectedChanged(const bool connected, const QString& name);
    void onAuthorNameChanged(const Author& author, const QString& prevName, const QString& newName);

private:
    void updateProxy();
    void addService(ChatService* service);

    MessagesModel messagesModel;

    QSettings& settings;
    QNetworkAccessManager& network;

    QList<ChatService*> services;

#ifndef AXELCHAT_LIBRARY
    OutputToFile outputToFile;
    ChatBot bot;
    AuthorQMLProvider authorQMLProvider;
#endif

    bool _enabledSoundNewMessage = false;
    bool _enabledClearMessagesOnLinkChange = false;
    bool _enableShowAuthorNameChanged = false;

#ifdef QT_MULTIMEDIA_LIB
    std::unique_ptr<QSound> _soundDefaultNewMessage = std::unique_ptr<QSound>(new QSound(":/resources/sound/ui/beep1.wav"));
#endif

    bool _enabledProxy = false;
    QNetworkProxy _proxy = QNetworkProxy(QNetworkProxy::ProxyType::Socks5Proxy/*HttpProxy*/);
};

