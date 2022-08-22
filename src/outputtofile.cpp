#include "outputtofile.hpp"
#include "models/chatauthor.h"
#include "models/chatmessage.h"
#include "youtube.hpp"
#include <QStandardPaths>
#include <QGuiApplication>
#include <QTextCodec>
#include <QDesktopServices>
#include <QTimeZone>
#include <QDir>
#include <QTextCodec>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QImage>
#include <QImageReader>
#include "utils_axelchat.hpp"

namespace
{

static const QString DateTimeFileNameFormat = "yyyy-MM-ddThh-mm-ss.zzz";
static const QString MessagesFileName = "messages.ini";
static const QString MessagesCountFileName = "messages_count.txt";

int getStatusCode(const QNetworkReply& reply)
{
    QVariant statusCode = reply.attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (statusCode.isValid())
    {
        bool ok = false;
        const int result = statusCode.toInt(&ok);
        if (ok)
        {
            return result;
        }
    }

    return -1;
}

QString timeToString(const QDateTime& dateTime)
{
    return dateTime.toTimeZone(QTimeZone::systemTimeZone()).toString(Qt::DateFormat::ISODateWithMs);
}

QString removePostfix(const QString& string, const QString& postfix, const Qt::CaseSensitivity caseSensitivity)
{
    if (!string.endsWith(postfix, caseSensitivity))
    {
        return string;
    }

    return string.left(string.length() - postfix.length());
}

}

OutputToFile::OutputToFile(QSettings &settings, const QString &settingsGroupPath, QNetworkAccessManager& network_, const ChatMessagesModel& messagesModel_, QObject *parent)
    : QObject(parent)
    , network(network_)
    , messagesModel(messagesModel_)
    , enabled(settings, settingsGroupPath + "/enabled", false)
    , outputDirectory(settings, settingsGroupPath + "/output_folder", standardOutputFolder())
    , youTubeLastMessageId(settings, settingsGroupPath + "/youtube_last_saved_message_id")
    , codec(settings, settingsGroupPath + "/codec", OutputToFileCodec::UTF8Codec)
{
    reinit(true);
}

OutputToFile::~OutputToFile()
{
    if (enabled.get())
    {
        if (_iniCurrentInfo)
        {
            _iniCurrentInfo->setValue("software/started", false);
        }
    }

    if (_fileMessages && _fileMessages->isOpen())
    {
        _fileMessages->close();
    }

    if (_fileMessagesCount && _fileMessagesCount->isOpen())
    {
        _fileMessagesCount->close();
    }
}

bool OutputToFile::isEnabled() const
{
    return enabled.get();
}

void OutputToFile::setEnabled(bool enabled_)
{
    if (enabled.set(enabled_))
    {
        if (enabled.get())
        {
            reinit(false);
        }

        emit enabledChanged();
    }
}

QString OutputToFile::standardOutputFolder() const
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/" + QGuiApplication::applicationName() + "/output";
}

QString OutputToFile::getOutputFolder() const
{
    return outputDirectory.get();
}

void OutputToFile::setOutputFolder(const QString& outputDirectory_)
{
    if (outputDirectory.set(outputDirectory_.trimmed()))
    {
        reinit(true);
        emit outputFolderChanged();
    }
}

void OutputToFile::resetSettings()
{
    setOutputFolder(standardOutputFolder());
}

