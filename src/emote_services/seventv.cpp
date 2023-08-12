#include "seventv.h"
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

SevenTV::SevenTV(QNetworkAccessManager& network_, QObject *parent)
    : EmoteService{parent}
    , network(network_)
{

}

QString SevenTV::findEmoteUrl(const QString &name) const
{
    if (auto it = global.find(name); it != global.end())
    {
        const QString& url = it.value();
        if (url.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "name is empty for global emote" << name;
        }
        else
        {
            return url;
        }
    }

    if (auto it = channel.find(name); it != channel.end())
    {
        const QString& url = it.value();
        if (url.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "name is empty for global emote" << name;
        }
        else
        {
            return url;
        }
    }

    return QString();
}

void SevenTV::loadGlobal()
{
    loadedGlobal = false;

    QNetworkRequest request(QUrl("https://api.7tv.app/v3/emote-sets/global"));

    QNetworkReply* reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        loadedGlobal = true;
        global.insert(parseSet(QJsonDocument::fromJson(reply->readAll()).object()));
        reply->deleteLater();
    });
}

void SevenTV::loadChannel(const QString &id)
{
    loadedGlobal = false;

    QNetworkRequest request(QUrl("https://api.7tv.app/v3/users/twitch/" + id));

    QNetworkReply* reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        loadedGlobal = true;
        channel.insert(parseSet(QJsonDocument::fromJson(reply->readAll()).object().value("emote_set").toObject()));
        reply->deleteLater();
    });
}

QHash<QString, QString> SevenTV::parseSet(const QJsonObject &set)
{
    QHash<QString, QString> result;

    const QJsonArray emotes = set.value("emotes").toArray();

    for (const QJsonValue& v : qAsConst(emotes))
    {
        const QJsonObject emote = v.toObject();

        const QString name = emote.value("name").toString();
        const QString url = "https:" + emote.value("data").toObject().value("host").toObject().value("url").toString() + "/2x.webp";

        result.insert(name, url);
    }

    return result;
}
