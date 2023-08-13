#include "outputtofile.h"
#include "models/author.h"
#include "models/message.h"
#include "chat_services/youtubeutils.h"
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

OutputToFile::OutputToFile(QSettings &settings_, const QString &settingsGroupPath_, QNetworkAccessManager& network_, const MessagesModel& messagesModel_, QList<std::shared_ptr<ChatService>>& services_, QObject *parent)
    : QObject(parent)
    , settings(settings_)
    , settingsGroupPath(settingsGroupPath_)
    , network(network_)
    , messagesModel(messagesModel_)
    , services(services_)
    , enabled(settings, settingsGroupPath + "/enabled", false)
    , outputDirectory(settings, settingsGroupPath + "/output_folder", standardOutputFolder())
{
    codec = (Codec)settings.value(settingsGroupPath + "/codec", (int)codec).toInt();

    reinit(true);
}

OutputToFile::~OutputToFile()
{
    writeApplicationState(false, -1);

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

void OutputToFile::writeMessages(const QList<std::shared_ptr<Message>>& messages, const AxelChat::ServiceType serviceType)
{
    if (!enabled.get())
    {
        return;
    }

    std::shared_ptr<ChatService> service;
    for (const std::shared_ptr<ChatService>& service_ : qAsConst(services))
    {
        if (!service_)
        {
            qWarning() << Q_FUNC_INFO << "service is null";
            continue;
        }

        if (service_->getServiceType() == serviceType)
        {
            service = service_;
            break;
        }
    }

    int firstValidMessage = 0;

    if (service)
    {
        if (!service->getLastSavedMessageId().get().isEmpty())
        {
            for (int i = 0; i < messages.count(); ++i)
            {
                const std::shared_ptr<Message>& message = messages[i];
                if (!message)
                {
                    qWarning() << Q_FUNC_INFO << "message is null";
                    continue;
                }

                if (message->getId() == service->getLastSavedMessageId().get())
                {
                    qDebug() << "found message with id" << message->getId() << ", ignore saving messages before it, index =" << i;
                    firstValidMessage = i + 1;
                    break;
                }
            }
        }
    }

    QString currentLastMessageId;

    for (int i = firstValidMessage; i < messages.count(); ++i)
    {
        const std::shared_ptr<Message>& message_ = messages[i];
        if (!message_)
        {
            qWarning() << Q_FUNC_INFO << "message is null";
            continue;
        }

        const Message& message = *message_;

        if (message.isHasFlag(Message::Flag::DeleterItem))
        {
            continue;
        }

        const QString authorId = message.getAuthorId();
        const std::shared_ptr<Author> author = messagesModel.getAuthor(authorId);
        if (!author)
        {
            qWarning() << "Not found author id" << message.getAuthorId();
            continue;
        }

        const AxelChat::ServiceType type = author->getServiceType();

        if (type == AxelChat::ServiceType::Unknown)
        {
            continue;
        }

        QList<QPair<QString, QString>> tags;

        QString text;

        for (const std::shared_ptr<Message::Content>& content : message.getContents())
        {
            if (!content)
            {
                qWarning() << Q_FUNC_INFO << "content is null";
                continue;
            }

            switch (content->getType())
            {
            case Message::Content::Type::Text:
                text += prepare(static_cast<const Message::Text*>(content.get())->getText());
                break;

            case Message::Content::Type::Image:
                text += "<emoji:" + convertUrlForFileName(static_cast<const Message::Image*>(content.get())->getUrl(), ImageFileFormat) + ">";
                break;

            case Message::Content::Type::Hyperlink:
                text += prepare(static_cast<const Message::Hyperlink*>(content.get())->getText());
                break;
            }
        }

        tags.append(QPair<QString, QString>("author", prepare(author->getValue(Author::Role::Name).toString())));
        tags.append(QPair<QString, QString>("author_id", authorId));
        tags.append(QPair<QString, QString>("message", text));
        tags.append(QPair<QString, QString>("time", timeToString(message.getPublishedAt())));
        tags.append(QPair<QString, QString>("service", ChatService::getServiceTypeId(type)));

        writeMessage(tags);

        const QString id = message.getId();
        if (!id.isEmpty())
        {
            currentLastMessageId = id;
        }

        if (type != AxelChat::ServiceType::Unknown && type != AxelChat::ServiceType::Software)
        {
            downloadAvatar(authorId, type, author->getValue(Author::Role::AvatarUrl).toUrl());
        }

        for (const std::shared_ptr<Message::Content>& content : message.getContents())
        {
            if (!content)
            {
                qWarning() << Q_FUNC_INFO << "content is null";
                continue;
            }

            if (content->getType() == Message::Content::Type::Image)
            {
                static const int ImageHeight = 24;
                const Message::Image* image = static_cast<const Message::Image*>(content.get());
                downloadEmoji(image->getUrl(), ImageHeight, author->getServiceType());
            }
        }
    }

    if (service)
    {
        if (!currentLastMessageId.isEmpty())
        {
            service->setLastSavedMessageId(currentLastMessageId);
        }
    }
}

void OutputToFile::showInExplorer()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString("file:///") + outputDirectory.get()));
}

