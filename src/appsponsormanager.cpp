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

static const QString SponsorsUrl = "https://raw.githubusercontent.com/3dproger/AxelChatRepoServer/main/sponsors.json";
static const QString SupportMethodsUrl = "https://raw.githubusercontent.com/3dproger/AxelChatRepoServer/main/support_methods.json";

};

AppSponsorManager::AppSponsorManager(QNetworkAccessManager &network_, QObject *parent)
    : QObject{parent}
    , network(network_)
{
    requestSponsors();
    requestSupportMethods();
}

void AppSponsorManager::requestSponsors()
{
    QNetworkReply* reply = network.get(QNetworkRequest(SponsorsUrl));
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        reply->deleteLater();

        const QString version = root.value("version").toString();
        if (version != "1.0.0")
        {
            qWarning() << "unsupported version" << version;
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
                qWarning() << "unknown type" << type;
                continue;
            }

            model.add(sponsor);
        }

        model.sortByTier();
    });
}

void AppSponsorManager::requestSupportMethods()
{
    QNetworkReply* reply = network.get(QNetworkRequest(SupportMethodsUrl));
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        reply->deleteLater();

        const QString version = root.value("version").toString();
        if (version != "1.0.0")
        {
            qWarning() << "unsupported version" << version;
        }

        const QJsonArray supportMethodsJson = root.value("support_methods").toArray();
        for (const QJsonValue& v : qAsConst(supportMethodsJson))
        {
            const QJsonObject methodJson = v.toObject();

            AppSupportMethod method;

            method.name = methodJson.value("name").toString();
            method.icon = methodJson.value("icon").toString();
            method.url = methodJson.value("url").toString();

            const QJsonArray possibilities = methodJson.value("possibilities").toArray();
            for (const QJsonValue& v : qAsConst(possibilities))
            {
                const QString type = v.toString();
                if (type == "paid_subscription")
                {
                    method.possibilities.append(AppSupportMethod::Possibility::PaidSubscription);
                }
                else if (type == "donation")
                {
                    method.possibilities.append(AppSupportMethod::Possibility::Donation);
                }
                else
                {
                    qWarning() << "unknown possibility" << type;
                }
            }

            if (!method.possibilities.isEmpty())
            {
                method.mainPossibility = method.possibilities.first();
            }
            else
            {
                qWarning() << "possibilities is empty, data =" << methodJson;
            }

            methods.append(method);
        }

        model.sortByTier();
    });
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

    qWarning() << "unknown role" << role;

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
