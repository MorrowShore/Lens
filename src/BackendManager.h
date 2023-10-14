#pragma once

#include "setting.h"
#include "chat_services/chatservice.h"
#include <QNetworkAccessManager>
#include <QElapsedTimer>

class BackendManager : public QObject
{
    Q_OBJECT
public:
    explicit BackendManager(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network, QObject *parent = nullptr);

    static BackendManager* getInstance();

public slots:
    void sendService(const ChatService& service);
    void sendSessionUsage();

    void setUsedLanguage(const QString& language);
    void setUsedWebEngineVersion(const QString& version, const QString& cefVersion);
    void addUsedFeature(const QString& feature);

private:
    QJsonObject getJsonMachine() const;
    QJsonObject getJsonApp() const;
    static QJsonObject getJsonServiceState(const ChatService::State &state);
    static QJsonObject getJsonService(const ChatService& service);

    QNetworkAccessManager& network;
    Setting<QString> instanceHash;

    QElapsedTimer usageDuration;
    const QDateTime startTime;

    QSet<QString> usedFeatures;
    QString usedLanguage;
    QString usedWebVersion;
    QString usedCefVersion;
};