void OutputToFile::writeMessages(const QList<ChatMessage>& messages)
{
    if (!enabled.get())
    {
        return;
    }

    int firstValidMessage = 0;

    if (!youTubeLastMessageId.get().isEmpty())
    {
        for (int i = 0; i < messages.count(); ++i)
        {
            const ChatMessage& message = messages[i];

            if (message.id() == youTubeLastMessageId.get())
            {
                qDebug() << "found youtube message with id" << message.id() << ", ignore saving messages before it, index =" << i;
                firstValidMessage = i + 1;
                break;
            }
        }
    }

    QString currentLastYouTubeMessageId;

    for (int i = firstValidMessage; i < messages.count(); ++i)
    {
        const ChatMessage& message = messages[i];

        const QString authorId = message.authorId();
        const ChatAuthor* author = messagesModel.getAuthor(authorId);
        if (!author)
        {
            qWarning() << "Not found author id" << message.authorId();
            continue;
        }

        const AbstractChatService::ServiceType type = author->getServiceType();

        if (type == AbstractChatService::ServiceType::Unknown ||
            type == AbstractChatService::ServiceType::Test)
        {
            continue;
        }

        QList<QPair<QString, QString>> tags;

        tags.append(QPair<QString, QString>("author", author->name()));
        tags.append(QPair<QString, QString>("author_id", authorId));
        tags.append(QPair<QString, QString>("message", message.text()));
        tags.append(QPair<QString, QString>("time", timeToString(message.publishedAt())));
        tags.append(QPair<QString, QString>("service", AbstractChatService::serviceTypeToString(type)));

        writeMessage(tags);

        if (author->getServiceType() == AbstractChatService::ServiceType::YouTube)
        {
            const QString id = message.id();
            if (!id.isEmpty())
            {
                currentLastYouTubeMessageId = id;
            }
        }

        switch (type)
        {
        case AbstractChatService::ServiceType::GoodGame:
        case AbstractChatService::ServiceType::YouTube:
        case AbstractChatService::ServiceType::Twitch:
            tryDownloadAvatar(authorId, author->avatarUrl(), type);
            break;

        case AbstractChatService::ServiceType::Unknown:
        case AbstractChatService::ServiceType::Software:
        case AbstractChatService::ServiceType::Test:
            break;
        }
    }

    if (!currentLastYouTubeMessageId.isEmpty())
    {
        youTubeLastMessageId.set(currentLastYouTubeMessageId);
    }
}

void OutputToFile::showInExplorer()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString("file:///") + outputDirectory.get()));
}

void OutputToFile::tryDownloadAvatar(const QString& authorId, const QUrl& url_, const AbstractChatService::ServiceType service)
{
    QUrl url = url_;
    if (service == AbstractChatService::ServiceType::YouTube)
    {
        url = YouTube::createResizedAvatarUrl(url, avatarHeight);
    }

    if (url.isEmpty() || !enabled.get())
    {
        return;
    }

    const QString urlStr = url.toString();
    if (!urlStr.contains('/'))
    {
        qWarning() << "Url not contains '/'";
        return;
    }

    QString avatarName = urlStr.mid(urlStr.lastIndexOf('/') + 1);

    avatarName = removePostfix(avatarName, ".png", Qt::CaseSensitivity::CaseInsensitive);
    avatarName = removePostfix(avatarName, ".jpg", Qt::CaseSensitivity::CaseInsensitive);
    avatarName = removePostfix(avatarName, ".jpeg", Qt::CaseSensitivity::CaseInsensitive);

    if (service == AbstractChatService::ServiceType::Twitch)
    {
        avatarName = removePostfix(avatarName, "-300x300", Qt::CaseSensitivity::CaseInsensitive);
    }

    static const QString ImageFileFormat = "png";

    const QString authorDirectory = getAuthorDirectory(service, authorId);

    const QString avatarsDirectory = authorDirectory + "/avatars";
    QDir dir(avatarsDirectory);
    if (!dir.exists())
    {
        if (!dir.mkpath(avatarsDirectory))
        {
            qDebug() << "Failed to make path" << avatarsDirectory;
        }
    }

    const QString fileName = avatarsDirectory + "/" + avatarName + "." + ImageFileFormat;

    if (needIgnoreDownloadFileNames.contains(fileName))
    {
        return;
    }

    if (QFile(fileName).exists())
    {
        needIgnoreDownloadFileNames.insert(fileName);
        return;
    }

    //qDebug() << "Load avatar " << avatarName << "from" << urlStr;

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, AxelChat::UserAgentNetworkHeaderName);
    QNetworkReply* reply = network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, fileName]()
    {
        if (needIgnoreDownloadFileNames.contains(fileName))
        {
            return;
        }

        QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
        if (!reply)
        {
            return;
        }

        if (reply->bytesAvailable() <= 0)
        {
            qDebug() << "Failed download, reply is empty, status code =" << reply->errorString() << " image =" << fileName;
            return;
        }

        QImage rawImage;
        if (rawImage.loadFromData(reply->readAll()))
        {
            const QImage scaled = rawImage.scaledToHeight(avatarHeight, Qt::TransformationMode::SmoothTransformation);

            if (scaled.save(fileName, ImageFileFormat.toStdString().c_str()))
            {
                qDebug() << "Saved downloaded image" << fileName;
            }
            else
            {
                qWarning() << "Failed to save downloaded image" << fileName;
            }

            needIgnoreDownloadFileNames.insert(fileName);
        }
        else
        {
            qWarning() << "Failed to open downloaded image" << fileName;
        }

        reply->deleteLater();
    });
}

