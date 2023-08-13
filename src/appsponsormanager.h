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

    explicit AppSponsorManager(QNetworkAccessManager& network, QObject *parent = nullptr);

signals:

public slots:
    void requestSponsors();

private:
    QNetworkAccessManager& network;
};
