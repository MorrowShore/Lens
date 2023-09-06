#include "Manager.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QDebug>

namespace
{

static const QStringList SupportedVersions =
{
    "1.1.0",
};

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

static const QChar ParameterValueSeparator = ';';

static QString boolToValue(const bool value)
{
    return value ? "true" : "false";
}

static bool getParamStr(const QMap<QString, QString>& parameters, const QString& name, QString& value)
{
    if (!parameters.contains(name))
    {
        qWarning() << "not found parameter" << name;
        return false;
    }

    value = parameters.value(name);
    return true;
}

static bool getParamInt64(const QMap<QString, QString>& parameters, const QString& name, int64_t& value)
{
    if (!parameters.contains(name))
    {
        qWarning() << "not found parameter" << name;
        return false;
    }

    const QString str = parameters.value(name);

    bool ok = false;
    const auto tmp = str.toLongLong(&ok);
    if (!ok)
    {
        qWarning() << "failed to convert parameter" << name << "=" << str << "to integer";
        return false;
    }

    value = tmp;

    return true;
}

static bool getParamUInt64(const QMap<QString, QString>& parameters, const QString& name, uint64_t& value)
{
    if (!parameters.contains(name))
    {
        qWarning() << "not found parameter" << name;
        return false;
    }

    const QString str = parameters.value(name);

    bool ok = false;
    const auto tmp = str.toULongLong(&ok);
    if (!ok)
    {
        qWarning() << "failed to convert parameter" << name << "=" << str << "to integer";
        return false;
    }

    value = tmp;

    return true;
}

static bool getParamInt(const QMap<QString, QString>& parameters, const QString& name, int& value)
{
    if (!parameters.contains(name))
    {
        qWarning() << "not found parameter" << name;
        return false;
    }

    const QString str = parameters.value(name);

    bool ok = false;
    const auto tmp = str.toInt(&ok);
    if (!ok)
    {
        qWarning() << "failed to convert parameter" << name << "=" << str << "to integer";
        return false;
    }

    value = tmp;

    return true;
}

static QString convertToParameterValue(const QSet<QString>& set)
{
    QString result;

    for (const QString& value : qAsConst(set))
    {
        if (!result.isEmpty())
        {
            result += ParameterValueSeparator;
        }

        result += value;
    }

    return result;
}

static QString convertToParameterValue(const QSet<int>& set)
{
    QString result;

    for (const int value : qAsConst(set))
    {
        if (!result.isEmpty())
        {
            result += ParameterValueSeparator;
        }

        result += QString("%1").arg(value);
    }

    return result;
}

}

