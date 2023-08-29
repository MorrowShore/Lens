#pragma once

#include <QJsonObject>

struct User
{
    static User fromJson(const QJsonObject& json)
    {
        User user;

        user.avatarHash = json.value("avatar").toString();
        user.discriminator = json.value("discriminator").toString();
        user.globalName = json.value("global_name").toString();
        user.id = json.value("id").toString();
        user.username = json.value("username").toString();
        user.bot = json.value("bot").toBool();

        return user;
    }

    static bool isValidDiscriminator(const QString& discriminator)
    {
        return !discriminator.isEmpty() && discriminator != "0";
    }

    QString avatarHash;
    QString discriminator;
    QString globalName;
    QString id;
    QString username;
    bool bot = false;

    QString getDisplayName(const bool includeDiscriminatorIfNeedAndValid) const
    {
        if (globalName.isEmpty())
        {
            QString name = username;
            if (includeDiscriminatorIfNeedAndValid && isValidDiscriminator(discriminator))
            {
                name += "#" + discriminator;
            }

            return name;
        }

        return globalName;
    }
};
