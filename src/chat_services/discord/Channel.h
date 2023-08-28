#pragma once

#include <QJsonObject>

struct Channel
{
    static std::optional<Channel> fromJson(const QJsonObject& object)
    {
        Channel channel;

        channel.id = object.value("id").toString().trimmed();
        channel.name = object.value("name").toString().trimmed();
        channel.nsfw = object.value("nsfw").toBool(channel.nsfw);

        if (channel.id.isEmpty() || channel.name.isEmpty())
        {
            return std::nullopt;
        }

        return channel;
    }

    QString id;
    QString name;
    bool nsfw = false;
};
