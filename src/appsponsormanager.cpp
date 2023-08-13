#include "appsponsormanager.h"

namespace
{

const QHash<int, QByteArray> RoleNames = QHash<int, QByteArray>
{
    {(int)AppSponsor::Role::Name,   "name"},
    {(int)AppSponsor::Role::Tier,   "tier"},
};

};

AppSponsorManager::AppSponsorManager(QNetworkAccessManager &network_, QObject *parent)
    : QObject{parent}
    , network(network_)
{
    requestSponsors();
}

void AppSponsorManager::requestSponsors()
{
    //TODO

    AppSponsor sponsor;

    sponsor = AppSponsor();
    sponsor.name = "AXEL";
    sponsor.tier = "SUPER";
    model.add(sponsor);

    sponsor = AppSponsor();
    sponsor.name = "AXEL2";
    sponsor.tier = "SUPER2";
    model.add(sponsor);

    sponsor = AppSponsor();
    sponsor.name = "AXEL3";
    sponsor.tier = "SUPER3";
    model.add(sponsor);

    model.sortByTier();
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
    case AppSponsor::Role::Tier:
        return sponsor.tier;
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
