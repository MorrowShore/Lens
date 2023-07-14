#include "Manager.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QDebug>

namespace
{

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

static QString boolToValue(const bool value)
{
    return value ? "true" : "false";
}

static bool getParamStr(const QMap<QString, QString>& parameters, const QString& name, QString& value)
{
    if (!parameters.contains(name))
    {
        qWarning() << Q_FUNC_INFO << "not found parameter" << name;
        return false;
    }

    value = parameters.value(name);
    return true;
}

static bool getParamInt64(const QMap<QString, QString>& parameters, const QString& name, int64_t& value)
{
    if (!parameters.contains(name))
    {
        qWarning() << Q_FUNC_INFO << "not found parameter" << name;
        return false;
    }

    const QString str = parameters.value(name);

    bool ok = false;
    const auto tmp = str.toLongLong(&ok);
    if (!ok)
    {
        qWarning() << Q_FUNC_INFO << "failed to convert parameter" << name << "=" << str << "to integer";
        return false;
    }

    value = tmp;

    return true;
}

static bool getParamUInt64(const QMap<QString, QString>& parameters, const QString& name, uint64_t& value)
{
    if (!parameters.contains(name))
    {
        qWarning() << Q_FUNC_INFO << "not found parameter" << name;
        return false;
    }

    const QString str = parameters.value(name);

    bool ok = false;
    const auto tmp = str.toULongLong(&ok);
    if (!ok)
    {
        qWarning() << Q_FUNC_INFO << "failed to convert parameter" << name << "=" << str << "to integer";
        return false;
    }

    value = tmp;

    return true;
}

static bool getParamInt(const QMap<QString, QString>& parameters, const QString& name, int& value)
{
    if (!parameters.contains(name))
    {
        qWarning() << Q_FUNC_INFO << "not found parameter" << name;
        return false;
    }

    const QString str = parameters.value(name);

    bool ok = false;
    const auto tmp = str.toInt(&ok);
    if (!ok)
    {
        qWarning() << Q_FUNC_INFO << "failed to convert parameter" << name << "=" << str << "to integer";
        return false;
    }

    value = tmp;

    return true;
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

Manager::Manager(const QString& scrapperExecutablePath, QObject *parent)
    : QObject{parent}
    , executablePath(scrapperExecutablePath)
{
    connect(&messanger, QOverload<const Messanger::Message&>::of(&Messanger::messageReceived), this, [this](const Messanger::Message& message)
    {
        const QString& type = message.getType();
        const QMap<QString, QString>& params = message.getParameters();
        const QByteArray& data = message.getData();

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

            auto it = browsers.find(browserId);
            if (it == browsers.end() || !it->second)
            {
                qWarning() << Q_FUNC_INFO << "browser id" << browserId << "not found or browser is null, message type =" << type;
                return;
            }

            it->second->addResponseData(responseId, data);
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

            auto it = browsers.find(response->browserId);
            if (it == browsers.end() || !it->second)
            {
                qWarning() << Q_FUNC_INFO << "browser id" << response->browserId << "not found or browser is null, message type =" << type;
                return;
            }

            it->second->registerResponse(response);
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

            auto it = browsers.find(browserId);
            if (it == browsers.end() || !it->second)
            {
                qWarning() << Q_FUNC_INFO << "browser id" << browserId << "not found or browser is null, message type =" << type;
                return;

            }

            it->second->finalizeResponse(responseId);
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

            auto it = creatingBrowsers.find(messageId);
            if (it == creatingBrowsers.end())
            {
                std::shared_ptr<Browser> browser(new Browser(*this, browserId, url, true));

                browsers[browserId] = browser;

                emit browserOpened(browser);
            }
            else
            {
                if (it->second)
                {
                    std::shared_ptr<Browser> browser = it->second;
                    creatingBrowsers.erase(it);

                    browser->setOpened(browserId);

                    browsers[browserId] = browser;

                    emit browserOpened(browser);
                }
                else
                {
                    qWarning() << Q_FUNC_INFO << "creating browser with message id" << messageId << "is null, browser id =" << browserId;
                }
            }
        }
        else if (type == "initialized")
        {
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

            auto it = browsers.find(browserId);
            if (it == browsers.end())
            {
                qWarning() << Q_FUNC_INFO << "unknown browser id" << browserId;
                return;
            }

            std::shared_ptr<Browser>& browser = it->second;

            if (browser)
            {
                browser->setClosed();
            }
            else
            {
                qWarning() << Q_FUNC_INFO << "browser is null";
            }

            browsers.erase(it);
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "unknown message type" << type;
        }
    });
}

std::shared_ptr<Browser> Manager::createBrowser(const QUrl &url, const Browser::Settings& settings)
{
    if (!isInitialized())
    {
        qWarning() << Q_FUNC_INFO << "not initialized";
        return nullptr;
    }

    const int64_t messageId = messanger.send(Messanger::Message(
                       "browser-open",
                        {
                            { "url", url.toString() },
                            { "visible", boolToValue(settings.visible) },
                            { "show-responses", boolToValue(settings.showResponses) },
                        }), process);

    std::shared_ptr<Browser> browser(new Browser(*this, -1, url, false));

    creatingBrowsers[messageId] = browser;

    return browser;
}

void Manager::closeBrowser(const int id)
{
    if (!isInitialized())
    {
        qWarning() << Q_FUNC_INFO << "not initialized";
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
    return _initialized;
}

void Manager::startProcess()
{
    stopProcess();

    if (!isExecutableExists())
    {
        qWarning() << Q_FUNC_INFO << "executable" << executablePath << "not exists";
        return;
    }

    process = new QProcess(this);
    
    connect(process, &QProcess::readyRead, this, &Manager::onReadyRead);
    connect(process, &QProcess::stateChanged, this, [this](const QProcess::ProcessState state)
    {
        if (state == QProcess::ProcessState::NotRunning)
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
        //TODO: send exit

        process->terminate();
        process = nullptr;
    }

    browsers.clear();
}

void Manager::onReadyRead()
{
    if (!process)
    {
        return;
    }

    while (process->canReadLine())
    {
        messanger.parseLine(process->readLine());

        if (!process)
        {
            break;
        }
    }
}

}
