#pragma once

#include "emoteservice.h"
#include <QObject>
#include <QHash>

class FrankerFaceZ : public EmoteService
{
    Q_OBJECT
public:
    explicit FrankerFaceZ(QNetworkAccessManager& network, QObject *parent = nullptr);

    QString findEmoteUrl(const QString &name) const override;
    void loadGlobal() override;
    void loadChannel(const QString &id) override;
    bool isLoadedGlobal() const override { return loadedGlobal; }
    bool isLoadedChannel() const override { return loadedChannel; }

private:
    static QHash<QString, QString> parseSet(const QJsonObject& set);

    QNetworkAccessManager& network;

    bool loadedGlobal = false;
    QHash<QString, QString> global;

    bool loadedChannel = false;
    QHash<QString, QString> channel;
};
