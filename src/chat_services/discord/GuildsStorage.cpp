#include "guildsstorage.h"
#include "Discord.h"

GuildsStorage::GuildsStorage(Discord& discord_, QNetworkAccessManager& network_, QObject *parent)
    : QObject(parent)
    , discord(discord_)
    , network(network_)
{

}

std::shared_ptr<Guild> GuildsStorage::getGuild(const QString &id) const
{
    if (guilds.contains(id))
    {
        return guilds[id];
    }

    qWarning() << "guild with id" << id << "not found";

    return nullptr;
}

const QMap<QString, std::shared_ptr<Guild>> &GuildsStorage::getGuilds() const
{
    return guilds;
}

bool GuildsStorage::isGuildsLoaded() const
{
    return guildsLoaded;
}

void GuildsStorage::requestGuilds(std::function<void()> onLoaded)
{
    QNetworkReply* reply = network.get(discord.createRequestAsBot(Discord::ApiPrefix + "/users/@me/guilds"));
    connect(reply, &QNetworkReply::finished, this, [this, reply, onLoaded]()
    {
        QByteArray data;
        if (!discord.checkReply(reply, Q_FUNC_INFO, data))
        {
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isArray())
        {
            qWarning() << "document is not array, doc =" << doc;
            return;
        }

        const QJsonArray jsonGuilds = doc.array();
        for (const QJsonValue& v : qAsConst(jsonGuilds))
        {
            if (Guild* guild_ = Guild::fromJson(v.toObject(), *this, discord, network); guild_)
            {
                std::shared_ptr<Guild> guild(guild_);

                addGuild(guild);

                guild->requestChannels([this, onLoaded]()
                {
                    if (checkChannelsLoaded())
                    {
                        if (onLoaded)
                        {
                            guildsLoaded = true;
                            onLoaded();
                        }
                    }
                });
            }
            else
            {
                qWarning() << "failed to parse guild";
            }
        }

        if (checkChannelsLoaded())
        {
            if (onLoaded)
            {
                guildsLoaded = true;
                onLoaded();
            }
        }
    });
}

void GuildsStorage::addGuild(std::shared_ptr<Guild> guild)
{
    if (!guild)
    {
        qWarning() << "guild is null";
        return;
    }

    if (guild->getId().isEmpty())
    {
        qWarning() << "guild has empty id, ignore, name =" << guild->getName();
        return;
    }

    if (guild->getName().isEmpty())
    {
        qWarning() << "guild has empty name, id =" << guild->getId();
    }

    guilds.insert(guild->getId(), guild);
}

bool GuildsStorage::checkChannelsLoaded() const
{
    bool allLoaded = true;

    for (const std::shared_ptr<Guild>& guild : qAsConst(guilds))
    {
        if (!guild)
        {
            qWarning() << "guild is null";
            continue;
        }

        if (!guild->isChannelsLoaded())
        {
            allLoaded = false;
            break;
        }
    }

    return allLoaded;
}
