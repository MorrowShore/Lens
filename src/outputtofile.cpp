#include "outputtofile.hpp"
#include "models/chatauthor.h"
#include "models/chatmessage.h"
#include "chat_services/youtube.hpp"
#include "chat_services/twitch.hpp"
#include "chat_services/goodgame.h"
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

QString removePrefix(const QString& string, const QString& postfix, const Qt::CaseSensitivity caseSensitivity)
{
    if (!string.startsWith(postfix, caseSensitivity))
    {
        return string;
    }

    return string.mid(postfix.length());
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

OutputToFile::OutputToFile(QSettings &settings, const QString &settingsGroupPath, QNetworkAccessManager& network_, const ChatMessagesModel& messagesModel_, const QList<ChatService*>& services_, QObject *parent)
    : QObject(parent)
    , network(network_)
    , messagesModel(messagesModel_)
    , services(services_)
    , enabled(settings, settingsGroupPath + "/enabled", false)
    , outputDirectory(settings, settingsGroupPath + "/output_folder", standardOutputFolder())
    , youTubeLastMessageId(settings, settingsGroupPath + "/youtube_last_saved_message_id")
    , codec(settings, settingsGroupPath + "/codec", OutputToFileCodec::UTF8Codec)
{
    reinit(true);
}

OutputToFile::~OutputToFile()
{
    writeApplicationState(false);

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

            if (message.getId() == youTubeLastMessageId.get())
            {
                qDebug() << "found youtube message with id" << message.getId() << ", ignore saving messages before it, index =" << i;
                firstValidMessage = i + 1;
                break;
            }
        }
    }

    QString currentLastYouTubeMessageId;

    for (int i = firstValidMessage; i < messages.count(); ++i)
    {
        const ChatMessage& message = messages[i];

        const QString authorId = message.getAuthorId();
        const ChatAuthor* author = messagesModel.getAuthor(authorId);
        if (!author)
        {
            qWarning() << "Not found author id" << message.getAuthorId();
            continue;
        }

        const ChatService::ServiceType type = author->getServiceType();

        if (type == ChatService::ServiceType::Unknown)
        {
            continue;
        }

        QList<QPair<QString, QString>> tags;

        QString text;

        for (const ChatMessage::Content* content : message.getContents())
        {
            switch (content->getContentType())
            {
            case ChatMessage::Content::Type::Unknown:
                break;

            case ChatMessage::Content::Type::Text:
                text += prepare(static_cast<const ChatMessage::Text*>(content)->getText());
                break;

            case ChatMessage::Content::Type::Image:
                text += "<emoji:" + convertUrlForFileName(static_cast<const ChatMessage::Image*>(content)->getUrl(), ImageFileFormat) + ">";
                break;

            case ChatMessage::Content::Type::Hyperlink:
                text += prepare(static_cast<const ChatMessage::Hyperlink*>(content)->getText());
                break;
            }
        }

        tags.append(QPair<QString, QString>("author", prepare(author->getName())));
        tags.append(QPair<QString, QString>("author_id", authorId));
        tags.append(QPair<QString, QString>("message", text));
        tags.append(QPair<QString, QString>("time", timeToString(message.getPublishedAt())));
        tags.append(QPair<QString, QString>("service", ChatService::getServiceTypeId(type)));

        writeMessage(tags);

        if (author->getServiceType() == ChatService::ServiceType::YouTube)
        {
            const QString id = message.getId();
            if (!id.isEmpty())
            {
                currentLastYouTubeMessageId = id;
            }
        }

        switch (type)
        {
        case ChatService::ServiceType::GoodGame:
        case ChatService::ServiceType::YouTube:
        case ChatService::ServiceType::Twitch:
            downloadAvatar(authorId, author->getAvatarUrl(), type);
            break;

        case ChatService::ServiceType::Unknown:
        case ChatService::ServiceType::Software:
            break;
        }

        for (const ChatMessage::Content* content : message.getContents())
        {
            if (content->getContentType() == ChatMessage::Content::Type::Image)
            {
                static const int ImageHeight = 24;
                const ChatMessage::Image* image = static_cast<const ChatMessage::Image*>(content);
                downloadEmoji(image->getUrl(), ImageHeight, author->getServiceType());
            }
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

void OutputToFile::downloadAvatar(const QString& authorId, const QUrl& url_, const ChatService::ServiceType service)
{
    QUrl url = url_;
    if (service == ChatService::ServiceType::YouTube)
    {
        url = YouTube::createResizedAvatarUrl(url, avatarHeight);
    }

    if (url.isEmpty() || !enabled.get())
    {
        return;
    }

    QString fileName = convertUrlForFileName(url, ImageFileFormat);
    if (fileName.isEmpty())
    {
        return;
    }

    if (service == ChatService::ServiceType::Twitch)
    {
        fileName = removePostfix(fileName, "-300x300", Qt::CaseSensitivity::CaseInsensitive);
    }

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

    const QString fullFileName = avatarsDirectory + "/" + fileName;
    downloadImage(url, fullFileName, ImageFileFormat, avatarHeight, true);

    QFile file(authorDirectory + "/avatar.txt");
    if (file.open(QFile::OpenModeFlag::WriteOnly | QFile::OpenModeFlag::Text))
    {
        file.write(fileName.toUtf8());
        file.close();
    }
}

void OutputToFile::downloadImage(const QUrl &url, const QString &fileName, const QString& imageFormat, const int height, bool ignoreIfExists)
{
    if ((ignoreIfExists && QFile(fileName).exists()) || needIgnoreDownloadFileNames.contains(fileName))
    {
        //qDebug() << "Ignore download image, file" << fileName << "already exists";
        needIgnoreDownloadFileNames.insert(fileName);
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, AxelChat::UserAgentNetworkHeaderName);
    QNetworkReply* reply = network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, fileName, imageFormat, height, ignoreIfExists]()
    {
        if (ignoreIfExists && QFile(fileName).exists())
        {
            //qDebug() << "Ignore save image, file" << fileName << "already exists";
            return;
        }

        QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
        if (!reply)
        {
            return;
        }

        if (reply->bytesAvailable() <= 0)
        {
            qDebug() << "Failed download image, reply is empty, error =" << reply->errorString() << ", file name =" << fileName;
            return;
        }

        QImage rawImage;
        if (rawImage.loadFromData(reply->readAll()))
        {
            QImage image = rawImage.scaledToHeight(height, Qt::TransformationMode::SmoothTransformation);

            const char* format = nullptr;
            if (!imageFormat.isEmpty())
            {
                format = imageFormat.toStdString().c_str();
            }

            if (image.save(fileName, format))
            {
                //qDebug() << "Downloaded image" << fileName;
                if (ignoreIfExists)
                {
                    needIgnoreDownloadFileNames.insert(fileName);
                }
            }
            else
            {
                qWarning() << "Failed to save downloaded image, file name =" << fileName;
            }
        }
        else
        {
            qWarning() << "Failed to read downloaded image, file name" << fileName;
        }

        reply->deleteLater();
    });
}