bool OutputToFile::setCodecOption(int option, bool applyWithoutReset)
{
    if (option == (int)codec.get())
    {
        return false;
    }

    if (applyWithoutReset)
    {
        switch (option) {
        case 0:
            codec.set(OutputToFileCodec::UTF8Codec);
            break;
        case 100:
            codec.set(OutputToFileCodec::ANSICodec);
            break;
        case 200:
            codec.set(OutputToFileCodec::ANSIWithUTF8Codec);
            break;
        default:
            qCritical() << "unknown codec option" << option << ", ignore";
            return false;
        }

        reinit(true);
    }

    return true;
}

int OutputToFile::codecOption() const
{
    return (int)codec.get();
}

void OutputToFile::writeMessage(const QList<QPair<QString, QString>> tags /*<tagName, tagValue>*/)
{
    if (!_fileMessages)
    {
        qDebug() << Q_FUNC_INFO << "!_fileMessages";
        return;
    }

    if (!_fileMessagesCount)
    {
        qDebug() << Q_FUNC_INFO << "!_fileMessagesStatistics";
        return;
    }

    if (!_fileMessages->isOpen())
    {
        if (!_fileMessages->open(QIODevice::OpenModeFlag::Text | QIODevice::OpenModeFlag::Append))
        {
            qWarning() << Q_FUNC_INFO << "failed to open file" << _fileMessages->fileName() << ":" << _fileMessages->errorString();
            return;
        }
    }

    if (!_fileMessagesCount->isOpen())
    {
        if (!_fileMessagesCount->open(QIODevice::OpenModeFlag::Text | QIODevice::OpenModeFlag::ReadWrite))
        {
            qWarning() << Q_FUNC_INFO << "failed to open file" << _fileMessagesCount->fileName() << ":" << _fileMessagesCount->errorString();
            return;
        }
    }

    int currentCount = 0;

    _fileMessagesCount->seek(0);
    const QByteArray countData = _fileMessagesCount->read(256);

    bool ok = false;
    currentCount = countData.trimmed().toInt(&ok);
    if (!ok || currentCount < 0)
    {
        currentCount = 0;
    }

    QByteArray data = "\n[" + QByteArray::number(currentCount) + "]\n";

    for (int i = 0; i < tags.count(); ++i)
    {
        const QPair<QString, QString>& tag = tags[i];
        data += tag.first.toLatin1() + "=" + prepare(tag.second) + "\n";
    }

    data += "\n";

    _fileMessages->write(data);

    if (!_fileMessages->flush())
    {
        qWarning() << "failed to flush file" << _fileMessages->fileName();
    }

    currentCount++;

    _fileMessagesCount->seek(0);
    _fileMessagesCount->write(QString("%1").arg(currentCount).toUtf8());
    if (!_fileMessagesCount->flush())
    {
        qWarning() << "failed to flush file" << _fileMessagesCount->fileName();
    }
}

