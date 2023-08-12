#pragma once

#include "chat_services/twitch.h"
#include "models/message.h"
#include <QNetworkAccessManager>
#include <QSettings>
#include <QTimer>

class EmotesProcessor : public QObject
{
    Q_OBJECT
public:
    explicit EmotesProcessor(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, QObject *parent = nullptr);
    void processMessage(std::shared_ptr<Message> message);
    void setTwitch(std::shared_ptr<Twitch> twitch);

signals:

private slots:
    void loadAll();

    void loadBttvGlobalEmotes();
    void loadBttvUserEmotes(const QString& twitchBroadcasterId);

    void loadFfzGlobalEmotes();
    void loadFfzUserEmotes(const QString& twitchBroadcasterId);
    static QHash<QString, QString> parseFfzSet(const QJsonObject& set);

    void loadSevenTvGlobalEmotes();
    void loadSevenTvUserEmotes(const QString& twitchBroadcasterId);
    static QHash<QString, QString> parseSevenTvSet(const QJsonObject& set);

private:
    QString getEmoteUrl(const QString& name) const;

    QSettings& settings;
    const QString settingsGroupPath;
    QNetworkAccessManager& network;

    QTimer timer;

    // <name, url>

    QHash<QString, QString> bttvGlobalEmotes;
    QHash<QString, QString> bttvUserEmotes;

    QHash<QString, QString> ffzGlobalEmotes;
    QHash<QString, QString> ffzUserEmotes;

    QHash<QString, QString> sevenTvGlobalEmotes;
    QHash<QString, QString> sevenTvUserEmotes;

    std::shared_ptr<Twitch> twitch;
};
