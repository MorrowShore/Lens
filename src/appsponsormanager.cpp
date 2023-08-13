#include "appsponsormanager.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace
{

const QHash<int, QByteArray> RoleNames = QHash<int, QByteArray>
{
    {(int)AppSponsor::Role::Name,       "name"},
    {(int)AppSponsor::Role::TierName,   "tierName"},
};

static const QString GitHubUrlSponsors = "https://raw.githubusercontent.com/3dproger/AxelChatSponsors/main/index.json";

};

AppSponsorManager::AppSponsorManager(QNetworkAccessManager &network_, QObject *parent)
    : QObject{parent}
    , network(network_)
{
    requestSponsors();
}

void AppSponsorManager::requestSponsors()
{
    QNetworkReply* reply = network.get(QNetworkRequest(GitHubUrlSponsors));
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        reply->deleteLater();

        const QString version = root.value("version").toString();
        if (version != "1.0.0")
        {
            qWarning() << Q_FUNC_INFO << "unsupported version" << version;
        }

        const QJsonArray sponsorsJson = root.value("sponsors").toArray();
        for (const QJsonValue& v : qAsConst(sponsorsJson))
        {
            const QJsonObject sponsorJson = v.toObject();

            AppSponsor sponsor;

            sponsor.name = sponsorJson.value("name").toString();
            sponsor.platform = sponsorJson.value("platform").toString();

            const QString type = sponsorJson.value("type").toString();
            if (type == "subscription")
            {
                sponsor.type = AppSponsor::Type::Subscription;

                const QJsonObject tier = sponsorJson.value("tier").toObject();
                sponsor.tierName = tier.value("name").toString();
            }
            else if (type == "donation")
            {
                sponsor.type = AppSponsor::Type::Donation;

                const QJsonObject tier = sponsorJson.value("tier").toObject();
                sponsor.tierName = tr("Donator");
            }
            else
            {
                qWarning() << Q_FUNC_INFO << "unknown type" << type;
                continue;
            }

            model.add(sponsor);
        }

        model.sortByTier();
    });

    /*//TODO

    AppSponsor sponsor;

    sponsor = AppSponsor();
    sponsor.name = "AXEL";
    sponsor.tierName = "SUPER";
    model.add(sponsor);

    sponsor = AppSponsor();
    sponsor.name = "AXEL2";
    sponsor.tierName = "SUPER2";
    model.add(sponsor);

    sponsor = AppSponsor();
    sponsor.name = "AXEL3";
    sponsor.tierName = "SUPER3";
    model.add(sponsor);

    model.sortByTier();*/
}

QHash<int, QByteArray> AppSponsorModel::roleNames() const
{
    return RoleNames;
}

int AppSponsorModel::rowCount(const QModelIndex &) const
{
    return sponsors.count();
}

QVariant AppSponsorModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (index.row() >= sponsors.size())
    {
        return QVariant();
    }

    const AppSponsor& sponsor = sponsors.value(index.row());
    switch ((AppSponsor::Role)role)
    {
    case AppSponsor::Role::Name:
        return sponsor.name;
    case AppSponsor::Role::TierName:
        return sponsor.tierName;
    }

    qWarning() << Q_FUNC_INFO << "unknown role" << role;

    return QVariant();
}

void AppSponsorModel::add(const AppSponsor &sponsor)
{
    beginInsertRows(QModelIndex(), sponsors.count(), sponsors.count());

    sponsors.append(sponsor);

    endInsertRows();
}

void AppSponsorModel::sortByTier()
{
    //TODO

    emit dataChanged(createIndex(0, 0), createIndex(sponsors.count() - 1, 0));
}