void OutputToFile::downloadEmoji(const QUrl &url, const int height, const ChatService::ServiceType serviceType)
{
    switch (serviceType)
    {
    case ChatService::ServiceType::Unknown:
    case ChatService::ServiceType::Software:
        return;

    case ChatService::ServiceType::YouTube:
    case ChatService::ServiceType::Twitch:
    case ChatService::ServiceType::GoodGame:
        break;
    }

    const QString emojiDirectory = getServiceDirectory(serviceType) + "/emoji";

    const QDir dir(emojiDirectory);
    if (!dir.exists())
    {
        dir.mkpath(emojiDirectory);
    }

    QString fileName = convertUrlForFileName(url, ImageFileFormat);
    if (fileName.isEmpty())
    {
        return;
    }

    fileName = emojiDirectory + "/" + fileName;

    downloadImage(url, fileName, ImageFileFormat, height, true);
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
        data += tag.first.toLatin1() + "=" + tag.second + "\n";
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

QString OutputToFile::getAuthorDirectory(const ChatService::ServiceType serviceType, const QString &authorId) const
{
    return getServiceDirectory(serviceType) + "/authors/" + authorId;
}

QString OutputToFile::getServiceDirectory(const ChatService::ServiceType serviceType) const
{
    return outputDirectory.get() + "/services/" + ChatService::getServiceTypeId(serviceType);
}

void OutputToFile::writeAuthors(const QList<ChatAuthor*>& authors)
{
    if (!enabled.get())
    {
        return;
    }

    for (const ChatAuthor* author : authors)
    {
        const QString pathDir = getAuthorDirectory(author->getServiceType(), author->getId());
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

            file.write("[info]\n");
            file.write("name=" + prepare(author->getName()) + "\n");
            file.write("id=" + prepare(author->getId()) + "\n");
            file.write("service=" + prepare(ChatService::getServiceTypeId(author->getServiceType())) + "\n");
            file.write("avatar_url=" + prepare(author->getAvatarUrl().toString()) + "\n");
            file.write("page_url=" + prepare(author->getPageUrl().toString()) + "\n");
            //ToDo: file.write("last_message_at=" +  + "\n");

            file.flush();
            file.close();
        }

        {
            const QByteArray newName = prepare(author->getName());

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
                        qDebug() << "Author" << author->getId() << "changed name" << prevName << "to" << newName;

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

QString OutputToFile::convertUrlForFileName(const QUrl &url, const QString& imageFileFormat) const
{
    if (url.isEmpty())
    {
        return QString();
    }

    QString fileName = url.toString();

    fileName = removePrefix(fileName, "https://", Qt::CaseSensitivity::CaseInsensitive);
    fileName = removePrefix(fileName, "http://", Qt::CaseSensitivity::CaseInsensitive);

    if (fileName.contains('/'))
    {
        fileName = fileName.mid(fileName.indexOf('/') + 1);

    }
    else
    {
        qWarning() << "Url not contains '/', url =" << url;
    }

    fileName = fileName.replace('/', '-');
    fileName = fileName.replace('\\', '-');
    fileName = fileName.replace(':', '-');
    fileName = fileName.replace('*', '-');
    fileName = fileName.replace('?', '-');
    fileName = fileName.replace('\"', '-');
    fileName = fileName.replace('<', '-');
    fileName = fileName.replace('>', '-');
    fileName = fileName.replace('|', '-');

    fileName = removePostfix(fileName, ".png", Qt::CaseSensitivity::CaseInsensitive);
    fileName = removePostfix(fileName, ".jpg", Qt::CaseSensitivity::CaseInsensitive);
    fileName = removePostfix(fileName, ".jpeg", Qt::CaseSensitivity::CaseInsensitive);
    fileName = removePostfix(fileName, ".svg", Qt::CaseSensitivity::CaseInsensitive);

    if (!imageFileFormat.isEmpty())
    {
        fileName += "." + imageFileFormat;
    }

    return fileName;
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

    writeApplicationState(true);
}

void OutputToFile::writeApplicationState(const bool started) const
{
    if (!enabled.get())
    {
        return;
    }

    const QString pathDir = outputDirectory.get();

    QDir dir(pathDir);
    if (!dir.exists())
    {
        if (!dir.mkpath(pathDir))
        {
            qCritical() << Q_FUNC_INFO << ": failed to make path" << pathDir;
        }
    }

    QFile file(pathDir + "/state.ini");
    if (!file.open(QFile::OpenModeFlag::WriteOnly | QFile::OpenModeFlag::Text))
    {
        qCritical() << Q_FUNC_INFO << ": failed to save file" << file.fileName();
        return;
    }

    file.write("[application]\n");
    file.write(QString("started=%1\n").arg(started ? "true" : "false").toUtf8());
    file.write(QString("version=%1\n").arg(QCoreApplication::applicationVersion()).toUtf8());
    file.write(QString("session_directory=%1\n").arg(_sessionFolder).toUtf8());

    for (const ChatService* service : services)
    {
        writeServiceState(service);
    }
}

void OutputToFile::writeServiceState(const ChatService* service) const
{
    if (!enabled.get())
    {
        return;
    }

    const QString pathDir = getServiceDirectory(service->getServiceType());

    QDir dir(pathDir);
    if (!dir.exists())
    {
        if (!dir.mkpath(pathDir))
        {
            qCritical() << Q_FUNC_INFO << ": failed to make path" << pathDir;
        }
    }

    QFile file(pathDir + "/state.ini");
    if (!file.open(QFile::OpenModeFlag::WriteOnly | QFile::OpenModeFlag::Text))
    {
        qCritical() << Q_FUNC_INFO << ": failed to save file" << file.fileName();
        return;
    }

    file.write("[service]\n");
    file.write(QString("service_type_id=%1\n").arg(ChatService::getServiceTypeId(service->getServiceType())).toUtf8());
    file.write(QString("connected=%1\n").arg(service->getConnectionStateType() == ChatService::ConnectionStateType::Connected ? "true" : "false").toUtf8());
    file.write(QString("stream_url=%1\n").arg(service->getBroadcastUrl().toString()).toUtf8());
    file.write(QString("chat_url=%1\n").arg(service->getChatUrl().toString()).toUtf8());
    file.write(QString("panel_url=%1\n").arg(service->getControlPanelUrl().toString()).toUtf8());
    file.write(QString("viewers_count=%1\n").arg(service->getViewersCount()).toUtf8());

    switch (service->getServiceType())
    {
    case ChatService::ServiceType::Unknown:
    case ChatService::ServiceType::Software:
        break;

    case ChatService::ServiceType::YouTube:
        file.write(QString("youtube_broadcast_id=%1\n").arg(static_cast<const YouTube*>(service)->getInfo().broadcastId).toUtf8());
        break;

    case ChatService::ServiceType::Twitch:
        file.write(QString("twitch_channel=%1\n").arg(static_cast<const Twitch*>(service)->getInfo().channelLogin).toUtf8());
        break;

    case ChatService::ServiceType::GoodGame:
        break;
    }
}
