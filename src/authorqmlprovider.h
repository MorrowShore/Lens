#pragma once

#include "models/messagesmodel.h"
#include "outputtofile.h"
#include <QObject>

class ChatHandler;

class AuthorQMLProvider : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ getName NOTIFY changed)
    Q_PROPERTY(int serviceType READ getServiceType NOTIFY changed)
    Q_PROPERTY(QUrl avatarUrl READ getAvatarUrl NOTIFY changed)
    Q_PROPERTY(int messagesCount READ getMessagesCount NOTIFY changed)

public:
    explicit AuthorQMLProvider(const ChatHandler& chantHandler, const MessagesModel& messagesModel, const OutputToFile& outputToFile, QObject *parent = nullptr);

    static void declareQML()
    {
        qmlRegisterUncreatableType<AuthorQMLProvider> ("AxelChat.AuthorQMLProvider", 1, 0, "AuthorQMLProvider", "Type cannot be created in QML");
    }

    Q_INVOKABLE void setSelectedAuthorId(const QString& authorId);
    QString getName() const;
    int getServiceType() const;
    QUrl getAvatarUrl() const;
    int getMessagesCount() const;

    Q_INVOKABLE bool openAvatar() const;
    Q_INVOKABLE bool openPage() const;
    Q_INVOKABLE bool openFolder() const;

signals:
    void changed();

private:
    const MessagesModel& messagesModel;
    const OutputToFile& outputToFile;
    QString authorId;
};
