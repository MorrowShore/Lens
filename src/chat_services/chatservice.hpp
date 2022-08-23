#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include "types.hpp"
#include <QObject>
#include <QQmlEngine>
#include <QCoreApplication>

class ChatHandler;
class ChatAuthor;
class ChatMessage;

class ChatService : public QObject
{
    Q_OBJECT

public:
    Q_PROPERTY(QUrl                 broadcastUrl                 READ broadcastUrl                    NOTIFY stateChanged)
    Q_PROPERTY(QUrl                 chatUrl                      READ chatUrl                         NOTIFY stateChanged)
    Q_PROPERTY(QUrl                 controlPanelUrl              READ controlPanelUrl                 NOTIFY stateChanged)

    Q_PROPERTY(ConnectionStateType  connectionStateType          READ connectionStateType             NOTIFY stateChanged)
    Q_PROPERTY(QString              stateDescription             READ stateDescription                NOTIFY stateChanged)

    Q_PROPERTY(int                  viewersCount                 READ viewersCount                    NOTIFY stateChanged)

    enum class ServiceType
    {
        Unknown = -1,

        Software = 10,

        YouTube = 100,
        Twitch = 101,
        GoodGame = 102,
    };

    enum class ConnectionStateType
    {
        NotConnected = 10,
        Connecting = 20,
        Connected = 30
    };
    Q_ENUM(ConnectionStateType)

    static QString getServiceTypeId(const ChatService::ServiceType serviceType)
    {
        switch (serviceType)
        {
        case ChatService::ServiceType::Unknown: return "unknown";
        case ChatService::ServiceType::Software: return "software";

        case ChatService::ServiceType::YouTube: return "youtube";
        case ChatService::ServiceType::Twitch: return "twitch";
        case ChatService::ServiceType::GoodGame: return "goodgame";
        }

        return "unknown";
    }

    static QString getNameLocalized(const ChatService::ServiceType serviceType)
    {
        switch (serviceType)
        {
        case ChatService::ServiceType::Unknown: return tr("Unknown");
        case ChatService::ServiceType::Software: return QCoreApplication::applicationName();

        case ChatService::ServiceType::YouTube: return tr("YouTube");
        case ChatService::ServiceType::Twitch: return tr("Twitch");
        case ChatService::ServiceType::GoodGame: return tr("GoodGame");
        }

        return tr("Unknown");
    }

    static QUrl getIconUrl(const ChatService::ServiceType serviceType)
    {
        switch (serviceType)
        {
        case ChatService::ServiceType::Unknown: return QUrl();
        case ChatService::ServiceType::Software: return QUrl("qrc:/resources/images/axelchat-rounded.svg");

        case ChatService::ServiceType::YouTube: return QUrl("qrc:/resources/images/youtube-icon.svg");
        case ChatService::ServiceType::Twitch: return QUrl("qrc:/resources/images/twitch-icon.svg");
        case ChatService::ServiceType::GoodGame: return QUrl("qrc:/resources/images/goodgame-icon.svg");
        }

        return QUrl();
    }

    explicit ChatService(ServiceType serviceType_, QObject *parent = nullptr)
        : QObject(parent)
        , serviceType(serviceType_)
    { }

    virtual QUrl chatUrl() const { return QString(); }
    virtual QUrl controlPanelUrl() const { return QString(); }
    virtual QUrl broadcastUrl() const { return QString(); }

    virtual ConnectionStateType connectionStateType() const = 0;
    virtual QString stateDescription() const  = 0;
    ServiceType getServiceType() const { return serviceType; }

    virtual int viewersCount() const = 0;

    virtual void reconnect() = 0;

    Q_INVOKABLE QString getNameLocalized() const
    {
        return getNameLocalized(serviceType);
    }

    Q_INVOKABLE QUrl getIconUrl() const
    {
        return getIconUrl(serviceType);
    }

    Q_INVOKABLE virtual void setBroadcastLink(const QString& link) = 0;
    Q_INVOKABLE virtual QString getBroadcastLink() const = 0;

signals:
    void stateChanged();
    void readyRead(QList<ChatMessage>& messages, QList<ChatAuthor>& authors);
    void connected(QString name);
    void disconnected(QString name);
    void avatarDiscovered(const QString& channelId, const QUrl& url);

protected:
    const ServiceType serviceType;
};

#endif // CHATSERVICE_H
