#pragma once

#include <QNetworkAccessManager>

class EmoteService : public QObject
{
    Q_OBJECT

public:
    explicit EmoteService(QObject *parent = nullptr)
        : QObject(parent)
    {}

    virtual ~EmoteService(){}

    virtual QString findEmoteUrl(const QString& name) const = 0;

    virtual void loadGlobal() = 0;
    virtual void loadChannel(const QString& id) = 0;

    virtual bool isLoadedGlobal() const = 0;
    virtual bool isLoadedChannel() const = 0;
};
