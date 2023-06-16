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
    explicit Rumble(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network, QObject *parent = nullptr);

    ConnectionStateType getConnectionStateType() const override;
    QString getStateDescription() const override;

protected:
    void reconnectImpl() override;

private:
    struct Info
    {
        QString chatId;
        QString videoId;
    };

    struct DonutColor
    {
        int64_t priceDollars = 0;
        QColor color;
    };

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
    QList<Message::Content*> parseBlock(const QJsonObject& block) const;

    QNetworkAccessManager& network;
    Info info;
    SseManager sse;

    QHash<QString, QString> badgesUrls;
    QList<DonutColor> donutColors;
    QHash<QString, QString> emotesUrls;

    QTimer timerRequestViewers;
    QTimer timerReconnect;
};
