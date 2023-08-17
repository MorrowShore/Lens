#pragma once

#include "i18n.h"
#include "clipboardqml.h"
#include "chathandler.h"
#include "githubapi.h"
#include "appsponsormanager.h"
#include "commandseditor.h"
#include "tray.h"
#include <QMainWindow>

class ChatWindow : public QMainWindow
{
    Q_OBJECT
public:
    static void declareQml();

    explicit ChatWindow(QWidget *parent = nullptr);

signals:

private:
    QSettings settings;

    cweqt::Manager web;
    QNetworkAccessManager network;
    AppSponsorManager appSponsorManager;
    QQmlEngine* qml = nullptr;
    Tray tray;
    I18n i18n;
    ClipboardQml qmlClipboard;
    GitHubApi github;
    ChatHandler chatHandler;
    CommandsEditor commandsEditor;
};
