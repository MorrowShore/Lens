#pragma once

#include "Browser.h"
#include "Messanger.h"
#include <QUrl>
#include <QProcess>
#include <QSet>
#include <QWindow>
#include <QMap>
#include <map>
#include <set>

namespace cweqt
{

class Manager : public QObject
{
    Q_OBJECT
public:
    static const QStringList& getAvailableResourceTypes();

    explicit Manager(const QString& scrapperExecutablePath, QObject *parent = nullptr);

    bool isExecutableExists() const;

    void startProcess();
    void stopProcess();

    std::shared_ptr<Browser> createBrowser(const QUrl& url, const Browser::Settings& settings);
    bool isInitialized() const;

    void request(const QString& method,
                 const QUrl& url,
                 const std::map<QString, QString> headerParameters,
                 std::function<void(std::shared_ptr<Response>)> onReceived); // TODO

signals:
    void initialized();
    void browserOpened(std::shared_ptr<Browser> browser);

private slots:
    void onReadyRead();

private:
    friend class Browser;

    void closeBrowser(const int id);

    const QString executablePath;

    QProcess* process = nullptr;
    bool _initialized = false;

    std::map<int, std::shared_ptr<Browser>> browsers; // <browser-id, Browser>

    std::map<int64_t, std::shared_ptr<Browser>> creatingBrowsers; // <message-id, Browser>

    Messanger messanger;
};

}
