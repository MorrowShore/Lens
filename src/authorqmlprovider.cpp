#include "authorqmlprovider.h"
#include "models/chatauthor.h"
#include "youtube.hpp"
#include <QDesktopServices>

namespace
{

static const int YouTubeAvatarSize = 1080;

}

AuthorQMLProvider::AuthorQMLProvider(const ChatMessagesModel& messagesModel_, const OutputToFile& outputToFile_, QObject *parent)
    : QObject{parent}
    , messagesModel(messagesModel_)
    , outputToFile(outputToFile_)
{

}

void AuthorQMLProvider::setSelectedAuthorId(const QString &authorId_)
{
    authorId = authorId_;
    emit changed();
}

QString AuthorQMLProvider::getName() const
{
    const ChatAuthor* author = messagesModel.getAuthor(authorId);
    if (!author)
    {
        return QString();
    }

    return author->name();
}

int AuthorQMLProvider::getServiceType() const
{
    const ChatAuthor* author = messagesModel.getAuthor(authorId);
    if (!author)
    {
        return (int)AbstractChatService::ServiceType::Unknown;
    }

    return (int)author->getServiceType();
}

QUrl AuthorQMLProvider::getAvatarUrl() const
{
    const ChatAuthor* author = messagesModel.getAuthor(authorId);
    if (!author)
    {
        return QUrl();
    }

    return author->avatarUrl();
}

int AuthorQMLProvider::getMessagesCount() const
{
    const ChatAuthor* author = messagesModel.getAuthor(authorId);
    if (!author)
    {
        return -1;
    }

    return author->messagesCount();
}

bool AuthorQMLProvider::openAvatar() const
{
    const ChatAuthor* author = messagesModel.getAuthor(authorId);
    if (!author)
    {
        return false;
    }

    QUrl url = author->avatarUrl();

    if (author->getServiceType() == AbstractChatService::ServiceType::YouTube)
    {
        const QUrl url_ = YouTube::createResizedAvatarUrl(author->avatarUrl(), YouTubeAvatarSize);
        if (url_.isValid())
        {
            url = url_;
        }
    }

    return QDesktopServices::openUrl(url);
}

bool AuthorQMLProvider::openPage() const
{
    const ChatAuthor* author = messagesModel.getAuthor(authorId);
    if (!author)
    {
        return false;
    }

    return QDesktopServices::openUrl(author->pageUrl());
}

bool AuthorQMLProvider::openFolder() const
{
    const ChatAuthor* author = messagesModel.getAuthor(authorId);
    if (!author)
    {
        return false;
    }

    return QDesktopServices::openUrl(QUrl::fromLocalFile(outputToFile.getAuthorDirectory(author->getServiceType(), author->authorId())));
}

