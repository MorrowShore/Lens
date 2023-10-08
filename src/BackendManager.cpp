#include "BackendManager.h"
#include "secrets.h"
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

static QJsonObject getSessionInfo()
{
    const QJsonObject app(
        {
            { "name", QCoreApplication::applicationName() },
            { "version", QCoreApplication::applicationVersion() },
            { "buildArch", QSysInfo::buildCpuArchitecture() },
            { "buildAbi", QSysInfo::buildAbi() },
        });

    const QJsonObject machine(
        {
            { "currentArch", QSysInfo::currentCpuArchitecture() },
            { "productName", QSysInfo::prettyProductName() },
            { "productType", QSysInfo::productType() },
            { "productVersion", QSysInfo::productVersion() },
            { "kernelType", QSysInfo::kernelType() },
            { "kernelVersion", QSysInfo::kernelVersion() },
            { "hash", getMachineHash() },
        });

    return QJsonObject(
        {
            { "app", app },
            { "machine", machine},
            { "hash", getSessionHash() },
        });
}

}

BackendManager::BackendManager(QNetworkAccessManager& network_, QObject *parent)
    : QObject{parent}
    , network(network_)
{

}

void BackendManager::sendStarted()
{
    sendEvent("SESSION_STARTED", getSessionInfo());
}

void BackendManager::sendEvent(const QString &type, const QJsonValue &data)
{
    const QJsonDocument doc(QJsonObject(
        {
            { "session", getSessionHash() },
            { "type", type },
            { "data", data },
        }));

    QNetworkRequest request(QUrl(OBFUSCATE(BACKEND_API_ROOT_URL) + QString("/events")));
    request.setRawHeader("Content-Type", "application/json");

    QNetworkReply* reply = network.post(request, doc.toJson(QJsonDocument::JsonFormat::Compact));

    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred), this, [type](QNetworkReply::NetworkError error)
    {
        qCritical() << error << ", type =" << type;
    });
}
