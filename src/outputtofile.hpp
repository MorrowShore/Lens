#ifndef OUTPUTTOFILE_HPP
#define OUTPUTTOFILE_HPP

#include "utils.hpp"
#include "setting.h"
#include "models/chatmessagesmodle.hpp"
#include "chat_services/chatservice.hpp"
#include <QObject>
#include <QSettings>

class OutputToFile : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString outputFolderPath READ getOutputFolder WRITE setOutputFolder NOTIFY outputFolderChanged)
    Q_PROPERTY(QString standardOutputFolder READ standardOutputFolder CONSTANT)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)

public:
    enum class OutputToFileCodec
    {
        UTF8Codec = 0,
        ANSICodec = 100,
        ANSIWithUTF8Codec = 200
    };

    explicit OutputToFile(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network, const ChatMessagesModel& messages, const QList<ChatService*>& services, QObject *parent = nullptr);
    ~OutputToFile();

    bool isEnabled() const;
    void setEnabled(bool enabled);
    QString standardOutputFolder() const;
    QString getOutputFolder() const;
    void resetSettings();

    Q_INVOKABLE bool setCodecOption(int option, bool applyWithoutReset); // return true if need restart
    Q_INVOKABLE int codecOption() const;

    void setOutputFolder(const QString& outputDirectory);
    void writeMessages(const QList<ChatMessage>& messages);
    Q_INVOKABLE void showInExplorer();
    void downloadAvatar(const QString& authorId, const QUrl &url, const ChatService::ServiceType serviceType);
    QString getAuthorDirectory(const ChatService::ServiceType serviceType, const QString& authorId) const;
    QString getServiceDirectory(const ChatService::ServiceType serviceType) const;
    void writeAuthors(const QList<ChatAuthor*>& authors);
    void writeServiceState(const ChatService* service) const;

signals:
    void outputFolderChanged();
    void enabledChanged();
    void youTubeLastMessageIdChanged(const QString& id);
    void authorNameChanged(const ChatAuthor& author, const QString& prevName, const QString& newName);

private:
    QString convertUrlForFileName(const QUrl& url, const QString& imageFileFormat) const;
    void downloadImage(const QUrl &url, const QString& fileName, const QString& imageFormat, const int height, bool ignoreIfExists);
    void downloadEmoji(const QUrl &url, const int height, const ChatService::ServiceType serviceType);

    void writeMessage(const QList<QPair<QString, QString>> tags /*<tagName, tagValue>*/);
    QByteArray prepare(const QString& text);

    void reinit(bool forceUpdateOutputFolder);

    void writeApplicationState(const bool started) const;

    QNetworkAccessManager& network;
    const ChatMessagesModel& messagesModel;
    const QList<ChatService*>& services;

    Setting<bool> enabled;
    Setting<QString> outputDirectory;
    QString _sessionFolder;

    QFile* _fileMessagesCount           = nullptr;
    QFile* _fileMessages                = nullptr;

    Setting<QString> youTubeLastMessageId;

    Setting<OutputToFileCodec> codec;

    const QDateTime _startupDateTime = QDateTime::currentDateTime();

    const QString ImageFileFormat = "png";
    const int avatarHeight = 72;
    QSet<QString> needIgnoreDownloadFileNames;
};

Q_DECLARE_METATYPE(OutputToFile::OutputToFileCodec)

#endif // OUTPUTTOFILE_HPP
