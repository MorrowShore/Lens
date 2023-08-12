#pragma once

#include "emoteservice.h"
#include <QJsonArray>
#include <QObject>
#include <QHash>

class BetterTTV : public EmoteService
{
    Q_OBJECT
public:
    explicit BetterTTV(QNetworkAccessManager& network, QObject *parent = nullptr);

    QString findEmoteUrl(const QString &name) const override;
    void loadGlobal() override;
    void loadChannel(const QString &id) override;
    bool isLoadedGlobal() const override { return loadedGlobal; }
    bool isLoadedChannel() const override { return loadedChannel; }

private:
    static QHash<QString, QString> parseArray(const QJsonArray& array);

    QNetworkAccessManager& network;

    bool loadedGlobal = false;
    QHash<QString, QString> global;

    bool loadedChannel = false;
    QHash<QString, QString> channel;
};
