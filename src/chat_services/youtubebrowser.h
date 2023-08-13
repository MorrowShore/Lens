#pragma once

#include "Manager.h"
#include <QNetworkAccessManager>
#include <QTimer>
#include <memory>

class YouTubeBrowser : public QObject
{
    Q_OBJECT
public:
    explicit YouTubeBrowser(cweqt::Manager& web, QObject *parent = nullptr);

signals:
    void actionsReceived(const QJsonArray& array, const QByteArray& data);

public slots:
    void openWindow();
    void reconnect(const QUrl& chatUrl);

private:
    cweqt::Manager& web;
    std::shared_ptr<cweqt::Browser> browser;
};
