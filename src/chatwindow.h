#pragma once

#include "i18n.h"
#include "clipboardqml.h"
#include "chathandler.h"
#include "githubapi.h"
#include "appsponsormanager.h"
#include "commandseditor.h"
#include "tray.h"
#include <QQuickView>

class ChatWindow : public QQuickView
{
    Q_OBJECT
public:
    static void declareQml();

    explicit ChatWindow(QWindow *parent = nullptr);

signals:

private:
    QSettings settings;

    cweqt::Manager web;
    QNetworkAccessManager network;
    AppSponsorManager appSponsorManager;
    Tray tray;
    I18n i18n;
    ClipboardQml qmlClipboard;
    GitHubApi github;
    ChatHandler chatHandler;
    CommandsEditor commandsEditor;
};
