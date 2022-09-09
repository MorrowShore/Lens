#ifndef OUTPUTTOFILE_HPP
#define OUTPUTTOFILE_HPP

#include "utils.h"
#include "setting.h"
#include "models/messagesmodle.h"
#include "chat_services/chatservice.h"
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

    explicit OutputToFile(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network, const MessagesModel& messages, const QList<ChatService*>& services, QObject *parent = nullptr);
    ~OutputToFile();

    bool isEnabled() const;
    void setEnabled(bool enabled);
    QString standardOutputFolder() const;
    QString getOutputFolder() const;
    void resetSettings();

    Q_INVOKABLE bool setCodecOption(int option, bool applyWithoutReset); // return true if need restart
    Q_INVOKABLE int codecOption() const;

    void setOutputFolder(const QString& outputDirectory);
    void writeMessages(const QList<Message>& messages);
    Q_INVOKABLE void showInExplorer();
    void downloadAvatar(const QString& authorId, const AxelChat::ServiceType serviceType, const QUrl &url);
    QString getAuthorDirectory(const AxelChat::ServiceType serviceType, const QString& authorId) const;
    QString getServiceDirectory(const AxelChat::ServiceType serviceType) const;
    void writeAuthors(const QList<Author*>& authors);
    void writeServiceState(const ChatService* service) const;

signals:
    void outputFolderChanged();
    void enabledChanged();
    void youTubeLastMessageIdChanged(const QString& id);
    void authorNameChanged(const Author& author, const QString& prevName, const QString& newName);

private:
    QString convertUrlForFileName(const QUrl& url, const QString& imageFileFormat) const;
    void downloadEmoji(const QUrl &url, const int height, const AxelChat::ServiceType serviceType);

    void downloadImage(const QUrl &url, const QString& fileName, const QString& imageFormat, const int height, bool ignoreIfExists);

    void writeMessage(const QList<QPair<QString, QString>> tags /*<tagName, tagValue>*/);
    QByteArray prepare(const QString& text);

    void reinit(bool forceUpdateOutputFolder);

    void writeApplicationState(const bool started) const;

    QNetworkAccessManager& network;
    const MessagesModel& messagesModel;
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
