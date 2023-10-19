#include "authorqmlprovider.h"
#include "models/author.h"
#include "chat_services/youtube/youtubeutils.h"
#include "ChatManager.h"
#include <QDesktopServices>

namespace
{

static const int YouTubeAvatarSize = 1080;

}

AuthorQMLProvider::AuthorQMLProvider(const ChatManager& chantHandler, const MessagesModel& messagesModel_, const OutputToFile& outputToFile_, QObject *parent)
    : QObject{parent}
    , messagesModel(messagesModel_)
    , outputToFile(outputToFile_)
{
    connect(&chantHandler, &ChatManager::messagesDataChanged, this, [this]()
    {
        emit changed();
    });
}

void AuthorQMLProvider::setSelectedAuthorId(const QString &authorId_)
{
    authorId = authorId_;
    emit changed();
}

QString AuthorQMLProvider::getName() const
{
    const std::shared_ptr<Author> author = messagesModel.getAuthor(authorId);
    if (!author)
    {
        return QString();
    }

    return author->getValue(Author::Role::Name).toString();
}

int AuthorQMLProvider::getServiceType() const
{
    const std::shared_ptr<Author> author = messagesModel.getAuthor(authorId);
    if (!author)
    {
        return (int)ChatServiceType::Unknown;
    }

    return (int)author->getServiceType();
}

QUrl AuthorQMLProvider::getAvatarUrl() const
{
    const std::shared_ptr<Author> author = messagesModel.getAuthor(authorId);
    if (!author)
    {
        return QUrl();
    }

    return author->getValue(Author::Role::AvatarUrl).toUrl();
}

int AuthorQMLProvider::getMessagesCount() const
{
    const std::shared_ptr<Author> author = messagesModel.getAuthor(authorId);
    if (!author)
    {
        return -1;
    }

    return author->getMessagesIds().size();
}

bool AuthorQMLProvider::openAvatar() const
{
    const std::shared_ptr<Author> author = messagesModel.getAuthor(authorId);
    if (!author)
    {
        return false;
    }

    QUrl url = author->getValue(Author::Role::AvatarUrl).toUrl();

    if (author->getServiceType() == ChatServiceType::YouTube)
    {
        const QUrl url_ = YouTubeUtils::createResizedAvatarUrl(url, YouTubeAvatarSize);
        if (url_.isValid())
        {
            url = url_;
        }
    }

    return QDesktopServices::openUrl(url);
}

bool AuthorQMLProvider::openPage() const
{
    const std::shared_ptr<Author> author = messagesModel.getAuthor(authorId);
    if (!author)
    {
        return false;
    }

    return QDesktopServices::openUrl(author->getValue(Author::Role::PageUrl).toUrl());
}

bool AuthorQMLProvider::openFolder() const
{
    const std::shared_ptr<Author> author = messagesModel.getAuthor(authorId);
    if (!author)
    {
        return false;
    }

    return QDesktopServices::openUrl(QUrl::fromLocalFile(outputToFile.getAuthorDirectory(author->getServiceType(), author->getId())));
}

