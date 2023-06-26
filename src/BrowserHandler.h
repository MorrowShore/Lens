#pragma once

#include <QUrl>
#include <QProcess>
#include <QSet>
#include <QTimer>
#include <QWindow>
#include <map>

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
        QUrl url;
        bool showResponses = false;
        bool windowVisible = true;
        FilterSettings filterSettings;
    };

    struct Response
    {
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

    void start(const CommandLineParameters& parameters, int timeout);
    void stop();

signals:
    void responsed(const Response& response);
    void windowCreated(QWindow* window);
    void processClosed();

private slots:
    void onReadyRead();

private:
    void parseLine(const QByteArray& line);
    void parse(const QByteArray& messageType, const QMap<QByteArray, QByteArray>& properties, const QByteArray& data);

    QProcess* process = nullptr;

    std::map<uint64_t, Response> responses;

    QTimer timeoutTimer;
};
