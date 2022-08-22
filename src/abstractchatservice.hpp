#ifndef ABSTRACTCHATSERVICE_H
#define ABSTRACTCHATSERVICE_H

#include "types.hpp"
#include <QObject>
#include <QQmlEngine>
#include <QCoreApplication>

class ChatHandler;
class ChatAuthor;
class ChatMessage;

class AbstractChatService : public QObject
{
    Q_OBJECT

public:
    enum class ServiceType
    {
        Unknown = -1,

        Software = 10,

        YouTube = 100,
        Twitch = 101,
        GoodGame = 102,
    };

    static QString getServiceTypeId(const AbstractChatService::ServiceType serviceType)
    {
        switch (serviceType)
        {
        case AbstractChatService::ServiceType::Unknown: return "unknown";
        case AbstractChatService::ServiceType::Software: return "software";

        case AbstractChatService::ServiceType::YouTube: return "youtube";
        case AbstractChatService::ServiceType::Twitch: return "twitch";
        case AbstractChatService::ServiceType::GoodGame: return "goodgame";
        }

        return "unknown";
    }

    static QString getNameLocalized(const AbstractChatService::ServiceType serviceType)
    {
        switch (serviceType)
        {
        case AbstractChatService::ServiceType::Unknown: return tr("Unknown");
        case AbstractChatService::ServiceType::Software: return QCoreApplication::applicationName();

        case AbstractChatService::ServiceType::YouTube: return tr("YouTube");
        case AbstractChatService::ServiceType::Twitch: return tr("Twitch");
        case AbstractChatService::ServiceType::GoodGame: return tr("GoodGame");
        }

        return tr("Unknown");
    }

    static QUrl getIconUrl(const AbstractChatService::ServiceType serviceType)
    {
        switch (serviceType)
        {
        case AbstractChatService::ServiceType::Unknown: return QUrl();
        case AbstractChatService::ServiceType::Software: return QUrl("qrc:/resources/images/axelchat-rounded.svg");

        case AbstractChatService::ServiceType::YouTube: return QUrl("qrc:/resources/images/youtube-icon.svg");
        case AbstractChatService::ServiceType::Twitch: return QUrl("qrc:/resources/images/twitch-icon.svg");
        case AbstractChatService::ServiceType::GoodGame: return QUrl("qrc:/resources/images/goodgame-icon.svg");
        }

        return QUrl();
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
    Q_PROPERTY(QUrl                 iconUrl                      READ getIconUrl                      CONSTANT)

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
    virtual ServiceType getServiceType() const { return serviceType; }

    virtual int viewersCount() const = 0;

    virtual qint64 traffic() const { return -1; }

    virtual void reconnect() = 0;

    QString getNameLocalized() const
    {
        return getNameLocalized(serviceType);
    }

    QUrl getIconUrl() const
    {
        return getIconUrl(serviceType);
    }

signals:
    void stateChanged();
    void detailedInformationChanged();
    void readyRead(QList<ChatMessage>& messages, QList<ChatAuthor>& authors);
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
