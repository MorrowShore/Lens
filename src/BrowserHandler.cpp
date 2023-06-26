#include "BrowserHandler.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QDebug>

namespace
{

static const QString ExecutableRelativePath = "WebInterceptor/WebInterceptor.exe";
static const QStringList AvailableResourceTypes =
{
    "MAIN_FRAME",
    "SUB_FRAME",
    "STYLESHEET",
    "SCRIPT",
    "IMAGE",
    "FONT_RESOURCE",
    "SUB_RESOURCE",
    "MEDIA",
    "WORKER",
    "SHARED_WORKER",
    "PREFETCH",
    "FAVICON",
    "XHR",
    "PING",
    "SERVICE_WORKER",
    "CSP_REPORT",
    "PLUGIN_RESOURCE",
    "NAVIGATION_PRELOAD_MAIN_FRAME",
    "NAVIGATION_PRELOAD_SUB_FRAME",
};

}

const QStringList &BrowserHandler::getAvailableResourceTypes()
{
    return AvailableResourceTypes;
}

bool BrowserHandler::checkExecutableExists()
{
    return QFileInfo::exists(getExecutablePath());
}

QString BrowserHandler::getExecutablePath()
{
    return qApp->applicationDirPath() + "/" + ExecutableRelativePath;
}

BrowserHandler::BrowserHandler(QObject *parent)
    : QObject{parent}
{
    timeoutTimer.setSingleShot(true);
    connect(&timeoutTimer, &QTimer::timeout, this, [this]()
            {
                qWarning() << Q_FUNC_INFO << "stop on timeout";
                stop();
            });
}

void BrowserHandler::start(const CommandLineParameters& parameters, int timeout)
{
    stop();

    if (!checkExecutableExists())
    {
        qWarning() << Q_FUNC_INFO << "executable" << getExecutablePath() << "not exists";
        return;
    }

    process = new QProcess(this);

    connect(process, &QProcess::readyRead, this, &BrowserHandler::onReadyRead);
    connect(process, &QProcess::stateChanged, this, [this](const QProcess::ProcessState state)
    {
        if (state == QProcess::ProcessState::NotRunning)
        {
            emit processClosed();
        }
    });

    const FilterSettings& filterSettings = parameters.filterSettings;

    const QString program = getExecutablePath();

    QStringList arguments;

    arguments.append("--url=" + parameters.url.toString() + "");

    if (parameters.windowVisible)
    {
        arguments.append("--window-visible=true");
    }
    else
    {
        arguments.append("--window-visible=false");
    }

    if (parameters.showResponses)
    {
        arguments.append("--show-responses=true");
    }
    else
    {
        arguments.append("--show-responses=false");
    }

    if (!filterSettings.resourceTypes.empty())
    {
        QString arg = "--interceptor-filter-resource-types=";

        const QList<QString> values = filterSettings.resourceTypes.values();

        for (int i = 0; i < values.count(); i++)
        {
            const QString value = values[i];
            arg += value;
            if (i < values.count() - 1)
            {
                arg += ",";
            }
        }

        arguments.append(arg);
    }

    if (!filterSettings.methods.empty())
    {
        QString arg = "--interceptor-filter-methods=";

        const QList<QString> values = filterSettings.methods.values();

        for (int i = 0; i < values.count(); i++)
        {
            const QString value = values[i];
            arg += value;
            if (i < values.count() - 1)
            {
                arg += ",";
            }
        }

        arguments.append(arg);
    }

    if (!filterSettings.mimeTypes.empty())
    {
        QString arg = "--interceptor-filter-mime-types=";

        const QList<QString> values = filterSettings.mimeTypes.values();

        for (int i = 0; i < values.count(); i++)
        {
            const QString value = values[i];
            arg += value;
            if (i < values.count() - 1)
            {
                arg += ",";
            }
        }

        arguments.append(arg);
    }

    if (!filterSettings.urlPrefixes.empty())
    {
        QString arg = "--interceptor-filter-url-prefixes=";

        const QList<QString> values = filterSettings.urlPrefixes.values();

        for (int i = 0; i < values.count(); i++)
        {
            const QString value = values[i];
            arg += value;
            if (i < values.count() - 1)
            {
                arg += ",";
            }
        }

        arguments.append(arg);
    }

    //qDebug() << "start program:" << program << ", arguments:" << arguments;

    process->start(program, arguments);

    timeoutTimer.start(timeout);
}

void BrowserHandler::stop()
{
    if (process)
    {
        process->terminate();
        process = nullptr;
    }

    responses.clear();

    timeoutTimer.stop();
}

void BrowserHandler::onReadyRead()
{
    if (!process)
    {
        return;
    }

    while (process->canReadLine())
    {
        parseLine(process->readLine());

        if (!process)
        {
            break;
        }
    }
}

