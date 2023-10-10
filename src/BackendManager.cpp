#include "BackendManager.h"
#include "secrets.h"
#include "i18n.h"
#include "crypto/obfuscator.h"
#include <QDebug>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QSysInfo>
#include <QCryptographicHash>

namespace
{

BackendManager* instance = nullptr;
static const QDateTime StartTime = QDateTime::currentDateTime();

static QString getMachineHash()
{
    static const QString Data =
        OBFUSCATE(BACKEND_API_HASH_SALT)+
        QSysInfo::machineHostName() +
        QSysInfo::machineUniqueId() +
        QSysInfo::prettyProductName() +
        OBFUSCATE(BACKEND_API_HASH_SALT);

    static const QString Hash = QString::fromLatin1(QCryptographicHash::hash(Data.toUtf8(), QCryptographicHash::Algorithm::Sha256).toBase64());

    return Hash;
}

static QString getSessionHash()
{
    static const QString Data =
        getMachineHash() +
        QSysInfo::bootUniqueId() +
        StartTime.toString(Qt::DateFormat::ISODateWithMs);

    static const QString Hash = QString::fromLatin1(QCryptographicHash::hash(Data.toUtf8(), QCryptographicHash::Algorithm::Sha256).toBase64());

    return Hash;
}

}

BackendManager::BackendManager(QNetworkAccessManager& network_, QObject *parent)
    : QObject{parent}
    , network(network_)
    , startTime(QDateTime::currentDateTime())
{
    instance = this;

    usageDuration.start();
}

BackendManager *BackendManager::getInstance()
{
    return instance;
}

void BackendManager::sendSessionUsage()
{
    const qint64 startedAtMs = startTime.toUTC().toMSecsSinceEpoch();
    const int startedAtOffsetSec = startTime.toLocalTime().offsetFromUtc();

    const qint64 durationMs = usageDuration.elapsed();

    const QJsonObject app(
        {
            { "name", QCoreApplication::applicationName() },
            { "version", QCoreApplication::applicationVersion() },
            { "buildArch", QSysInfo::buildCpuArchitecture() },
            { "buildAbi", QSysInfo::buildAbi() },
            { "locale", I18n::getInstance() ? I18n::getInstance()->language() : QString() },
            { "qtVersion", qVersion() },
            { "webVersion", usedWebVersion },
            { "cefVersion", usedCefVersion },
        });

    const QJsonObject machine(
        {
            { "currentArch", QSysInfo::currentCpuArchitecture() },
            { "productName", QSysInfo::prettyProductName() },
            { "productType", QSysInfo::productType() },
            { "productVersion", QSysInfo::productVersion() },
            { "kernelType", QSysInfo::kernelType() },
            { "kernelVersion", QSysInfo::kernelVersion() },
            { "locale", QLocale::system().name() },
            { "hash", getMachineHash() },
        });

    QJsonArray features;
    for (const QString& feature : qAsConst(usedFeatures))
    {
        features.append(feature);
    }

    const QJsonObject usage =
        {
            { "startedAtMs", startedAtMs },
            { "startedAtOffsetSec", startedAtOffsetSec },
            { "durationMs", durationMs },
            { "app", app },
            { "machine", machine },
            { "features", features },
        };

    const QJsonDocument doc(QJsonObject(
        {
            { "machineHash", getMachineHash() },
            { "sessionHash", getSessionHash() },
            { "usage", usage },
        }));

    QNetworkRequest request(QUrl(OBFUSCATE(BACKEND_API_ROOT_URL) + QString("/usage?secret=") + OBFUSCATE(BACKEND_API_SECRET)));
    request.setRawHeader("Content-Type", "application/json");

    QNetworkReply* reply = network.post(request, doc.toJson(QJsonDocument::JsonFormat::Compact));

    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred), this, [](QNetworkReply::NetworkError error)
    {
        qCritical() << error;
    });
}

void BackendManager::setUsedLanguage(const QString &language)
{
    usedLanguage = language;
}

void BackendManager::setUsedWebEngineVersion(const QString &version, const QString &cefVersion)
{
    usedWebVersion = version;
    usedCefVersion = cefVersion;
}

void BackendManager::addUsedFeature(const QString &feature)
{
    usedFeatures.insert(feature);
}
