#pragma once

#include <QJsonObject>

class Channel
{
public:
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

    QString getId() const { return id; }
    QString getName() const { return name; }
    bool isNsfw() const { return nsfw; }

private:
    QString id;
    QString name;
    bool nsfw = false;
};
