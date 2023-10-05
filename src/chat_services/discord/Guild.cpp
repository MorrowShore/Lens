#include "Guild.h"
#include "discord.h"

Guild* Guild::fromJson(const QJsonObject &object, GuildsStorage& storage, Discord& discord, QNetworkAccessManager& network, QObject *parent)
{
    const QString id = object.value("id").toString();
    const QString name = object.value("name").toString().trimmed();

    if (id.isEmpty())
    {
        qWarning() << "id is empty";
        return nullptr;
    }

    return new Guild(id, name, storage, discord, network, parent);
}

Guild::Guild(const QString& id_, const QString& name_, GuildsStorage &storage_, Discord &discord_, QNetworkAccessManager &network_, QObject *parent)
    : QObject(parent)
    , id(id_)
    , name(name_)
    , storage(storage_)
    , discord(discord_)
    , network(network_)
{

}

Channel &Guild::getChannel(const QString &id)
{
    if (channels.contains(id))
    {
        return channels[id];
    }

    qWarning() << "channel with id" << id << "not found";

    static Channel channel;

    return channel;
}

const Channel &Guild::getChannel(const QString &id) const
{
    return const_cast<Guild&>(*this).getChannel(id);
}

bool Guild::isChannelsLoaded() const
{
    return channelsLoaded;
}

void Guild::addChannel(const Channel &channel)
{
    if (channel.getId().isEmpty())
    {
        qWarning() << "channel has empty id, name =" << channel.getName();
    }

    if (channel.getName().isEmpty())
    {
        qWarning() << "channel has empty name, id =" << channel.getId();
    }

    channels.insert(channel.getId(), channel);
}

const QMap<QString, Channel> &Guild::getChannels() const
{
    return channels;
}

void Guild::requestChannels(std::function<void()> onLoaded)
{
    QNetworkReply* reply = network.get(discord.createRequestAsBot(Discord::ApiPrefix + "/guilds/" + id + "/channels"));
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
            qCritical() << "document is not array, doc =" << doc;
            return;
        }

        const QJsonArray array = doc.array();
        for (const QJsonValue& v : qAsConst(array))
        {
            const QJsonObject object = v.toObject();
            const std::optional<Channel> channel = Channel::fromJson(object);
            if (channel)
            {
                addChannel(*channel);
            }
            else
            {
                qCritical() << "failed to parse channel, object =" << object;
            }
        }

        channelsLoaded = true;

        if (onLoaded)
        {
            onLoaded();
        }
    });
}
