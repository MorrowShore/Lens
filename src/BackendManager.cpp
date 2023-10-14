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

static const int TimerCanSendUsageInterval = 30 * 1000;
static const int TimerSendServicesInterval = 60 * 1000;

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

    timerCanSendUsage.setSingleShot(true);
    timerCanSendUsage.setInterval(TimerCanSendUsageInterval);
    timerCanSendUsage.start();

    connect(&timerSendServices, &QTimer::timeout, this, &BackendManager::sendServices);
}

BackendManager *BackendManager::getInstance()
{
    return instance;
}

void BackendManager::sendServices()
{
    QJsonArray jsonServices;
    for (const QJsonObject& jsonService : qAsConst(services))
    {
        jsonServices.append(jsonService);
    }

    const QJsonDocument doc(QJsonObject(
        {
            { "instanceHash", instanceHash.get() },
            { "services", jsonServices },
            { "app", getJsonApp() },
            { "machine", getJsonMachine() },
        }));

    QNetworkRequest request(QUrl(OBFUSCATE(BACKEND_API_ROOT_URL) + QString("/services?secret=") + OBFUSCATE(BACKEND_API_SECRET)));
    request.setRawHeader("Content-Type", "application/json");

    QNetworkReply* reply = network.post(request, doc.toJson(QJsonDocument::JsonFormat::Compact));

    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred), this, [](QNetworkReply::NetworkError error)
    {
        qCritical() << error;
    });
}

void BackendManager::sendSessionUsage()
{
    if (timerCanSendUsage.isActive())
    {
        qDebug() << "timer CanSendUsage is active, ignore";
        return;
    }

    const qint64 startedAtMs = startTime.toUTC().toMSecsSinceEpoch();
    const int startedAtOffsetSec = startTime.toLocalTime().offsetFromUtc();

    const qint64 durationMs = usageDuration.elapsed();

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
            { "app", getJsonApp() },
            { "machine", getJsonMachine() },
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

void BackendManager::setService(const ChatService &service)
{
    const ChatService::State& state = service.getState();

    const QJsonObject jsonState = QJsonObject(
        {
            { "connected", state.connected },
            { "streamUrl", state.streamUrl.toString() },
            { "viewers", state.viewers },
        });

    const QString typeId = ChatService::getServiceTypeId(service.getServiceType());

    const QJsonObject jsonService(
        {
            { "name", service.getName() },
            { "typeId", typeId },
            { "state", jsonState },
        });

    if (!services.contains(typeId))
    {
        timerSendServices.stop();
        timerSendServices.setSingleShot(true);
        timerSendServices.setInterval(TimerSendServicesInterval);
        timerSendServices.start();
    }

    services.insert(typeId, jsonService);
}

void BackendManager::onBeforeQuit()
{
    sendSessionUsage();
}

QJsonObject BackendManager::getJsonMachine() const
{
    return QJsonObject(
        {
            { "currentArch", QSysInfo::currentCpuArchitecture() },
            { "productName", QSysInfo::prettyProductName() },
            { "productType", QSysInfo::productType() },
            { "productVersion", QSysInfo::productVersion() },
            { "kernelType", QSysInfo::kernelType() },
            { "kernelVersion", QSysInfo::kernelVersion() },
            { "locale", QLocale::system().name() },
        });
}

QJsonObject BackendManager::getJsonApp() const
{
    return QJsonObject(
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
}
