#pragma once

#include <QObject>
#include <QSet>
#include <QUrl>
#include <memory>

namespace cweqt
{

class Manager;

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

        Filter filter;
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

    bool isOpened() const { return opened; }
    int getId() const { return id; }
    QUrl getInitialUrl() const { return initialUrl; }

signals:
    void responseRecieved(std::shared_ptr<Browser::Response> response);
    void closed();

public slots:
    void close();

private:
    friend class Manager;
    
    explicit Browser(Manager& manager, const int id, const QUrl& initialUrl, const bool opened, QObject *parent = nullptr);
    
    Manager& manager;
    const int id;
    const QUrl initialUrl;
    bool opened = false;
};

}
