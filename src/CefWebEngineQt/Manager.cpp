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

Manager::Manager(const QString& executablePath_, QObject *parent)
    : QObject{parent}
    , executablePath(executablePath_)
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

                if (std::shared_ptr<Browser> browser = storage.findByBrowserId(browserId); browser)
                {
                    browser->addResponseData(responseId, data);
                }
                else
                {
                    qWarning() << Q_FUNC_INFO << "browser id" << browserId << "not found, message type =" << type;
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
                    qWarning() << Q_FUNC_INFO << "browser id" << response->browserId << "not found, message type =" << type;
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
                    qWarning() << Q_FUNC_INFO << "browser id" << browserId << "not found, message type =" << type;
                }
            }
            else if (type == "log")
            {
                QString text;
                if (!getParamStr(params, "text", text))
                {
                    return;
                }

                qInfo() << "WebEngine:" << text;
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

                if (std::shared_ptr<Browser> openingBrowser = storage.findByMessageId(messageId); openingBrowser)
                {
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
                    qWarning() << Q_FUNC_INFO << "unknown browser id" << browserId;
                }
            }
            else if (type == "exited")
            {
                //qWarning() << Q_FUNC_INFO << type << "recevied";
            }
            else
            {
                qWarning() << Q_FUNC_INFO << "unknown message type" << type;
            }
        }
    );

    connect(&timerPing, &QTimer::timeout, this, [this]()
        {
            if (!isInitialized())
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
                            // TODO: filter settings
                        }), process);

    std::shared_ptr<Browser> browser(new Browser(*this, -1, url, false));

    storage.addWithMessageId(browser, messageId);

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
    if (!process)
    {
        Manager& this_ = const_cast<Manager&>(*this);

        this_._initialized = false;
        this_.startProcess();
    }

    return _initialized;
}

void Manager::createDisposable(const QUrl &url, const Browser::Settings::Filter &filter, std::function<void(std::shared_ptr<Response>, bool& closeBrowser)> onReceived)
{
    if (!isInitialized())
    {
        qWarning() << Q_FUNC_INFO << "not initialized";
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
                qWarning() << Q_FUNC_INFO << "callback is null, browser will be closed";
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
        qWarning() << Q_FUNC_INFO << "browser is null";
    }
}

void Manager::startProcess()
{
    stopProcess();

    if (!isExecutableExists())
    {
        qWarning() << Q_FUNC_INFO << "executable" << executablePath << "not exists";
        return;
    }

    process = new QProcess();
    
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
        messanger.parseLine(process->readLine());

        if (!process)
        {
            break;
        }
    }
}

}