void BrowserHandler::parseLine(const QByteArray &line)
{
    if (line.isEmpty())
    {
        return;
    }

    //>messageType:property1=value1;property2=value2;:dataSize:data

    static const char Prefix = '>';
    static const char SectionsSeparator = ':';
    static const char PropertyEnd = ';';
    static const char PropertyValueSeparator = '=';
    static const int MaxHeaderLength = 1024;
    static const int SectionSeparatorsCount = 3;

    if (line[0] != Prefix)
    {
        return;
    }

    QList<int> sectionSeparatorsPos;
    for (int i = 1; i < std::min(MaxHeaderLength, line.length()); i++)
    {
        if (line[i] == SectionsSeparator)
        {
            sectionSeparatorsPos.append(i);

            if (sectionSeparatorsPos.size() == SectionSeparatorsCount)
            {
                break;
            }
        }
    }

    const int sectionSeparatorsCount = sectionSeparatorsPos.count();
    if (sectionSeparatorsCount != SectionSeparatorsCount)
    {
        qWarning() << Q_FUNC_INFO << "wrong section separators count, found =" << sectionSeparatorsCount << ", expected =" << SectionSeparatorsCount;
        return;
    }

    const QByteArray messageType = line.mid(1, sectionSeparatorsPos[0] - 1);

    QMap<QByteArray, QByteArray> properties;
    const QByteArray propertiesRaw = line.mid(sectionSeparatorsPos[0] + 1, (sectionSeparatorsPos[1] - sectionSeparatorsPos[0]) - 1);
    const QList<QByteArray> propertiesSplited = propertiesRaw.split(PropertyEnd);
    for (QByteArray property : qAsConst(propertiesSplited))
    {
        property = property.trimmed();
        if (property.isEmpty())
        {
            continue;
        }

        if (!property.contains(PropertyValueSeparator))
        {
            qWarning() << Q_FUNC_INFO << "property" << PropertyValueSeparator << "not contains" << PropertyValueSeparator;
            return;
        }

        const int separatorPos = property.indexOf(PropertyValueSeparator);
        const QByteArray name = property.left(separatorPos).trimmed();
        const QByteArray value = property.mid(separatorPos + 1).trimmed();

        properties[name] = value;
    }

    const QByteArray dataSizeRaw = line.mid(sectionSeparatorsPos[1] + 1, (sectionSeparatorsPos[2] - sectionSeparatorsPos[1]) - 1);
    bool ok = false;
    const int dataSize = dataSizeRaw.toInt(&ok);
    if (!ok)
    {
        qWarning() << Q_FUNC_INFO << "failed to convert data size" << dataSizeRaw << "to integer";
        return;
    }

    const int startDataPos = sectionSeparatorsPos[2] + 1;
    const QByteArray data = line.mid(startDataPos, dataSize);

    parse(messageType, properties, data);
}

void BrowserHandler::parse(const QByteArray &messageType, const QMap<QByteArray, QByteArray> &properties, const QByteArray &data)
{
    if (messageType == "resd")
    {
        bool ok = false;
        const uint64_t requestId = properties.value("i").toULongLong(&ok);
        if (!ok)
        {
            qWarning() << Q_FUNC_INFO << messageType << "unknown request id";
            return;
        }

        if (responses.find(requestId) == responses.end())
        {
            qWarning() << Q_FUNC_INFO << messageType << "request id" << requestId << "not registered";
            return;
        }

        Response& response = responses[requestId];
        response.data += QByteArray::fromBase64(data);
    }
    else if (messageType == "ress")
    {
        Response response;

        bool ok = false;
        response.requestId = properties.value("i").toULongLong(&ok);
        if (!ok)
        {
            qWarning() << Q_FUNC_INFO << messageType << "failed to convert request id to integer";
            return;
        }

        ok = false;
        response.status = properties.value("st").toInt(&ok);
        if (!ok)
        {
            qWarning() << Q_FUNC_INFO << messageType << "failed to convert status to integer";
        }

        response.method = QString::fromUtf8(properties.value("mthd"));
        response.resourceType = QString::fromUtf8(properties.value("rtype"));
        response.mimeType = QString::fromUtf8(properties.value("mime"));
        response.url = QUrl(QString::fromUtf8(data));

        responses[response.requestId] = std::move(response);
    }
    else if (messageType == "rese")
    {
        bool ok = false;
        const uint64_t requestId = properties.value("i").toULongLong(&ok);
        if (!ok)
        {
            qWarning() << Q_FUNC_INFO << messageType << "failed to convert request id to integer";
            return;
        }

        auto it = responses.find(requestId);
        if (it == responses.end())
        {
            qWarning() << Q_FUNC_INFO << messageType << "request id" << requestId << "not registered";
            return;
        }

        emit responsed(it->second);
        responses.erase(it);
    }
    else if (messageType == "wndo")
    {
        bool ok = false;
        const uint64_t handle = properties.value("h").toULongLong(&ok);
        if (!ok)
        {
            qWarning() << Q_FUNC_INFO << messageType << "failed to convert request id to integer";
            return;
        }

        emit windowCreated(QWindow::fromWinId(handle));
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "unknown message type" << messageType;
    }
}
