#pragma once

#include "Channel.h"
#include <QJsonObject>

struct Guild
{
    static std::optional<Guild> fromJson(const QJsonObject& object)
    {
        Guild guild;

        guild.id = object.value("id").toString().trimmed();
        guild.name = object.value("name").toString().trimmed();

        if (guild.id.isEmpty() || guild.name.isEmpty())
        {
            return std::nullopt;
        }

        return guild;
    }

    QString id;
    QString name;

    bool channelsLoaded = false;

    Channel& getChannel(const QString& id)
    {
        if (channels.contains(id))
        {
            return channels[id];
        }

        qWarning() << Q_FUNC_INFO << "channel with id" << id << "not found";

        static Channel channel;

        return channel;
    }

    const Channel& getChannel(const QString& id) const
    {
        return const_cast<Guild&>(*this).getChannel(id);
    }

    void addChannel(const Channel& channel)
    {
        if (channel.id.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "channel has empty id, name =" << channel.name;
        }

        if (channel.name.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "channel has empty name, id =" << channel.id;
        }

        channels.insert(channel.id, channel);
    }

    const QMap<QString, Channel>& getChannels() const
    {
        return channels;
    }

private:
    QMap<QString, Channel> channels;
};
