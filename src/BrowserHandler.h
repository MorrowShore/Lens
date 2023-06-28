#pragma once

#include <QUrl>
#include <QProcess>
#include <QSet>
#include <QWindow>
#include <QMap>
#include <map>
#include <set>

class BrowserHandler : public QObject
{
    Q_OBJECT
public:
    struct FilterSettings
    {
        QSet<QString> resourceTypes;
        QSet<QString> methods;
        QSet<QString> mimeTypes;
        QSet<QString> urlPrefixes;
        QSet<int> responseStatuses;
    };

    struct CommandLineParameters
    {
        bool showResponses = false;
        FilterSettings filterSettings;
    };

    struct Browser
    {
        int id = 0;
        QUrl initialUrl;
    };

    struct Response
    {
        int browserId = 0;
        uint64_t requestId = 0;
        QString method;
        QString resourceType;
        QString mimeType;
        QUrl url;
        int status = 0;
        QByteArray data;
    };

    static const QStringList& getAvailableResourceTypes();
    static bool checkExecutableExists();
    static QString getExecutablePath();

    explicit BrowserHandler(QObject *parent = nullptr);

    void startProcess(const CommandLineParameters& parameters);
    void stopProcess();

    void openBrowser(const QUrl& url);
    void closeBrowser(const int id);

    bool isInitialized() const { return _initialized; }

signals:
    void initialized();
    void responsed(const Response& response);
    void browserOpened(const Browser& browser);
    void browserClosed(const Browser& browser);
    void windowCreated(QWindow* window);

private slots:
    void onReadyRead();

private:
    void send(const QString& type, const QMap<QString, QString>& properties, const QByteArray& data);

    void parseLine(const QByteArray& line);
    void parse(const QByteArray& messageType, const QMap<QByteArray, QByteArray>& properties, const QByteArray& data);

    QProcess* process = nullptr;
    bool _initialized = false;

    std::map<uint64_t, Response> responses;
    std::map<int, Browser> browsers;
};
