#pragma once

#include "Guild.h"
#include <QNetworkAccessManager>

class Discord;

class GuildsStorage : public QObject
{
    Q_OBJECT
public:
    explicit GuildsStorage(Discord& discord, QNetworkAccessManager& network, QObject *parent = nullptr);

    std::shared_ptr<Guild> getGuild(const QString& id) const;

    const QMap<QString, std::shared_ptr<Guild>>& getGuilds() const;

    bool isGuildsLoaded() const;

    void requestGuilds(std::function<void()> onLoaded);

private:
    void addGuild(std::shared_ptr<Guild> guild);
    bool checkChannelsLoaded() const;

    Discord& discord;
    QNetworkAccessManager& network;

    bool guildsLoaded = false;
    QMap<QString, std::shared_ptr<Guild>> guilds;
};
