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
    void loadAll();

    void loadBttvGlobalEmotes();

    void loadFfzGlobalEmotes();
    void parseFfzSet(const QJsonObject& set);

    void loadSevenTvEmotes();

private:
    QString getEmoteUrl(const QString& name) const;

    QSettings& settings;
    const QString settingsGroupPath;
    QNetworkAccessManager& network;

    QTimer timer;

    QHash<QString, QString> bttvEmotes; // <name, url>
    QHash<QString, QString> ffzEmotes; // <name, url>
    QHash<QString, QString> sevenTvEmotes; // <name, url>
};
