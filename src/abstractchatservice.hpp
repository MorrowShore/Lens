#ifndef ABSTRACTCHATSERVICE_H
#define ABSTRACTCHATSERVICE_H

#include "types.hpp"
#include <QObject>
#include <QQmlEngine>

class ChatHandler;
class MessageAuthor;
class ChatMessage;

class AbstractChatService : public QObject
{
    Q_OBJECT

public:
    enum class ServiceType
    {
        Unknown = -1,

        Software = 10,
        Test = 11,

        YouTube = 100,
        Twitch = 101,
        GoodGame = 102,
    };

    static QString serviceTypeToString(const AbstractChatService::ServiceType serviceType)
    {
        switch (serviceType)
        {
        case AbstractChatService::ServiceType::Software: return "softwarenotification";
        case AbstractChatService::ServiceType::Test: return "testmessage";
        case AbstractChatService::ServiceType::YouTube: return "youtube";
        case AbstractChatService::ServiceType::Twitch: return "twitch";
        case AbstractChatService::ServiceType::Unknown:
        default:
            return "unknown";
        }

        return "unknown";
    }

    enum class ConnectionStateType
    {
        NotConnected = 10,
        Connecting = 20,
        Connected = 30
    };
    Q_ENUM(ConnectionStateType)

    Q_PROPERTY(QUrl                 broadcastUrl                 READ broadcastUrl                    NOTIFY stateChanged)
    Q_PROPERTY(QUrl                 chatUrl                      READ chatUrl                         NOTIFY stateChanged)
    Q_PROPERTY(QUrl                 controlPanelUrl              READ controlPanelUrl                 NOTIFY stateChanged)

    Q_PROPERTY(ConnectionStateType  connectionStateType          READ connectionStateType             NOTIFY stateChanged)
    Q_PROPERTY(QString              stateDescription             READ stateDescription                NOTIFY stateChanged)
    Q_PROPERTY(QString              detailedInformation          READ detailedInformation             NOTIFY detailedInformationChanged)

    Q_PROPERTY(int                  viewersCount                 READ viewersCount                    NOTIFY stateChanged)

    Q_PROPERTY(qint64               traffic                      READ traffic                    NOTIFY stateChanged)

    explicit AbstractChatService(ServiceType serviceType_, QObject *parent = nullptr)
        : QObject(parent)
        , serviceType(serviceType_)
    { }

    virtual QUrl chatUrl() const { return QString(); }
    virtual QUrl controlPanelUrl() const { return QString(); }
    virtual QUrl broadcastUrl() const { return QString(); }

    virtual ConnectionStateType connectionStateType() const = 0;
    virtual QString stateDescription() const  = 0;
    virtual QString detailedInformation() const = 0;
    virtual QString getNameLocalized() const = 0;
    virtual ServiceType getServiceType() const { return serviceType; }

    virtual int viewersCount() const = 0;

    virtual qint64 traffic() const { return -1; }

    virtual void reconnect() = 0;

signals:
    void stateChanged();
    void detailedInformationChanged();
    void readyRead(QList<ChatMessage>& messages);
    void connected(QString name);
    void disconnected(QString name);
    void avatarDiscovered(const QString& channelId, const QUrl& url);
    void needSendNotification(const QString& text);

protected:
    void sendNotification(const QString& text)
    {
        emit needSendNotification(text);
    }

    const ServiceType serviceType;
};

#endif // ABSTRACTCHATSERVICE_H
