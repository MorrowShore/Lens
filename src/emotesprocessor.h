#pragma once

#include "models/message.h"
#include <QNetworkAccessManager>
#include <QSettings>
#include <QTimer>

class EmotesProcessor : public QObject
{
    Q_OBJECT
public:
    explicit EmotesProcessor(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, QObject *parent = nullptr);
    void processMessage(std::shared_ptr<Message> message);

signals:

private slots:
    void loadEmotes();

private:
    QString getEmoteUrl(const QString& name) const;

    QSettings& settings;
    const QString settingsGroupPath;
    QNetworkAccessManager& network;

    bool needLoadEmotes = true;

    QTimer timer;

    QHash<QString, QString> betterTTVEmotes; // <code, id>
};