QByteArray OutputToFile::prepare(const QString &text_)
{
    QString text = text_;

    const OutputToFileCodec codec_ = codec.get();

    if (codec_ != OutputToFileCodec::ANSIWithUTF8Codec)
    {
        text.replace('\n', "\\n");
        text.replace('\r', "\\r");
    }

    if (codec_ == OutputToFileCodec::UTF8Codec)
    {
        return text.toUtf8();
    }
    else if (codec_ == OutputToFileCodec::ANSICodec)
    {
        return text.toLocal8Bit();
    }
    else if (codec_ == OutputToFileCodec::ANSIWithUTF8Codec)
    {
        return convertANSIWithUtf8Numbers(text);
    }

    qWarning() << Q_FUNC_INFO << "unknown codec";

    return text.toUtf8();
}

QString OutputToFile::getAuthorDirectory(const AbstractChatService::ServiceType serviceType, const QString &authorId) const
{
    return outputDirectory.get() + "/authors/" + AbstractChatService::serviceTypeToString(serviceType) + "/" + authorId;
}

void OutputToFile::writeAuthors(const QList<ChatAuthor*>& authors)
{
    if (!enabled.get())
    {
        return;
    }

    for (const ChatAuthor* author : authors)
    {
        const QString pathDir = getAuthorDirectory(author->getServiceType(), author->authorId());
        if (!QDir(pathDir).exists())
        {
            QDir().mkpath(pathDir);
        }

        {
            const QString fileName = pathDir + "/info.ini";
            QFile file(fileName);
            if (!file.open(QFile::OpenModeFlag::WriteOnly | QFile::OpenModeFlag::Text))
            {
                qWarning() << "Failed to open/save" << fileName;
                continue;
            }

            const QString avatarUrlStr = author->avatarUrl().toString();
            if (!avatarUrlStr.contains('/'))
            {
                qWarning() << "Url not contains '/', url =" << avatarUrlStr << ", authorId =" << author->authorId() << author->name();
            }

            const QString avatarName = avatarUrlStr.mid(avatarUrlStr.lastIndexOf('/') + 1);

            file.write("[info]\n");
            file.write("name=" + prepare(author->name()) + "\n");
            file.write("id=" + prepare(author->authorId()) + "\n");
            file.write("service=" + prepare(AbstractChatService::serviceTypeToString(author->getServiceType())) + "\n");
            file.write("avatar_url=" + prepare(avatarUrlStr) + "\n");
            file.write("avatar_name=" + prepare(avatarName) + "\n");
            file.write("page_url=" + prepare(author->pageUrl().toString()) + "\n");
            //ToDo: file.write("last_message_at=" +  + "\n");

            file.flush();
            file.close();
        }

        {
            const QByteArray newName = prepare(author->name());

            bool needAddName = false;

            const QString fileName = pathDir + "/names.txt";
            QFile file(fileName);
            if (file.exists())
            {
                if (!file.open(QFile::OpenModeFlag::ReadOnly | QFile::OpenModeFlag::Text))
                {
                    qWarning() << "Failed to open" << fileName;
                    continue;
                }

                const QByteArray data = file.readAll();
                QByteArray prevName;
                if (data.contains('\n'))
                {
                    prevName = data.mid(data.lastIndexOf('\n') + 1);
                }
                else
                {
                    prevName = data;
                }

                file.close();

                if (prevName != newName)
                {
                    if (!prevName.isEmpty())
                    {
                        qDebug() << "Author" << author->authorId() << "changed name" << prevName << "to" << newName;

                        emit authorNameChanged(*author, prevName, newName);
                    }

                    needAddName = true;
                }
            }
            else
            {
                needAddName = true;
            }

            if (needAddName)
            {
                QFile file(fileName);
                if (!file.open(QFile::OpenModeFlag::Append | QFile::OpenModeFlag::Text))
                {
                    qWarning() << "Failed to save" << fileName;
                    continue;
                }

                if (file.size() > 0)
                {
                    file.write("\n");
                }

                file.write(newName);
                file.flush();
                file.close();
            }
        }
    }
}

