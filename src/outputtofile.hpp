#ifndef OUTPUTTOFILE_HPP
#define OUTPUTTOFILE_HPP

#include <QObject>
#include <QSettings>
#include "models/chatmessagesmodle.hpp"
#include "types.hpp"
#include "abstractchatservice.hpp"

class OutputToFile : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString outputFolderPath READ outputFolder WRITE setOutputFolder NOTIFY outputFolderChanged)
    Q_PROPERTY(QString standardOutputFolder READ standardOutputFolder CONSTANT)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    enum OutputToFileCodec
    {
        UTF8Codec = 0,
        ANSICodec = 100,
        ANSIWithUTF8Codec = 200
    };

    explicit OutputToFile(QSettings& settings, const QString& settingsGroupPath, QNetworkAccessManager& network, const ChatMessagesModel& messages, QObject *parent = nullptr);
    ~OutputToFile();

    bool enabled() const;
    void setEnabled(bool enabled);
    QString standardOutputFolder() const;
    QString outputFolder() const;
    void resetSettings();

    void setYouTubeInfo(const AxelChat::YouTubeInfo& youTubeCurrent);
    void setTwitchInfo(const AxelChat::TwitchInfo& twitchCurrent);
    void setGoodGameInfo(const AxelChat::GoodGameInfo& goodGameCurrent);

    Q_INVOKABLE bool setCodecOption(int option, bool applyWithoutReset); // return true if need restart
    Q_INVOKABLE int codecOption() const;

    void setOutputFolder(QString outputFolder);
    void writeMessages(const QList<ChatMessage>& messages);
    Q_INVOKABLE void showInExplorer();
    void tryDownloadAvatar(const QString& authorId, const QUrl &url, const AbstractChatService::ServiceType serviceType);
    QString getAuthorDirectory(const AbstractChatService::ServiceType serviceType, const QString& authorId) const;
    void writeAuthors(const QList<ChatAuthor*>& authors);

signals:
    void outputFolderChanged();
    void enabledChanged();
    void youTubeLastMessageIdChanged(const QString& id);
    void authorNameChanged(const ChatAuthor& author, const QString& prevName, const QString& newName);

private:
    void writeMessage(const QList<QPair<QString, QString>> tags /*<tagName, tagValue>*/);
    QByteArray prepare(const QString& text);

    struct AuthorInfo{
        QString name;
        QString youtubeUrl;
        int messagesCount = 0;
    };

    void reinit(bool forceUpdateOutputFolder);
    void writeStartupInfo(const QString& messagesFolder);
    void writeInfo();

    QSettings& settings;
    const QString SettingsGroupPath;
    QNetworkAccessManager& network;
    const ChatMessagesModel& messagesModel;

    bool _enabled = false;
    QString _outputFolder = standardOutputFolder();
    QString _sessionFolder;

    QFile* _fileMessagesCount           = nullptr;
    QFile* _fileMessages                = nullptr;
    QSettings* _iniCurrentInfo          = nullptr;

    QString _youTubeLastMessageId;

    OutputToFileCodec _codec = OutputToFileCodec::UTF8Codec;

    AxelChat::YouTubeInfo _youTubeInfo;
    AxelChat::TwitchInfo _twitchInfo;
    AxelChat::GoodGameInfo _goodGameInfo;

    const QDateTime _startupDateTime = QDateTime::currentDateTime();

    const int youTubeAvatarSize = 72;
    QSet<QString> downloadedAvatarsAuthorId;
};

#endif // OUTPUTTOFILE_HPP
