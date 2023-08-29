#pragma once

#include "Channel.h"
#include <QJsonObject>
#include <QNetworkAccessManager>

class Discord;
class GuildsStorage;

class Guild : public QObject
{
public:
    static Guild* fromJson(const QJsonObject& object, GuildsStorage& storage, Discord& discord, QNetworkAccessManager& network, QObject *parent = nullptr);

    explicit Guild(const QString& id, const QString& name, GuildsStorage& storage, Discord& discord, QNetworkAccessManager& network, QObject *parent = nullptr);

    Channel& getChannel(const QString& id);

    const Channel& getChannel(const QString& id) const;

    bool isChannelsLoaded() const;

    const QMap<QString, Channel>& getChannels() const;

    void requestChannels(std::function<void()> onLoaded);

    QString getId() const { return id; }

    QString getName() const { return name; }

private:
    void addChannel(const Channel& channel);

    const QString id;
    const QString name;

    GuildsStorage& storage;
    Discord& discord;
    QNetworkAccessManager& network;

    bool channelsLoaded = false;
    QMap<QString, Channel> channels;
};