void OutputToFile::reinit(bool forceUpdateOutputFolder)
{
    //Messages
    if (_fileMessages)
    {
        _fileMessages->close();
        _fileMessages->deleteLater();
        _fileMessages = nullptr;
    }

    if (_fileMessagesCount)
    {
        _fileMessagesCount->close();
        _fileMessagesCount->deleteLater();
        _fileMessagesCount = nullptr;
    }

    if (forceUpdateOutputFolder || _sessionFolder.isEmpty())
    {
        _sessionFolder = outputDirectory.get() + "/sessions/" + _startupDateTime.toString(DateTimeFileNameFormat);
    }

    if (enabled.get())
    {
        QDir dir;

        dir = QDir(_sessionFolder);
        if (!dir.exists())
        {
            dir.mkpath(_sessionFolder);
        }
    }

    _fileMessages               = new QFile(_sessionFolder + "/" + MessagesFileName,       this);
    _fileMessagesCount          = new QFile(_sessionFolder + "/" + MessagesCountFileName,  this);

    //Current
    if (_iniCurrentInfo)
    {
        _iniCurrentInfo->sync();
        _iniCurrentInfo->deleteLater();
        _iniCurrentInfo = nullptr;
    }

    _iniCurrentInfo = new QSettings(outputDirectory.get() + "/current.ini", QSettings::IniFormat, this);
    _iniCurrentInfo->setIniCodec("UTF-8");

    if (enabled.get())
    {
        _iniCurrentInfo->setValue("software/started", true);
    }

    writeStartupInfo(_sessionFolder);
    writeInfo();
}

void OutputToFile::writeStartupInfo(const QString& messagesFolder)
{
    if (enabled.get())
    {
        _iniCurrentInfo->setValue("software/version",                   QCoreApplication::applicationVersion());

        _iniCurrentInfo->setValue("software/current_messages_folder",   messagesFolder);

        _iniCurrentInfo->setValue("software/startup_time",              timeToString(_startupDateTime));
    }
}

void OutputToFile::writeInfo()
{
    if (!enabled.get())
    {
        return;
    }

    _iniCurrentInfo->setValue("youtube/broadcast_connected", _youTubeInfo.broadcastConnected);
    _iniCurrentInfo->setValue("youtube/broadcast_id", _youTubeInfo.broadcastId);
    _iniCurrentInfo->setValue("youtube/broadcast_user_specified", _youTubeInfo.userSpecified);
    _iniCurrentInfo->setValue("youtube/broadcast_url", _youTubeInfo.broadcastLongUrl.toString());
    _iniCurrentInfo->setValue("youtube/broadcast_chat_url", _youTubeInfo.broadcastChatUrl.toString());
    _iniCurrentInfo->setValue("youtube/broadcast_control_panel_url", _youTubeInfo.controlPanelUrl.toString());
    _iniCurrentInfo->setValue("youtube/viewers_count", _youTubeInfo.viewers);

    _iniCurrentInfo->setValue("twitch/broadcast_connected", _twitchInfo.connected);
    _iniCurrentInfo->setValue("twitch/channel_name", _twitchInfo.channelLogin);
    _iniCurrentInfo->setValue("twitch/user_specified", _twitchInfo.userSpecifiedChannel);
    _iniCurrentInfo->setValue("twitch/channel_url", _twitchInfo.channelUrl.toString());
    _iniCurrentInfo->setValue("twitch/chat_url", _twitchInfo.chatUrl.toString());
    _iniCurrentInfo->setValue("twitch/control_panel_url", _twitchInfo.controlPanelUrl.toString());
    _iniCurrentInfo->setValue("twitch/viewers_count", _twitchInfo.viewers);
}

void OutputToFile::setYouTubeInfo(const AxelChat::YouTubeInfo &youTubeCurrent)
{
    _youTubeInfo = youTubeCurrent;

    reinit(false);
}

void OutputToFile::setTwitchInfo(const AxelChat::TwitchInfo &twitchCurrent)
{
    _twitchInfo = twitchCurrent;

    reinit(false);
}

void OutputToFile::setGoodGameInfo(const AxelChat::GoodGameInfo &goodGameCurrent)
{
    _goodGameInfo = goodGameCurrent;

    reinit(false);
}
