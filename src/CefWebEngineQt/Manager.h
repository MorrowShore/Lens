#pragma once

#include "Browser.h"
#include "BrowsersStorage.h"
#include "Messanger.h"
#include <QUrl>
#include <QProcess>
#include <QSet>
#include <QWindow>
#include <QMap>
#include <QTimer>
#include <map>
#include <set>

namespace cweqt
{

class Manager : public QObject
{
    Q_OBJECT
public:
    static const QStringList& getAvailableResourceTypes();

    explicit Manager(const QString& executablePath, QObject *parent = nullptr);
    virtual ~Manager();

    bool isExecutableExists() const;

    std::shared_ptr<Browser> createBrowser(const QUrl& url, const Browser::Settings& settings = Browser::Settings());
    bool isInitialized() const;

    void createBrowserOnce(const QUrl& url,
                const cweqt::Browser::Settings::Filter& filter,
                std::function<void(std::shared_ptr<Response>, bool& closeBrowser)> onReceived);

signals:
    void initialized();
    void browserOpened(std::shared_ptr<Browser> browser);

private slots:
    void onReadyRead();

private:
    friend class Browser;

    void startProcess();
    void stopProcess();
    void closeBrowser(const int id);

    const QString executablePath;

    QProcess* process = nullptr;
    bool _initialized = false;

    BrowsersStorage storage;

    Messanger messanger;

    QTimer timerPing;
};

}
