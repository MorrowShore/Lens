#pragma once

#include <QNetworkAccessManager>
#include <QAbstractListModel>

class AppSponsor
{
    Q_GADGET
public:
    enum class Role {
        Name = Qt::UserRole + 1,
        TierName,
    };
    Q_ENUM(Role)

    enum class Type {
        Unknown,
        Subscription,
        Donation,
    };

    Type type = Type::Unknown;
    QString name;
    QString tierName;
    QString platform;
};

class AppSupportMethod
{
    Q_GADGET
public:
    enum class Role {
        Name = Qt::UserRole + 1,
        Url,
    };
    Q_ENUM(Role)

    enum class Possibility {
        Unknown,
        PaidSubscription,
        Donation,
    };
    Q_ENUM(Possibility)

    QList<Possibility> possibilities;
    Possibility mainPossibility = Possibility::Unknown;
    QString name;
    QUrl url;
    QString icon;
};

class AppSponsorModel : public QAbstractListModel
{
    Q_OBJECT
public:
    AppSponsorModel(QObject *parent = 0) : QAbstractListModel(parent) {}

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    void add(const AppSponsor& sponsor);
    void sortByTier();

private:
    QList<AppSponsor> sponsors;
};

class AppSponsorManager : public QObject
{
    Q_OBJECT
public:
    AppSponsorModel model;
    QList<AppSupportMethod> methods;

    explicit AppSponsorManager(QNetworkAccessManager& network, QObject *parent = nullptr);

signals:

public slots:
    void requestSponsors();
    void requestSupportMethods();

private:
    QNetworkAccessManager& network;
};
