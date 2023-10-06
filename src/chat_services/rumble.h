#pragma once

#include "chatservice.h"
#include "models/message.h"
#include "ssemanager.h"
#include <QJsonObject>
#include <QTimer>

class Rumble : public ChatService
{
    Q_OBJECT
public:
    explicit Rumble(ChatHandler& manager, QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, cweqt::Manager& web, QObject *parent = nullptr);
    
    ConnectionState getConnectionState() const override;
    QString getMainError() const override;

protected:
    void reconnectImpl() override;

private:
    struct Info
    {
        QString chatId;
        QString videoId;
        QString viewerId;
    };

    struct DonutColor
    {
        int64_t priceDollars = 0;
        QColor color;
    };

    static QString generateViewerId();
    static QString extractLinkId(const QString& rawLink);
    static QString parseChatId(const QByteArray& html);
    static QString parseVideoId(const QByteArray& html);

    void requestVideoPage();
    void requestViewers();
    void requestEmotes();
    void startReadChat();
    void onSseReceived(const QJsonObject& root);
    void parseMessage(const QJsonObject& user, const QJsonObject& message);
    void parseConfig(const QJsonObject& config);
    QColor getDonutColor(const int64_t priceDollars) const;
    QList<std::shared_ptr<Message::Content>> parseBlock(const QJsonObject& block) const;

    QNetworkAccessManager& network;
    Info info;
    SseManager sse;

    QHash<QString, QString> badgesUrls;
    QList<DonutColor> donutColors;
    QHash<QString, QString> emotesUrls;

    QTimer timerRequestViewers;
};

