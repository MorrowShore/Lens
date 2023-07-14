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

    void openBrowser(const QUrl& url, const Browser::Settings& settings);

signals:
    void initialized();
    void browserOpened(std::shared_ptr<Browser> browser);

private slots:
    void onReadyRead();

private:
    const QString executablePath;

    QProcess* process = nullptr;
    bool _initialized = false;

    std::map<uint64_t, std::shared_ptr<Browser::Response>> responses;
    std::map<int, std::shared_ptr<Browser>> browsers;

    Messanger messanger;
};

}
