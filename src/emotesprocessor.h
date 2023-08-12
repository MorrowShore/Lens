#pragma once

#include "chat_services/twitch.h"
#include "emote_services/emoteservice.h"
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
    void connectTwitch(std::shared_ptr<Twitch> twitch);

signals:

private slots:
    void loadAll();

    void loadSevenTvGlobalEmotes();
    void loadSevenTvUserEmotes(const QString& twitchBroadcasterId);
    static QHash<QString, QString> parseSevenTvSet(const QJsonObject& set);

private:
    QString getEmoteUrl(const QString& name) const;
    template <typename EmoteServiceInheritedClass> void addService();

    QSettings& settings;
    const QString settingsGroupPath;
    QNetworkAccessManager& network;

    QTimer timer;

    QList<std::shared_ptr<EmoteService>> services;

    QHash<QString, QString> sevenTvGlobalEmotes;
    QHash<QString, QString> sevenTvUserEmotes;

    std::optional<Twitch::ChannelInfo> twitchChannelInfo;
};
