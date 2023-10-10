#pragma once

#include "Feature.h"
#include "utils/QtAxelChatUtils.h"
#include "setting.h"
#include "models/messagesmodel.h"
#include "chat_services/chatservice.h"
#include <QObject>
#include <QSettings>

class BackendManager;

class OutputToFile : public Feature
{
    Q_OBJECT
    Q_PROPERTY(QString outputFolderPath READ getOutputFolder WRITE setOutputFolder NOTIFY outputFolderChanged)
    Q_PROPERTY(QString standardOutputFolder READ standardOutputFolder CONSTANT)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)

public:
    enum class Codec
    {
        UTF8 = 0,
        ANSI = 100,
        ANSIWithUTF8Codes = 200
    };

    explicit OutputToFile(QSettings& settings, const QString& settingsGroupPath, BackendManager& backend, QNetworkAccessManager& network, const MessagesModel& messages, QList<std::shared_ptr<ChatService>>& services, QObject *parent = nullptr);
    ~OutputToFile();

    bool isEnabled() const;
    void setEnabled(bool enabled);
    QString standardOutputFolder() const;
    QString getOutputFolder() const;
    void resetSettings();

    Q_INVOKABLE bool setCodecOption(int option); // return true if need restart
    Q_INVOKABLE int codecOption() const;

    void setOutputFolder(const QString& outputDirectory);
    void writeMessages(const QList<std::shared_ptr<Message>>& messages, const AxelChat::ServiceType serviceType);
    Q_INVOKABLE void showInExplorer();
    void downloadAvatar(const QString& authorId, const AxelChat::ServiceType serviceType, const QUrl &url);
    QString getAuthorDirectory(const AxelChat::ServiceType serviceType, const QString& authorId) const;
    QString getServiceDirectory(const AxelChat::ServiceType serviceType) const;
    void writeAuthors(const QList<std::shared_ptr<Author>>& authors);
    void writeServiceState(const ChatService* service) const;
    void writeApplicationState(const bool started, const int viewersTotalCount) const;

signals:
    void outputFolderChanged();
    void enabledChanged();
    void authorNameChanged(const Author& author, const QString& prevName, const QString& newName);

private:
    QString convertUrlForFileName(const QUrl& url, const QString& imageFileFormat) const;
    void downloadEmoji(const QUrl &url, const int height, const AxelChat::ServiceType serviceType);

    void downloadImage(const QUrl &url, const QString& fileName, const QString& imageFormat, const int height, bool ignoreIfExists);

    void writeMessage(const QList<QPair<QString, QString>> tags /*<tagName, tagValue>*/);
    QByteArray prepare(const QString& text);

    void reinit(bool forceUpdateOutputFolder);

    QSettings& settings;
    const QString settingsGroupPath;
    QNetworkAccessManager& network;
    const MessagesModel& messagesModel;
    QList<std::shared_ptr<ChatService>>& services;

    Setting<bool> enabled;
    Setting<QString> outputDirectory;
    QString _relativeSessionFolder;

    QFile* _fileMessagesCount           = nullptr;
    QFile* _fileMessages                = nullptr;

    Codec codec = Codec::UTF8;

    const QDateTime _startupDateTime = QDateTime::currentDateTime();

    const QString ImageFileFormat = "png";
    const int avatarHeight = 72;
    QSet<QString> needIgnoreDownloadFileNames;
};
