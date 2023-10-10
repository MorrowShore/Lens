#pragma once

#include "Feature.h"
#include "chat_services/twitch.h"
#include "emote_services/emoteservice.h"
#include "models/message.h"
#include <QNetworkAccessManager>
#include <QSettings>
#include <QTimer>

class EmotesProcessor : public Feature
{
    Q_OBJECT
public:
    explicit EmotesProcessor(BackendManager& backend, QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, QObject *parent = nullptr);
    void processMessage(std::shared_ptr<Message> message);
    void connectTwitch(std::shared_ptr<Twitch> twitch);

signals:

private slots:
    void loadGlobal();
    void loadChannel(const Twitch::ChannelInfo& channelInfo);

private:
    QString getEmoteUrl(const QString& name) const;
    template <typename EmoteServiceInheritedClass> void addService();

    QSettings& settings;
    const QString settingsGroupPath;
    QNetworkAccessManager& network;

    QTimer timer;

    QList<std::shared_ptr<EmoteService>> services;

    std::optional<Twitch::ChannelInfo> twitchChannelInfo;
};
