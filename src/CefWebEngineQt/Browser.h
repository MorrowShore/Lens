#pragma once

#include <QObject>
#include <QSet>
#include <QUrl>
#include <memory>

namespace cweqt
{

class Manager;

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

class Browser : public QObject
{
    Q_OBJECT
public:
    struct Settings
    {
        struct Filter
        {
            QSet<QString> resourceTypes;
            QSet<QString> methods;
            QSet<QString> mimeTypes;
            QSet<QString> urlPrefixes;
            QSet<int> responseStatuses;
        };

        bool visible = true;
        bool showResponses = false;
        bool closeOnFirstResponse = false;//TODO

        Filter filter;
    };

    bool isOpened() const { return state == State::Opened; }
    QUrl getInitialUrl() const { return initialUrl; }

signals:
    void opened();
    void recieved(std::shared_ptr<Response> response);
    void closed();

public slots:
    void close();

private:
    enum class State { NotOpened, Opened, Closed };

    friend class Manager;

    static Settings getDefaultSettingsForDisposable();
    
    explicit Browser(Manager& manager, const int id, const QUrl& initialUrl, const bool opened);

    void setOpened(const int id);
    void setClosed();

    void registerResponse(const std::shared_ptr<Response>& response);
    void addResponseData(const uint64_t responseId, const QByteArray& data);
    void finalizeResponse(const uint64_t responseId);
    
    Manager& manager;
    const int id;
    const QUrl initialUrl;
    State state = State::NotOpened;

    std::map<uint64_t, std::shared_ptr<Response>> responses;
};

}
