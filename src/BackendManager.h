#pragma once

#include <QNetworkAccessManager>
#include <QElapsedTimer>

class BackendManager : public QObject
{
    Q_OBJECT
public:
    explicit BackendManager(QNetworkAccessManager& network, QObject *parent = nullptr);

    static BackendManager* getInstance();

public slots:
    void sendSessionUsage();

    void setUsedLanguage(const QString& language);
    void setUsedWebEngineVersion(const QString& version, const QString& cefVersion);
    void addUsedFeature(const QString& feature);

private:
    QNetworkAccessManager& network;

    QElapsedTimer usageDuration;
    const QDateTime startTime;

    QSet<QString> usedFeatures;
    QString usedLanguage;
    QString usedWebVersion;
    QString usedCefVersion;
};
