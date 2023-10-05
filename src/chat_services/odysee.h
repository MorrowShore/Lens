#pragma once

#include "chatservice.h"
#include <QJsonObject>
#include <QWebSocket>
#include <QTimer>

class Odysee : public ChatService
{
    Q_OBJECT
public:
    explicit Odysee(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);
    
    ConnectionState getConnectionState() const override;
    QString getStateDescription() const override;

protected:
    void reconnectImpl() override;
    void onReceiveWebSocket(const QString& rawData);

private slots:
    void requestClaimId();
    void requestLive();

    void requestChannelInfo(const QString& lbryUrl, const QString& authorId);

    void sendPing();

private:
    struct Info
    {
        QString channel;
        QString video;
        QString claimId;
    };

    static void extractChannelAndVideo(const QString& rawLink, QString& channel, QString& video);
    static QString extractChannelId(const QString& rawLink);

    void parseComment(const QJsonObject& data);
    void parseRemoved(const QJsonObject& data);

    QNetworkAccessManager& network;
    QWebSocket socket;

    QTimer timerSendPing;
    QTimer timerAcknowledgePingTimer;

    Info info;

    QHash<QString, QUrl> avatars; // <author id, avatarUrl>
};