namespace cweqt
{

const QStringList &Manager::getAvailableResourceTypes()
{
    return AvailableResourceTypes;
}

bool Manager::isExecutableExists() const
{
    return QFileInfo::exists(executablePath);
}

Manager::Manager(const QString& executablePath_, QObject *parent)
    : QObject{parent}
    , executablePath(executablePath_)
{
    connect(&messanger, QOverload<const Messanger::Message&>::of(&Messanger::messageReceived), this, [this](const Messanger::Message& message)
        {
            const QString& type = message.getType();
            const QMap<QString, QString>& params = message.getParameters();
            const QByteArray& data = message.getData();
            const int64_t dataSize = message.getDataSize();

            if (type == "resd")
            {
                uint64_t responseId = 0;
                if (!getParamUInt64(params, "i", responseId))
                {
                    return;
                }

                int browserId = 0;
                if (!getParamInt(params, "bi", browserId))
                {
                    return;
                }

                if (std::shared_ptr<Browser> browser = storage.findByBrowserId(browserId); browser)
                {
                    browser->addResponseData(responseId, data, dataSize);
                }
                else
                {
                    qWarning() << "browser id" << browserId << "not found, message type =" << type;
                }
            }
            else if (type == "ress")
            {
                std::shared_ptr<Response> response = std::make_shared<Response>();

                if (!getParamInt(params, "browser-id", response->browserId))
                {
                    return;
                }

                if (!getParamUInt64(params, "i", response->requestId))
                {
                    return;
                }

                if (!getParamStr(params, "method", response->method))
                {
                    return;
                }

                if (!getParamStr(params, "mime-type", response->mimeType))
                {
                    return;
                }

                if (!getParamStr(params, "resource-type", response->resourceType))
                {
                    return;
                }

                if (!getParamInt(params, "status", response->status))
                {
                    return;
                }

                QString url;
                if (!getParamStr(params, "url", url))
                {
                    return;
                }

                response->url = url;

                if (std::shared_ptr<Browser> browser = storage.findByBrowserId(response->browserId); browser)
                {
                    browser->registerResponse(response);
                }
                else
                {
                    qWarning() << "browser id" << response->browserId << "not found, message type =" << type;
                }
            }
            else if (type == "rese")
            {
                uint64_t responseId = 0;
                if (!getParamUInt64(params, "i", responseId))
                {
                    return;
                }

                int browserId = 0;
                if (!getParamInt(params, "bi", browserId))
                {
                    return;
                }

                if (std::shared_ptr<Browser> browser = storage.findByBrowserId(browserId); browser)
                {
                    browser->finalizeResponse(responseId);
                }
                else
                {
                    qWarning() << "browser id" << browserId << "not found, message type =" << type;
                }
            }
            else if (type == "log")
            {
                QString text;
                if (!getParamStr(params, "text", text))
                {
                    return;
                }

                qInfo() << "WebEngine.log:";
                const QStringList lines = text.split("\\n");
                for (const QString& line : qAsConst(lines))
                {
                    qInfo().noquote() << line;
                }
            }
            else if (type == "browser-opened")
            {
                QString url;
                if (!getParamStr(params, "url", url))
                {
                    return;
                }

                int64_t messageId = 0;
                if (!getParamInt64(params, "message-id", messageId))
                {
                    return;
                }

                int browserId = 0;
                if (!getParamInt(params, "browser-id", browserId))
                {
                    return;
                }

                uint64_t windowHanle = 0;
                if (!getParamUInt64(params, "window-handle", windowHanle))
                {
                    return;
                }

                if (std::shared_ptr<Browser> openingBrowser = storage.findByMessageId(messageId); openingBrowser)
                {
                    openingBrowser->setWindowHandle(windowHanle);
                    openingBrowser->setOpened(browserId);

                    storage.moveFromMessageIdToBrowserId(messageId, browserId);

                    emit browserOpened(openingBrowser);
                }
                else
                {
                    std::shared_ptr<Browser> newBrowser(new Browser(*this, browserId, url, true));

                    storage.addWithBrowserId(newBrowser, browserId);

                    emit browserOpened(newBrowser);
                }
            }
            else if (type == "initialized")
            {
                QString version;
                getParamStr(params, "version", version);

                if (!SupportedVersions.contains(version))
                {
                    qWarning() << "Unsupported version" << version << ", supported versions =" << SupportedVersions;
                }

                _initialized = true;
                emit initialized();
            }
            else if (type == "browser-closed")
            {
                int browserId = 0;
                if (!getParamInt(params, "id", browserId))
                {
                    return;
                }

                if (std::shared_ptr<Browser> browser = storage.findByBrowserId(browserId); browser)
                {
                    browser->setClosed();
                    storage.removeByBrowserId(browserId);
                }
                else
                {
                    qWarning() << "unknown browser id" << browserId;
                }
            }
            else if (type == "exited")
            {
                qWarning() << type << "recevied";
            }
            else
            {
                qWarning() << "unknown message type" << type;
            }
        }
    );

    connect(&timerPing, &QTimer::timeout, this, [this]()
        {
            if (!process || !isInitialized())
            {
                return;
            }

            messanger.send(Messanger::Message("ping"), process);
        }
    );

    timerPing.setInterval(3000);
    timerPing.start();

    startProcess();
}

Manager::~Manager()
{
    stopProcess();
}

std::shared_ptr<Browser> Manager::createBrowser(const QUrl &url, const Browser::Settings& settings)
{
    if (!process || !isInitialized())
    {
        qWarning() << "not initialized";
        return nullptr;
    }

    QMap<QString, QString> parameters =
    {
        { "url", url.toString() },
        { "visible", boolToValue(settings.visible) },
        { "show-responses", boolToValue(settings.showResponses) },
    };

    parameters.insert("filter.resource-types", convertToParameterValue(settings.filter.resourceTypes));
    parameters.insert("filter.methods", convertToParameterValue(settings.filter.methods));
    parameters.insert("filter.mime-types", convertToParameterValue(settings.filter.mimeTypes));
    parameters.insert("filter.url-prefixes", convertToParameterValue(settings.filter.urlPrefixes));
    parameters.insert("filter.response-statuses", convertToParameterValue(settings.filter.responseStatuses));

    const int64_t messageId = messanger.send(Messanger::Message("browser-open", parameters), process);

    std::shared_ptr<Browser> browser(new Browser(*this, -1, url, false));

    storage.addWithMessageId(browser, messageId);

    return browser;
}

void Manager::closeBrowser(const int id)
{
    if (!process || !isInitialized())
    {
        qWarning() << "not initialized";
        return;
    }

    messanger.send(Messanger::Message(
                        "browser-close",
                        {
                            { "id", QString("%1").arg(id) },
                        }), process);
}

bool Manager::isInitialized() const
{
    if (!process)
    {
        Manager& this_ = const_cast<Manager&>(*this);

        this_._initialized = false;
        this_.startProcess();
    }

    return _initialized;
}

void Manager::createBrowserOnce(const QUrl &url, const Browser::Settings::Filter &filter, std::function<void(std::shared_ptr<Response>, bool& closeBrowser)> onReceived)
{
    if (!process || !isInitialized())
    {
        qWarning() << "not initialized";
        return;
    }

    Browser::Settings settings = Browser::getDefaultSettingsForDisposable();
    settings.filter = filter;

    std::shared_ptr<cweqt::Browser> browser = createBrowser(url, settings);
    if (browser)
    {
        QObject::connect(browser.get(), QOverload<std::shared_ptr<cweqt::Response>>::of(&cweqt::Browser::recieved), this, [onReceived, browser](std::shared_ptr<cweqt::Response> response)
        {
            if (!onReceived)
            {
                qWarning() << "callback is null, browser will be closed";
                browser->close();
                return;
            }

            bool closeBrowser = true;
            onReceived(response, closeBrowser);

            if (closeBrowser)
            {
                browser->close();
            }
        });
    }
    else
    {
        qWarning() << "browser is null";
    }
}

void Manager::startProcess()
{
    stopProcess();

    if (!isExecutableExists())
    {
        qWarning() << "executable" << executablePath << "not exists";
        return;
    }

    process = new QProcess();
    
    connect(process, &QProcess::started, this, []() { qDebug() << "started"; });
    connect(process, &QProcess::readyRead, this, &Manager::onReadyRead);
    connect(process, QOverload<QProcess::ProcessError>::of(&QProcess::errorOccurred), this, [](QProcess::ProcessError error) { qDebug() << "error =" << error; });
    //connect(process, &QProcess::stateChanged, this, [](const QProcess::ProcessState state) { qDebug() << "state =" << state; });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus exitStatus)
    {
        qDebug() << "exit code =" << exitCode << ", exit status =" << exitStatus;
        if (process)
        {
            process->deleteLater();
            process = nullptr;
        }
    });

    process->start(executablePath, QStringList());
}

void Manager::stopProcess()
{
    _initialized = false;

    if (process)
    {
        messanger.send(Messanger::Message("exit"), process);
        process->waitForFinished(1000);
        process = nullptr;
    }

    storage.clear();
}

void Manager::onReadyRead()
{
    if (!process)
    {
        return;
    }

    while (process->canReadLine())
    {
        const QByteArray line = process->readLine();
        //qDebug() << "recieved:" << line;
        messanger.parseLine(line);

        if (!process)
        {
            break;
        }
    }
}

}
