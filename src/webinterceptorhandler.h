#pragma once

#include <QUrl>
#include <QProcess>
#include <QSet>
#include <QTimer>
#include <map>

class WebInterceptorHandler : public QObject
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

    static bool checkExecutableExists();
    static QString getExecutablePath();

    explicit WebInterceptorHandler(QObject *parent = nullptr);

    void setFilterSettings(const FilterSettings& filterSettings_) { filterSettings = filterSettings_; }
    void start(const bool visibleWindow, const QUrl& url, int timeout, std::function<void(const Response&)> onResponseDone);
    void stop();

signals:
    void readyRead();

private slots:
    void onReadyRead();

private:
    void parseLine(const QByteArray& line);
    void parse(const QByteArray& messageType, const QMap<QByteArray, QByteArray>& properties, const QByteArray& data);

    std::function<void(const Response&)> onResponseDone = nullptr;

    FilterSettings filterSettings;

    QProcess* process = nullptr;

    std::map<uint64_t, Response> responses;

    QTimer timeoutTimer;
};
