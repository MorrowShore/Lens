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
#include <QRandomGenerator>

namespace
{

BackendManager* instance = nullptr;
static const QDateTime StartTime = QDateTime::currentDateTime();

static QString generateRandomString(const int length)
{
    static const QString PossibleCharacters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

    QString result;
    for(int i = 0; i < length; ++i)
    {
        int index = QRandomGenerator::securelySeeded().generate() % PossibleCharacters.length();

        result.append(PossibleCharacters.at(index));
    }

    return result;
}

static QString generateDisposableHash()
{
    const QString data =
        OBFUSCATE(BACKEND_API_HASH_SALT) +
        generateRandomString(10).toUtf8() +
        QCoreApplication::applicationName() +
        QCoreApplication::applicationVersion() +
        QDateTime::currentDateTime().toString().toUtf8() +
        OBFUSCATE(BACKEND_API_HASH_SALT);

    const QString hash = QString::fromLatin1(QCryptographicHash::hash(data.toUtf8(), QCryptographicHash::Algorithm::Sha256).toBase64());

    return hash;
}

}

BackendManager::BackendManager(QSettings& settings, const QString& settingsGroupPathParent, QNetworkAccessManager& network_, QObject *parent)
    : QObject{parent}
    , network(network_)
    , instanceHash(settings, settingsGroupPathParent + "/instanceHash", QString(), true)
    , startTime(QDateTime::currentDateTime())
{
    instance = this;

    if (instanceHash.get().isEmpty())
    {
        instanceHash.set(generateDisposableHash());
    }

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
            { "features", features },
        };

    const QJsonDocument doc(QJsonObject(
        {
            { "instanceHash", instanceHash.get() },
            { "usage", usage },
            { "app", app },
            { "machine", machine },
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