void OutputToFile::downloadAvatar(const QString& authorId, const AxelChat::ServiceType serviceType, const QUrl &url_)
{
    QUrl url = url_;
    if (serviceType == AxelChat::ServiceType::YouTube)
    {
        url = YouTubeUtils::createResizedAvatarUrl(url, avatarHeight);
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

    if (serviceType == AxelChat::ServiceType::Twitch)
    {
        fileName = removePostfix(fileName, "-300x300", Qt::CaseSensitivity::CaseInsensitive);
    }

    const QString authorDirectory = getAuthorDirectory(serviceType, authorId);

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
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, fileName, imageFormat, height, ignoreIfExists, url]()
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
            qWarning() << Q_FUNC_INFO << ": failed download image, reply is empty, error =" << reply->errorString() << ", file name =" << fileName << ", url =" << url;
            return;
        }

        const QByteArray data = reply->readAll();

        QImage rawImage;
        if (rawImage.loadFromData(data))
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
                qWarning() << Q_FUNC_INFO << ": failed to save downloaded image" << url;
            }
        }
        else
        {
            qWarning() << Q_FUNC_INFO << ": failed to read downloaded image" << url;
        }

        reply->deleteLater();
    });
}

void OutputToFile::downloadEmoji(const QUrl &url, const int height, const AxelChat::ServiceType serviceType)
{
    if (serviceType == AxelChat::ServiceType::Unknown || serviceType == AxelChat::ServiceType::Software)
    {
        return;
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

bool OutputToFile::setCodecOption(int option)
{
    if (option == (int)codec)
    {
        return false;
    }

    switch (option) {
    case 0:
    case 100:
    case 200:
        settings.setValue(settingsGroupPath + "/codec", option);
        break;

    default:
        qCritical() << "unknown codec option" << option << ", ignore";
        return false;
    }

    return true;
}

int OutputToFile::codecOption() const
{
    return (int)codec;
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
        data += tag.first.toLatin1() + "=" + tag.second.toUtf8() + "\n"; // TODO: fix
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

    if (codec != Codec::ANSIWithUTF8Codes)
    {
        text.replace('\n', "\\n");
        text.replace('\r', "\\r");
    }

    if (codec == Codec::UTF8)
    {
        return text.toUtf8();
    }
    else if (codec == Codec::ANSI)
    {
        return text.toLocal8Bit();
    }
    else if (codec == Codec::ANSIWithUTF8Codes)
    {
        return AxelChat::convertANSIWithUtf8Numbers(text);
    }

    qWarning() << Q_FUNC_INFO << "unknown codec";

    return text.toUtf8();
}

QString OutputToFile::getAuthorDirectory(const AxelChat::ServiceType serviceType, const QString &authorId) const
{
    return getServiceDirectory(serviceType) + "/authors/" + authorId;
}

QString OutputToFile::getServiceDirectory(const AxelChat::ServiceType serviceType) const
{
    return outputDirectory.get() + "/services/" + ChatService::getServiceTypeId(serviceType);
}

void OutputToFile::writeAuthors(const QList<std::shared_ptr<Author>>& authors)
{
    if (!enabled.get())
    {
        return;
    }

    for (const std::shared_ptr<Author>& author : qAsConst(authors))
    {
        if (!author)
        {
            qWarning() << Q_FUNC_INFO << "author is null";
            continue;
        }

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
            file.write("name=" + prepare(author->getValue(Author::Role::Name).toString()) + "\n");
            file.write("id=" + prepare(author->getId()) + "\n");
            file.write("service=" + prepare(ChatService::getServiceTypeId(author->getServiceType())) + "\n");
            file.write("avatar_url=" + prepare(author->getValue(Author::Role::AvatarUrl).toUrl().toString()) + "\n");
            file.write("page_url=" + prepare(author->getValue(Author::Role::PageUrl).toUrl().toString()) + "\n");
            //ToDo: file.write("last_message_at=" +  + "\n");

            file.flush();
            file.close();
        }

        {
            const QByteArray newName = prepare(author->getValue(Author::Role::Name).toString());

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

    if (forceUpdateOutputFolder || _relativeSessionFolder.isEmpty())
    {
        _relativeSessionFolder = "sessions/" + _startupDateTime.toString(DateTimeFileNameFormat);
    }

    const QString absoluteSessionFolder = outputDirectory.get() + "/" + _relativeSessionFolder;

    if (enabled.get())
    {
        QDir dir;

        dir = QDir(absoluteSessionFolder);
        if (!dir.exists())
        {
            dir.mkpath(absoluteSessionFolder);
        }
    }

    _fileMessages               = new QFile(absoluteSessionFolder + "/" + MessagesFileName,       this);
    _fileMessagesCount          = new QFile(absoluteSessionFolder + "/" + MessagesCountFileName,  this);

    writeApplicationState(true, -1);
}

void OutputToFile::writeApplicationState(const bool started, const int viewersTotalCount) const
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
    file.write(QString("session_directory=%1\n").arg(_relativeSessionFolder).toUtf8());
    file.write(QString("viewers_count_total=%1\n").arg(viewersTotalCount).toUtf8());

    for (const std::shared_ptr<ChatService>& service : qAsConst(services))
    {
        writeServiceState(service.get());
    }
}

void OutputToFile::writeServiceState(const ChatService* service) const
{
    if (!enabled.get())
    {
        return;
    }

    if (!service)
    {
        qWarning() << Q_FUNC_INFO << "service is null";
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
    file.write(QString("stream_id=%1\n").arg(service->getState().streamId).toUtf8());
    file.write(QString("enabled=%1\n").arg(service->isEnabled() ? "true" : "false").toUtf8());
    file.write(QString("connected=%1\n").arg(service->getConnectionStateType() == ChatService::ConnectionStateType::Connected ? "true" : "false").toUtf8());
    file.write(QString("stream_url=%1\n").arg(service->getStreamUrl().toString()).toUtf8());
    file.write(QString("chat_url=%1\n").arg(service->getChatUrl().toString()).toUtf8());
    file.write(QString("panel_url=%1\n").arg(service->getControlPanelUrl().toString()).toUtf8());
    file.write(QString("viewers_count=%1\n").arg(service->getViewersCount()).toUtf8());
}

